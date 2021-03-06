#include "savior.hpp"

forest::details::Savior::Savior()
{
	items_queue.resize(SAVIOUR_QUEUE_LENGTH);
	items_queue.set_callback([this](save_key item){
		save(item, false);
	});
}

forest::details::Savior::~Savior()
{
	// Save everything in sync mode
	save_all();
	
	// Wait workers to finish current work
	joiner.wait();
	file_deleter.wait();
	
	// Just to make sure there is no any threads anymore
	wait_for_threads();
}

void forest::details::Savior::put(save_key item, SAVE_TYPES type, void_shared node)
{
	std::unique_lock<std::mutex> lock(map_mtx);
	
	if(has(item) && !has_locking(item)){
		// Just schedule as its going to be saved
		schedule_save(item);
		return;
	}
	
	define_item(item, type, ACTION_TYPE::SAVE, node);
	schedule_save(item);
}

void forest::details::Savior::remove(save_key item, SAVE_TYPES type, void_shared node)
{
	std::unique_lock<std::mutex> lock(map_mtx);

	define_item(item, type, ACTION_TYPE::REMOVE, node);
	schedule_save(item);
}

void forest::details::Savior::leave(save_key item, SAVE_TYPES type, void_shared nodef)
{
	std::unique_lock<std::mutex> lock(map_mtx);
	
	// Implicitly close file if leaf is up to date
	if(type == SAVE_TYPES::LEAF){
		node_ptr node = std::static_pointer_cast<tree_t::Node>(nodef);
		get_data(node).leaved = true;
		if(!map.count(item)){
			auto f = get_data(node).f;
			if(f){
				f->close();
				get_data(node).f = nullptr;
			}
		}
	}
	
	if(has(item)){
		save(item, false);
	}
}

void forest::details::Savior::save(save_key item, bool sync)
{
	if(sync){
		save_item(item);
	} else {
		join_mtx.lock();
		thrds.push(std::thread([this](string item){
			save_item(item);
		}, item));
		join_mtx.unlock();
		joiner.work([this]{ wait_for_threads(); });
	}
}

void forest::details::Savior::get(save_key item)
{
	std::unique_lock lock(map_mtx);
	while(map.count(item)){
		cv.wait(lock);
	}
}

int forest::details::Savior::save_queue_size()
{
	std::unique_lock lock(map_mtx);
	return items_queue.size();
}

void forest::details::Savior::save_all()
{
	while(true){
		std::unique_lock<std::mutex> lock(map_mtx);
		if(map.size() == 0){
			while(items_queue.size()){
				cv.wait(lock);
			}
			return;
		}
		save_key item = (map.begin())->first;
		lock.unlock();
		save(item, true);
	}
}

bool forest::details::Savior::has(save_key& item)
{
	return map.count(item);
}

bool forest::details::Savior::has_locking(save_key& item)
{
	return locking_items.count(item);
}

void forest::details::Savior::lock_map()
{
	map_mtx.lock();
}

void forest::details::Savior::unlock_map()
{
	map_mtx.unlock();
}

void forest::details::Savior::save_item(save_key item)
{
	std::unique_lock<std::mutex> lock(map_mtx);
	
	// Wait if it already saving
	while(saving_items.count(item)){
		cv.wait(lock);
	}
	
	// Return if it's already up to date
	if(!has(item)){
		cv.notify_all();
		return;
	}
	
	save_value* it = get_item(item);
	
	// Mark item for saving
	saving_items.insert(item);
	lock.unlock();
	
	if(it->type == SAVE_TYPES::INTR){
		node_ptr node = std::static_pointer_cast<tree_t::Node>(it->node);
		
		// To avoid any deadlocks and RC, lock the node
		forest::details::lock_write(node);
		
		it = lock_item(item);
		
		if(it->action == ACTION_TYPE::SAVE){
			DBFS::File* f = DBFS::create();
			forest::details::Tree::save_intr(node, f);
			string new_name = f->name();
			delete f;
			
			node_data_ptr data = get_node_data(node);
			string cur_name = data->path;
			
			DBFS::remove(cur_name);
			DBFS::move(new_name, cur_name);
		} else { // REMOVE
			node_data_ptr data = get_node_data(node);
			remove_file_async(data->path);
		}
		
		forest::details::unlock_write(node);
	} else if(it->type == SAVE_TYPES::LEAF){
		node_ptr node = std::static_pointer_cast<tree_t::Node>(it->node);
		
		// To avoid any deadlocks and RC, lock the node
		change_lock_write(node);
		
		it = lock_item(item);
		
		if(it->action == ACTION_TYPE::SAVE){
			node_data_ptr data = get_node_data(node);
			string cur_name = data->path;
		
			file_ptr cur_f = get_data(node).f;
			if(cur_f){
				// Update count of opened files to not exceed the limit
				forest::details::opened_files_inc();
				auto locked = cur_f->get_lock();
				cur_f->move(DBFS::random_filename());
				
				// Other could still reference this leaf, so delete file
				// when no references left
				lazy_delete_file(cur_f);
			}
			
			file_ptr fp = file_ptr(new DBFS::File(cur_name));
			get_data(node).f = fp;
			forest::details::Tree::save_leaf(node, fp);
		} else { // REMOVE
			file_ptr cur_f = get_data(node).f;
			if(cur_f){
				opened_files_inc();
				
				// Same as for saving
				lazy_delete_file(cur_f);
			}
			get_data(node).f = nullptr;
		}
		
		change_unlock_write(node);
	} else {
		tree_ptr tree = std::static_pointer_cast<Tree>(it->node);
		
		// To avoid any deadlocks and RC, lock the node
		tree->get_tree()->lock_write();
		
		it = lock_item(item);
		
		if(it->action == ACTION_TYPE::SAVE){
			DBFS::File* base_f = DBFS::create();
			forest::details::Tree::save_base(tree, base_f);
			
			string base_file_name = tree->get_name();
			string new_base_file_name = base_f->name();
			
			delete base_f;
			DBFS::remove(base_file_name);
			DBFS::move(new_base_file_name, base_file_name);
		} else { // REMOVE
			remove_file_async(item);
		}
		tree->get_tree()->unlock_write();
	}
	
	
	lock.lock();
	
	// Remove item
	saving_items.erase(item);
	locking_items.erase(item);
	
	SAVE_TYPES node_type = it->type;
	auto void_node = it->node;
	
	// Remove item from map
	pop_item(item);

	// Close item if needed
	if(!has(item)){
		items_queue.remove(item);
		
		// Close file implicitly if leaf already left
		if(node_type == SAVE_TYPES::LEAF){
			node_ptr node = std::static_pointer_cast<tree_t::Node>(void_node);
			auto& data = get_data(node);
			if(data.leaved && data.f){
				data.f->close();
				data.f = nullptr;
			}
		}
	}
	
	// Notify for changes
	cv.notify_all();
}

void forest::details::Savior::run_scheduler()
{
	if(scheduler_running){
		return;
	}
	
	scheduler_running = true;

	scheduler_worker.work([this]{
		delayed_save();
	});
}

void forest::details::Savior::delayed_save()
{
	while(true){
		std::this_thread::sleep_for(std::chrono::microseconds(SCHEDULE_TIMER));
		
		lock_map();
		if(!items_queue.size()){
			scheduler_running = false;
			unlock_map();
			return;
		}
		///{
		save_key item = items_queue.back().first;
		items_queue.pop();
		///}
		save(item, false);
		unlock_map();
	}
}

void forest::details::Savior::schedule_save(save_key& item)
{
	items_queue.push(item, true);
	run_scheduler();
}

forest::details::Savior::save_value* forest::details::Savior::define_item(save_key item, SAVE_TYPES type, ACTION_TYPE action, void_shared node)
{
	
	save_value* val = new save_value();
	map[item].push(val);
	
	val->action = action;
	val->type = type;
	val->node = node;
	
	return val;
}

void forest::details::Savior::remove_file_async(string name)
{
	file_deleter.work([name]{
		DBFS::remove(name);
	});
}

forest::details::Savior::save_value* forest::details::Savior::get_item(save_key& item)
{
	auto& map_ref = map[item];
	while(map_ref.size() > 1){
		delete map_ref.front();
		map_ref.pop();
	}
	return map_ref.front();
}

forest::details::Savior::save_value* forest::details::Savior::lock_item(save_key& item)
{
	std::lock_guard<std::mutex> lock(map_mtx);
	locking_items.insert(item);
	return get_item(item);
}

void forest::details::Savior::pop_item(save_key& item)
{
	auto& map_ref = map[item];
	delete map_ref.front();
	map_ref.pop();
	if(map_ref.empty()){
		map.erase(item);
	}
}

void forest::details::Savior::lazy_delete_file(file_ptr f)
{
	f->on_close([this](DBFS::File* file){
		// preserve limit
		forest::details::opened_files_dec();
		remove_file_async(file->name());
	});
}

void forest::details::Savior::wait_for_threads()
{
	while(true){
		join_mtx.lock();
		if(thrds.empty()){
			join_mtx.unlock();
			return;
		}
		auto t = std::move(thrds.front());
		thrds.pop();
		join_mtx.unlock();
		t.join();
	}
}

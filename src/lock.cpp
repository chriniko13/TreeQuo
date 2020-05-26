#include "lock.hpp"

void forest::lock_read(tree_t::node_ptr node)
{
	lock_read(node.get());
}

void forest::lock_read(tree_t::Node* node)
{
	auto& tl = node->data.travel_locks;
	
	tl.m.lock();
	if(tl.c++ == 0){
		tl.g.lock();
	}
	tl.m.unlock();
}

void forest::unlock_read(tree_t::node_ptr node)
{
	unlock_read(node.get());
}

void forest::unlock_read(tree_t::Node* node)
{
	auto& tl = node->data.travel_locks;
	
	tl.m.lock();
	if(--tl.c == 0){
		tl.g.unlock();
	}
	tl.m.unlock();
}

void forest::lock_write(tree_t::node_ptr node)
{
	lock_write(node.get());
}

void forest::lock_write(tree_t::Node* node)
{
	auto& tl = node->data.travel_locks;
	
	tl.g.lock();
	tl.wlock = true;
}

void forest::unlock_write(tree_t::node_ptr node)
{
	unlock_write(node.get());
}

void forest::unlock_write(tree_t::Node* node)
{
	auto& tl = node->data.travel_locks;
	
	tl.wlock = false;
	tl.g.unlock();
}

void forest::lock_type(tree_t::node_ptr node, tree_t::PROCESS_TYPE type)
{
	lock_type(node.get(), type);
}

void forest::lock_type(tree_t::Node* node, tree_t::PROCESS_TYPE type)
{
	(type == tree_t::PROCESS_TYPE::WRITE) ? lock_write(node) : lock_read(node);
}

void forest::unlock_type(tree_t::node_ptr node, tree_t::PROCESS_TYPE type)
{
	unlock_type(node.get(), type);
}

void forest::unlock_type(tree_t::Node* node, tree_t::PROCESS_TYPE type)
{
	(type == tree_t::PROCESS_TYPE::WRITE) ? unlock_write(node) : unlock_read(node);
}

bool forest::is_write_locked(tree_t::node_ptr node)
{
	return node->data.travel_locks.wlock;
}


void forest::lock_read(tree_t::child_item_type_ptr item)
{
	auto it = item->item->second;
	it->g.lock();
	if(it->c++ == 0){
		it->m.lock();
	}
	it->g.unlock();
}

void forest::unlock_read(tree_t::child_item_type_ptr item)
{
	auto it = item->item->second;
	it->g.lock();
	if(--it->c == 0){
		it->m.unlock();
	}
	it->g.unlock();
}

void forest::lock_write(tree_t::child_item_type_ptr item)
{
	auto it = item->item->second;
	it->m.lock();
}

void forest::unlock_write(tree_t::child_item_type_ptr item)
{
	auto it = item->item->second;
	it->m.unlock();
}

void forest::lock_type(tree_t::child_item_type_ptr item, tree_t::PROCESS_TYPE type)
{
	(type == tree_t::PROCESS_TYPE::WRITE) ? lock_write(item) : lock_read(item);
}

void forest::unlock_type(tree_t::child_item_type_ptr item, tree_t::PROCESS_TYPE type)
{
	(type == tree_t::PROCESS_TYPE::WRITE) ? unlock_write(item) : unlock_read(item);
}


void forest::change_lock_bunch(tree_t::node_ptr node, tree_t::node_ptr c_node, bool w_prior)
{
	// quick-access
	auto& ch_node = node->data.change_locks;
	auto& ch_shift_node = c_node->data.change_locks;
	
	if(w_prior){
		// Priority lock
		ch_node.p.lock();
		ch_shift_node.p.lock();
		/// prior_lock{
		
		// Assign flag
		ch_node.shared_lock = true;
		ch_shift_node.shared_lock = true;
		
		/// }prior_lock
		ch_node.p.unlock();
		ch_shift_node.p.unlock();
	}
	
	// Lock
	std::lock(ch_node.m, ch_shift_node.m);
	
	if(w_prior){
		// Priority lock
		ch_node.p.lock();
		ch_shift_node.p.lock();
		/// prior_lock{
		
		// Cleanup
		ch_node.shared_lock = false;
		ch_shift_node.shared_lock = false;
		
		// Notify threads
		ch_node.cond.notify_all();
		ch_shift_node.cond.notify_all();
		
		/// }prior_lock
		ch_node.p.unlock();
		ch_shift_node.p.unlock();
	}
}

void forest::change_lock_bunch(tree_t::node_ptr node, tree_t::node_ptr m_node, tree_t::node_ptr c_node, bool w_prior)
{
	// quick-access
	auto& ch_node = node->data.change_locks;
	auto& ch_new_node = m_node->data.change_locks;
	auto& ch_link_node = c_node->data.change_locks; 
	
	if(w_prior){
		// Priority lock
		ch_node.p.lock();
		ch_new_node.p.lock();
		ch_link_node.p.lock();
		/// prior_lock{
		
		// assing flag
		ch_node.shared_lock = true;
		ch_new_node.shared_lock = true;
		ch_link_node.shared_lock = true;
		
		/// }prior_lock
		ch_node.p.unlock();
		ch_new_node.p.unlock();
		ch_link_node.p.unlock();
	}
	
	// Lock nodes
	std::lock(ch_node.m, ch_new_node.m, ch_link_node.m);
	
	if(w_prior){
		// Priority lock
		ch_node.p.lock();
		ch_new_node.p.lock();
		ch_link_node.p.lock();
		/// prior_lock{
		
		// Cleanup
		ch_node.shared_lock = false;
		ch_new_node.shared_lock = false;
		ch_link_node.shared_lock = false;
		
		// Notify threads
		ch_node.cond.notify_all();
		ch_new_node.cond.notify_all();
		ch_link_node.cond.notify_all();
		
		/// }prior_lock
		ch_node.p.unlock();
		ch_new_node.p.unlock();
		ch_link_node.p.unlock();
	}
}

void forest::change_lock_bunch(tree_t::node_ptr node, tree_t::child_item_type_ptr item, bool w_prior)
{	
	// Quick-access
	auto& ch_node = node->data.change_locks;
	
	// Set the flags
	if(w_prior){
		ch_node.p.lock();
		/// prior_lock{
		ch_node.shared_lock = true;
		item->item->second->shared_lock = true;
		/// }prior_lock
		ch_node.p.unlock();
	}
	
	// Lock
	std::lock(item->item->second->m, ch_node.m);
	
	// Notify
	if(w_prior){
		ch_node.p.lock();
		/// prior_lock{
		ch_node.shared_lock = false;
		item->item->second->shared_lock = false;
		ch_node.cond.notify_all();
		/// }prior_lock
		ch_node.p.unlock();
	}
}


void forest::own_lock(tree_t::node_ptr node)
{
	node->data.owner_locks.m.lock();
}

void forest::own_unlock(tree_t::node_ptr node)
{
	node->data.owner_locks.m.unlock();
}

int forest::own_inc(tree_t::node_ptr node)
{
	return node->data.owner_locks.c++;
}

int forest::own_dec(tree_t::node_ptr node)
{
	assert(node->data.owner_locks.c > 0);
	return --node->data.owner_locks.c;
}

void forest::change_lock_read(tree_t::node_ptr node)
{
	change_lock_read(node.get());
}

void forest::change_lock_read(tree_t::Node* node)
{
	auto& ch_node = node->data.change_locks;
	ch_node.g.lock();
	if(ch_node.c++ == 0){
		ch_node.m.lock();
	}
	ch_node.g.unlock();
}

void forest::change_unlock_read(tree_t::node_ptr node)
{
	change_unlock_read(node.get());
}

void forest::change_unlock_read(tree_t::Node* node)
{
	auto& ch_node = node->data.change_locks;
	ch_node.g.lock();
	if(--ch_node.c == 0){
		ch_node.m.unlock();
	}
	ch_node.g.unlock();
}

void forest::change_lock_write(tree_t::node_ptr node)
{
	change_lock_write(node.get());
}

void forest::change_lock_write(tree_t::Node* node)
{
	node->data.change_locks.m.lock();
}

void forest::change_unlock_write(tree_t::node_ptr node)
{
	change_unlock_write(node.get());
}

void forest::change_unlock_write(tree_t::Node* node)
{
	node->data.change_locks.m.unlock();
}

void forest::change_lock_type(tree_t::node_ptr node, tree_t::PROCESS_TYPE type)
{
	(type == tree_t::PROCESS_TYPE::WRITE) ? change_lock_write(node) : change_lock_read(node);
}

void forest::change_unlock_type(tree_t::node_ptr node, tree_t::PROCESS_TYPE type)
{
	(type == tree_t::PROCESS_TYPE::WRITE) ? change_unlock_write(node) : change_unlock_read(node);
}
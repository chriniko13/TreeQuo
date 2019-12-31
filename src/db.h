#ifndef DB_H
#define DB_H

#include <iostream>

#include <string>
#include <exception>
#include <unordered_map>
#include <utility>
#include <memory>
#include <mutex>
#include <future>
#include <vector>
#include <type_traits>
#include <atomic>
#include "bplustree.hpp"
#include "dbfs.hpp"
#include "listcache.hpp"
#include "dbexception.hpp"
#include "dbutils.hpp"
#include "dbdriver.hpp"

class DB{
	
	enum class KEY_TYPES { INT, STRING };
	enum class NODE_TYPES { INTR, LEAF };
	
	using string = std::string;
	using int_t = long long int;
	using file_pos_t = long long int;
	using mutex = std::mutex;
	using void_shared = std::shared_ptr<void>;
	
	using driver_root_t = DBDriver<string, string>;
	using driver_int_t = DBDriver<int_t, file_data_t>;
	using driver_string_t = DBDriver<string, file_data_t>;
	
	using root_tree_t = BPlusTree<string, string, driver_root_t>;
	using int_tree_t = BPlusTree<int_t, file_data_t, driver_int_t>;
	using string_tree_t = BPlusTree<string, file_data_t, driver_string_t>;
	
	using int_a = std::atomic<int>;
	
	struct tree_t {
		void_shared tree;
		TREE_TYPES type;
	};
	struct leaf_t {
		TREE_TYPES type;
		void* child_keys;
		void* child_lengths;
		int_t start_data;
		DBFS::File* file;
		string left_leaf, right_leaf;
	};
	struct intr_t {
		TREE_TYPES type;
		NODE_TYPES childs_type;
		void* child_keys;
		void* child_values;
	};
	struct tree_base_read_t {
		int_t count;
		int factor;
		TREE_TYPES type;
		string branch;
		bool is_leaf;
	};
	struct tree_intr_read_t {
		
	};
	struct tree_leaf_read_t {
		
	};
	struct node_data_t {
		bool ghost = true;
		string path = "";
		TREE_TYPES type;
		node_data_t(bool ghost, string path, TREE_TYPES type) : ghost(ghost), path(path), type(type) {};
	};
	
	using node_data_ptr = std::shared_ptr<node_data_t>;

	public:
		DB();
		~DB();
		DB(string path);
		
		void create_qtree(TREE_TYPES type, string name);
		void delete_qtree(string name);
		tree_t find_qtree(string name);
		tree_t open_qtree(string path);
		void close_qtree(string path);
		
		void insert_qleaf(string name);
		void insert_qleaf(tree_t tree);
		void erase_qleaf(string name);
		void erase_qleaf(tree_t tree);
		
		//find_qleaf();
				
		void bloom(string path);
		void fold(bool cut);
		
	private:
		root_tree_t* FOREST;
		bool blossomed = false;
		
		// Root methods
		root_tree_t* open_root();
		
		// Intr methods
		template<typename T>
		intr_t read_intr<T>(string filename);
		template<typename T>
		void materialize_intr(typename T::node_ptr& node, string path);
		void check_intr_ref(string key);
		
		// Leaf methods
		template<typename T>
		leaf_t read_leaf<T>(string filename);
		template<typename T>
		void materialize_leaf(typename T::node_ptr& node, string path);
		void check_leaf_ref(string key);
		
		// Tree base methods
		tree_base_read_t read_base(string filename);
		void create_root_file();
		string create_qtree_base(TREE_TYPES type);
		void insert_qtree(string name, string file_name, TREE_TYPES type);
		void erase_qtree(string name);
		void check_tree_ref(string key);
		
		// Create Tree Node smart ptr
		template<typename T>
		typename T::node_ptr create_node(string path, bool is_leaf);
		
		// Node data
		node_data_ptr create_node_data(bool ghost, string path);
		
		// Cache
		ListCache<string, tree_t> tree_cache;
		ListCache<string, void_shared> leaf_cache;
		ListCache<string, void_shared> intr_cache;
		mutex tree_cache_m, leaf_cache_m, intr_cache_m;
		std::unordered_map<string, std::shared_future<tree_t> > tree_cache_q;
		std::unordered_map<string, std::shared_future<void_shared> > intr_cache_q, leaf_cache_q;
		std::unordered_map<string, std::pair<tree_t, int_a> > tree_cache_r;
		std::unordered_map<string, std::pair<void_shared, int_a> > intr_cache_r, leaf_cache_r;
		std::unordered_map<int_t, string> tree_cache_f;
		tree_cache.set_callback([this](string key){ this->check_tree_ref(); });
		leaf_cache.set_callback([this](string key){ this->check_leaf_ref(); });
		intr_cache.set_callback([this](string key){ this->check_intr_ref(); });
		
		// Drivers
		void init_drivers();
		driver_root_t* driver_root;
		driver_int_t* driver_int;
		driver_string_t* driver_string;
		
		// Proceed
		template<typename T>
		void d_enter(typename T::node_ptr& node, T* tree);
		template<typename T>
		void d_leave(typename T::node_ptr& node, T* tree);
		template<typename T>
		void d_insert(typename T::node_ptr& node, T* tree);
		template<typename T>
		void d_remove(typename T::node_ptr& node, T* tree);
		template<typename T>
		void d_reserve(typename T::node_ptr& node, T* tree);
		template<typename T>
		void d_release(typename T::node_ptr& node, T* tree);
		template<typename T>
		void d_before_move(typename T::iterator& it, typename T::node_ptr& node, int_t step, T* tree);
		template<typename T>
		void d_after_move(typename T::iterator& it, typename T::node_ptr& node, int_t step, T* tree);
		
		// Getters
		template<typename T>
		typename T::node_ptr get_intr(string path);
		template<typename T>
		typename T::node_ptr get_leaf(string path);
		tree_t get_tree(string path);
		
		// Other
		template<typename T>
		TREE_TYPES get_tree_type<T>();
	
		// Properties
		const int DEFAULT_FACTOR = 100;
		const string ROOT_TREE = "_root";
		const string LEAF_NULL = "-";
};

template<typename T>
typename T::node_ptr DB::create_node(string path, bool is_leaf)
{
	typename T::Node* node;
	if(!is_leaf){
		node = new typename T::InternalNode();
	} else {
		node = new typename T::LeafNode();
	}
	node->data = create_node_data(true, path);
	return typename T::node_ptr(node);
}

template<typename T>
void DB::d_enter(typename T::node_ptr& node, T* tree)
{	
	std::cout << "ENTER" << std::endl;
		
	// Lock node mutex
	node->lock();
	
	// Check for newly created node
	if(!node->data)
		return;
		
	// Check for node already materialized
	node_data_ptr data = static_pointer_cast<node_data_t>(node->data);
	if(!data->ghost)
		return;
		
	if(!tree->is_leaf(node)){
		materialize_intr<T>(node, data->path);
		//TODO: Lock shared node
	}
	else{
		materialize_leaf<T>(node, data->path);
		//TODO: Lock shared node
	}
	
	data->ghost = false;
}

template<typename T>
void DB::d_leave(typename T::node_ptr& node, T* tree)
{
	std::cout << "LEAVE" << std::endl;
	
	node_data_ptr data = static_pointer_cast<node_data_t>(node->data);
	if(!tree->is_leaf(node)){
		unmaterialize_intr<T>(node);
		//TODO: Unlock shared node
	}
	else{
		unmaterialize_leaf<T>(node);
		//TODO: Unlock shared node
	}
	data->ghost = true;
	node->unlock();
}

template<typename T>
void DB::d_insert(typename T::node_ptr& node, T* tree)
{
	std::cout << "INSERT" << std::endl;
	
	DBFS::File* f = DBFS::create();
	string new_name = f->name();
	
	if(!tree->is_leaf(node)){
		intr_t intr_d;
		intr_d.type = get_tree_type<T>();
		intr_d.childs_type = (tree->is_leaf(node->first_child_node()) ? NODE_TYPES::LEAF : NODE_TYPES::INTR);
		int c = node->get_nodes()->size();
		std::vector<string> keys(node->keys_iterator(), node->keys_iterator_end());
		std::vecotr<string> nodes(c);
		for(int i=0;i<c;i++){
			node_data_ptr d = static_pointer_cast<node_data_t>( (*(node->get_nodes()))[i]->data );
			nodes[i] = d->path;
		}
		intr_d.child_keys = keys;
		intr_d.child_values = nodes;
		write_intr<T>(f, intr_d);
		
		f->close();
		if(!node->data){
			node->data = create_node_data(false, new_name);
			// Catche new intr node
			typename T::node_ptr n = new typename T::InternalNode();
			n->set_keys(node->get_keys());
			n->set_nodes(node->get_nodes());
			intr_cache_m.lock();
			intr_cache.push(new_name, n);
			intr_cache_r[new_name] = make_pair(n, 1);
		} else {
			node_data_ptr data = static_pointer_cast<node_data_t>(node->data);
			string cur_name = data->path;
			DBFS::remove(cur_name);
			DBFS::move(new_name, cur_name);
		}
	} else {
		leaf_t leaf_d;
		leaf_d.type = get_tree_type<T>();
		int c = node->get_childs()->size();
		auto* keys = new std::vector<typename T::key_type>(c);
		auto* lengths = new std::vector<int_t>(c);
		
		typename T::node_ptr p_leaf = node->prev_leaf();
		typename T::node_ptr n_leaf = node->next_leaf();
		node_data_ptr p_leaf_data = static_pointer_cast<node_data_t>(p_leaf->data);
		node_data_ptr n_leaf_data = static_pointer_cast<node_data_t>(n_leaf->data);
		leaf_d.left_leaf = p_leaf_data->path;
		leaf_d.right_leaf = n_leaf_data->path;
		
		for(auto& it : node->get_childs()){
			keys->push_back(it.first);
			lengths->push_back(it.second.size());
		}
		leaf_d.child_keys = keys;
		leaf_d.child_lengths = lengths;
		write_leaf(f, leaf_d);
		for(auto& it : node->get_childs()){
			write_leaf_item(f, it.second);
		}
		
		// TODO: complete leaf writing
	}
	
	// Save Base File
	string base_file_name = tree_cache_f[(int_t)tree];
	DBFS::File base_f = DBFS::create();
	tree_base_read_t base_d;
	base_d.type = get_tree_type<T>();
	base_d.count = tree->size();
	base_d.factor = tree->get_factor();
	typename T::node_ptr root_node = tree->get_root_pub();
	base_d.is_leaf = tree->is_leaf(root_node);
	node_data_ptr base_data = static_pointer_cast<node_data_t>(root_node->data);
	base_d.branch = base_data->path;
	write_base(base_f, base_d);
	string new_base_file_name = base_f->name();
	base_f->close();
	DBFS::remove(base_file_name);
	DBFS::rename(new_base_file_name, base_file_name);
}

template<typename T>
void DB::d_remove(typename T::node_ptr& node, T* tree)
{
	std::cout << "REMOVE" << std::endl;
	
	node_data_ptr = static_pointer_cast<node_data_t>(node->data);
	DBFS::remove(data->path);
}

template<typename T>
void DB::d_reserve(typename T::node_ptr& node, T* tree)
{
	std::cout << "RESERVE" << std::endl;
}

template<typename T>
void DB::d_release(typename T::node_ptr& node, T* tree)
{
	std::cout << "RELEASE" << std::endl;
}

template<typename T>
void DB::d_before_move(typename T::iterator& it, typename T::node_ptr& node, int_t step, T* tree)
{
	std::cout << "BEFORE_MOVE" << std::endl;
}

template<typename T>
void DB::d_after_move(typename T::iterator& it, typename T::node_ptr& node, int_t step, T* tree)
{
	std::cout << "AFTER_MOVE" << std::endl;
}

template<typename T>
void DB::materialize_intr(typename T::node_ptr& node, string path)
{
	typename T::node_ptr n = get_intr<T>(path);
	node->set_keys(n->get_keys());
	node->set_nodes(n->get_nodes());
	intr_cache_r[path].second++;
}

template<typename T>
void DB::materialize_leaf(typename T::node_ptr& node, string path)
{
	typename T::node_ptr n = get_leaf<T>(path);
	node->set_childs(n->get_childs());
	leaf_cache_r[path].second++;
}

template<typename T>
void DB::unmaterialize_intr(typename T::node_ptr& node, string path)
{
	intr_cache_r[path].second--;
	node->set_keys(nullptr);
	node->set_nodes(nullptr);
	check_intr_ref();
}

template<typename T>
void DB::unmaterialize_leaf(typename T::node_ptr& node, string path)
{
	leaf_cache_r[path].second--;
	node->set_childs(nullptr);
	check_leaf_ref();
}

template<typename T>
T::node_ptr DB::get_intr(string path)
{
	using node_ptr = T::node_ptr;
	
	node_ptr intr_data;
	
	// Check cache
	intr_cache_m.lock();
	if(intr_cache.has(path)){
		intr_data = intr_cache.get(path);
		intr_cache_m.unlock();
		return intr_data;
	}
	
	// Check reference
	if(intr_cache_r.count(path)){
		intr_data = intr_cache_r[path].first;
		intr_cache.push(path, intr_data);
		intr_cache_m.unlock();
		return intr_data;
	}
	
	// Check future
	if(intr_cache_q.count(path)){
		std::shared_future<node_ptr> f = intr_cache_q[path];
		intr_cache_m.unlock();
		intr_data = f.get();
		return intr_data;
	}
	
	// Read intr node data
	std::promise<node_ptr> p;
	intr_cache_q[path] = p.get_future();
	intr_cache_m.unlock();
	
	// Fill node
	intr_t intr_d = read_intr<T>(path);
	std::vector<typename T::key_type>* keys_ptr = intr_d.child_keys;
	std::vector<typename T::val_type>* vals_ptr = intr_d.child_values;
	intr_data->add_keys(0, keys_ptr->begin(), keys_ptr->end());
	int c = vals_ptr->size();
	for(int i=0;i<c;i++){
		typename T::node_ptr n;
		string& child_path = (*vals_ptr)[i];
		if(intr_d.childs_type == NODE_TYPES::INTR){
			n = typename T::node_ptr(new typename T::InternalNode());
		} else {
			n = typename T::node_ptr(new typename T::LeafNode());
		}
		n->data = create_node_data(true, child_path);
		intr_data->add_nodes(i,n);
	}
	intr_data->data = create_node_data(false, path);
	
	// Set cache and future
	intr_cache_m.lock();
	intr_cache.push(path, intr_data);
	intr_cache_r[path] = std::make_pair(intr_data,0);
	p.set_value(intr_data);
	intr_cache_q.erase(path);
	intr_cache_m.unlock();
	
	// Return
	return intr_data;
}

template<typename T>
T::node_ptr DB::get_leaf(string path)
{
	using node_ptr = T::node_ptr;
	using entry_pair = <std::pair<const typename T::key_type, typename T::val_type>;
	using entry_ptr = std::shared_ptr<entry_pair>;
	
	node_ptr leaf_data;
	
	// Check cache
	leaf_cache_m.lock();
	if(leaf_cache.has(path)){
		leaf_data = leaf_cache.get(path);
		leaf_cache_m.unlock();
		return leaf_data;
	}
	
	// Check reference
	if(leaf_cache_r.count(path)){
		leaf_data = leaf_cache_r[path].first;
		leaf_cache.push(path, leaf_data);
		leaf_cache_m.unlock();
		return leaf_data;
	}
	
	// Check future
	if(leaf_cache_q.count(path)){
		std::shared_future<node_ptr> f = leaf_cache_q[path];
		leaf_cache_m.unlock();
		leaf_data = f.get();
		return leaf_data;
	}
	
	// Read leaf node data
	std::promise<node_ptr> p;
	leaf_cache_q[path] = p.get_future();
	leaf_cache_m.unlock();
	
	// Fill node
	leaf_t leaf_d = read_leaf<T>(path);
	std::vector<typename T::key_type>* keys_ptr = leaf_d.child_keys;
	std::vector<int_t>* vals_length = leaf_d.child_lengths;
	int_t start_data = leaf_d.start_data;
	std::shared_ptr<File> f(leaf_d.file);
	int c = keys_ptr->size();
	if(leaf_d.type == TREE_TYPES::ROOT){
		int mx_key = 0;
		for(int i=0;i<c;i++){
			mx_key = std::max(mx_key, (*vals_length)[i]);
		}
		char* buf = new char[mx_key+1];
		for(int i=0;i<c;i++){
			f->read(buf, (*vals_length)[i]);
			leaf_data->insert(entry_ptr(entry_pair( (*keys_ptr)[i], string(buf,(*vals_length)[i]) )));
		}
	}
	else{
		for(int i=0;i<c;i++){
			leaf_data->insert(entry_ptr(entry_pair( (*keys_ptr)[i], file_data_t(start_data, (*vals_length)[i], f, [](file_data_t* self, char* buf, int sz){ 
				self->f->read(buf, sz);
			}) )));
		}
	}
	typename T::node_ptr left_node = typename T::node_ptr(new typename T::LeafNode());
	typename T::node_ptr right_node = typename T::node_ptr(new typename T::LeafNode());
	left_node->data = create_node_data(true, leaf_d.left_leaf);
	right_node->data = create_node_data(true, leaf_d.right_leaf);
	leaf_data.set_left_leaf(left_node);
	leaf_data.set_right_leaf(right_node);
	leaf_data->data = create_node_data(false, path);
	
	// Set cache and future
	leaf_cache_m.lock();
	leaf_cache.push(path, leaf_data);
	leaf_cache_r[path] = std::make_pair(leaf_data,0);
	p.set_value(leaf_data);
	leaf_cache_q.erase(path);
	leaf_cache_m.unlock();
	
	// Return
	return leaf_data;
}

template<typename T>
DB::intr_t DB::read_intr<T>(string filename)
{
	TREE_TYPES type = get_tree_type<T>();
	using key_type = typename T::key_type;
	using val_type = typename T::val_type;
	
	int t;
	int c;
	
	DBFS::File* f = new DBFS::File(filename);
	f->read(t);
	f->read(c);
	
	std::vector<key_type>* keys = new std::vector<key_type>(c-1);
	std::vector<val_type>* vals = new std::vector<val_type>(c);
	
	for(int i=0;i<c-1;i++){
		f->read(keys[i]);
	}
	for(int i=0;i<c;i++){
		f->read(vals[i]);
	}
	
	return {type, (NODE_TYPES)t, keys, vals};
}

template<typename T>
DB::leaf_t DB::read_leaf<T>(string filename)
{
	TREE_TYPES type = get_tree_type<T>();
	using key_type = typename T::key_type;
	using val_type = typename T::val_type;
	
	DBFS::File* f = new DBFS::File(filename);
	
	int c;
	string left_leaf, right_leaf;
	int_t start_data;
	
	f->read(c);
	f->read(left_leaf);
	f->read(right_leaf);
	auto* keys = new std::vector<string>(c);
	auto* vals_lengths = new std::vector<int_t>(c);
	for(int i=0;i<c;i++){
		f->read(keys[i]);
	}
	for(int i=0;i<c;i++){
		f->read(vals_lengths[i]);
	}
	start_data = f->tell();
	leaf_t t;
	t.type = get_tree_type<T>();
	t.child_keys = keys;
	t.child_lengths = vals_lengths;
	t.left_leaf = left_leaf;
	t.right_leaf = right_leaf;
	t.start_data = start_data;
	t.file = f;
	return t;
}

template<typename T>
TREE_TYPES DB::get_tree_type<T>()
{
	if(std::is_same<T,int_tree_t>::value)
		return TREE_TYPES::KEY_INT;
	if(std::is_same<T,string_tree_t>::value)
		return TREE_TYPES::KEY_STRING;
	return TREE_TYPES::ROOT;
}

#endif // DB_H

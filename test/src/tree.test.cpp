#include "forest.hpp"

int hooks_time = 0, hooks_time_inner = 0;
int hook_d_enter_time=0, 
	hook_d_leave_time=0, 
	hook_d_insert_time=0, 
	hook_d_remove_time=0, 
	hook_d_reserve_time=0, 
	hook_d_release_time=0, 
	hook_d_insert_leaf_time=0,
	hook_d_split_time=0,
	hook_d_ref_time=0,
	hook_d_base_time=0,
	hook_unmaterialize_leaf=0,
	hook_unmaterialize_intr=0,
	hook_remove_leaf=0,
	hook_read_leaf=0;
	

SCENARIO_START

DESCRIBE("Test multi threads", {
	DESCRIBE("Initialize forest at tmp/t2", {
		
		//std::cout << "HER!" << std::endl;
		
		mutex mt;
		
		BEFORE_ALL({
			config_high();
			forest::bloom("tmp/t2");
		});
		
		AFTER_ALL({
			forest::fold();
			//std::cout << "AFTER_ALL!!!\n";
			// Remove dirs?
		});
		
		DESCRIBE("Add 100 trees in 10 threads", {
			BEFORE_ALL({
				vector<thread> trds;
				for(int i=0;i<10;i++){
					thread t([&mt](int i){
						while(i<100){
							//mt.lock();
							forest::create_tree(forest::TREE_TYPES::KEY_STRING, "test_string_tree_"+to_string(i));
							i+=10;
							//mt.unlock();
						}
					},i);
					trds.push_back(move(t));
				}
				for(int i=0;i<10;i++){
					trds[i].join();
				}
			});
			
			IT("Trees should be created", {
				int fc = dir_count("tmp/t2");
				TEST_SUCCEED();
				//EXPECT(fc).toBeGreaterThanOrEqual(102);
				INFO_PRINT("Dirs count: " + to_string(fc));
			});
			
			DESCRIBE("Then remove 100 trees in 10 threads", {
				BEFORE_ALL({
					vector<thread> trds;
					for(int i=0;i<10;i++){
						thread t([&mt](int i){
							while(i<100){
								//mt.lock();
								forest::delete_tree("test_string_tree_"+to_string(i));
								i+=10;
								//mt.unlock();
							}
						},i);
						trds.push_back(move(t));
					}
					for(int i=0;i<10;i++){
						trds[i].join();
					}
				});
				
				IT("Trees should be deleted", {
					int fc = dir_count("tmp/t2");
					// After savior implementation not all files is going to be delete immediatelu
					// So increasing the value to not block the project with tests reimplenemtation.
					//EXPECT(fc).toBeLessThanOrEqual(20);
					TEST_SUCCEED();
					INFO_PRINT("Dirs count: " + to_string(fc));
				});
			});
		});
		
		DESCRIBE("Add 100 trees in random shuffle", {
			vector<int> nums;
			int cnt = 100;
			
			BEFORE_ALL({
				for(int i=0;i<cnt;i++){
					nums.push_back(i);
				}
				for(int i=0;i<cnt;i++){
					swap(nums[i],nums[rand()%cnt]);
				}
				
				vector<thread> trds;
				
				for(int i=0;i<100;i++){
					thread t([&cnt,&nums,&mt](int i){
						while(i<cnt){
							//mt.lock();
							forest::create_tree(forest::TREE_TYPES::KEY_STRING, "test_string_tree_"+to_string(nums[i]));
							i+=100;
							//mt.unlock();
						}
					},i);
					trds.push_back(move(t));
				}
				for(int i=0;i<100;i++){
					trds[i].join();
				}
			});
			
			IT("Trees should be created", {
				int fc = dir_count("tmp/t2");
				//EXPECT(fc).toBeGreaterThanOrEqual(10);
				TEST_SUCCEED();
				INFO_PRINT("Dirs count: " + to_string(fc));
			});
			
			DESCRIBE("Then remove all trees in random shuffle in 100 threads", {
				BEFORE_ALL({
					for(int i=0;i<cnt;i++){
						swap(nums[i],nums[rand()%cnt]);
					}
					vector<thread> trds;
					for(int i=0;i<100;i++){
						thread t([&cnt,&nums,&mt](int i){
							while(i<cnt){
								//mt.lock();
								forest::delete_tree("test_string_tree_"+to_string(nums[i]));
								i+=100;
								//mt.unlock();
							}
						},i);
						trds.push_back(move(t));
					}
					for(int i=0;i<100;i++){
						trds[i].join();
					}
				});
				
				IT("Trees should be deleted", {
					int fc = dir_count("tmp/t2");
					// After savior implementation not all files is going to be delete immediatelu
					// So increasing the value to not block the project with tests reimplenemtation.
					//EXPECT(fc).toBeLessThanOrEqual(100);
					TEST_SUCCEED();
					INFO_PRINT("Dirs count: " + to_string(fc));
				});
			});
		});
		
		// Leaf insertions
		DESCRIBE("Add `test` tree to the forest", {
			BEFORE_EACH({
				forest::create_tree(forest::TREE_TYPES::KEY_STRING, "test", 3);
			});
			
			AFTER_EACH({
				//std::cout << "WAIT 3 SEC\n";
				//std::this_thread::sleep_for(std::chrono::seconds(3));
				//std::cout << "================================================\nSTART_DELETE_TREE\n";
				forest::delete_tree("test");
			});
			
			DESCRIBE("Add 100 items with even keys and lower/upper bound records", {
				vector<string> resl[100];
				vector<string> resr[100];
				
				BEFORE_ALL({
					for(int i=0;i<10;i++){
						char c = (i*2)+'a';
						string k = "";
						k.push_back(c);
						forest::insert_leaf("test", k, forest::leaf_value("value_" + std::to_string(i*2)));
					}
					
					vector<thread> trds;
					for(int i=0;i<100;i++){
						thread t([&resl, &resr](int ind){
							for(int j=0;j<10;j++){
								string k="";
								k.push_back('a'+j);
								auto record = forest::find_leaf("test", k, forest::RECORD_POSITION::LOWER);
								resl[ind].push_back(record->key());
								record = forest::find_leaf("test", k, forest::RECORD_POSITION::UPPER);
								resr[ind].push_back(record->key());
							}
						}, i);
						trds.push_back(move(t));
					}
					for(int i=0;i<100;i++){
						trds[i].join();
					}
				});
				
				IT("result should be expected", {
					for(int i=0;i<100;i++){
						for(int j=0;j<10;j++){
							string kl = "", kr = "";
							kl.push_back('a' + (j-j%2));
							kr.push_back('a' + (j-j%2 + 2));
							EXPECT(resl[i][j]).toBe(kl);
							EXPECT(resr[i][j]).toBe(kr);
						}
					}
				});
			});
			
			DESCRIBE("Add 120 items in 10 threads to the tree", {
				BEFORE_ALL({
					vector<thread> trds;
					for(int i=0;i<10;i++){
						thread t([](int ind){
							while(ind<120){
								forest::insert_leaf("test", "k"+std::to_string(ind), forest::leaf_value("value_" + std::to_string(ind*ind)));
								ind += 10;
							}
						}, i);
						trds.push_back(move(t));
					}
					for(int i=0;i<10;i++){
						trds[i].join();
					}
				});
				
				IT("Tree should contain keys from `k0` to `k99`", {
					EXPECT(forest::read_leaf_item(forest::find_leaf("test", "k25")->val())).toBe("value_625");
					EXPECT(forest::read_leaf_item(forest::find_leaf("test", "k100")->val())).toBe("value_10000");
					for(int i=0;i<100;i++){
						EXPECT(forest::read_leaf_item(forest::find_leaf("test", "k" + std::to_string(i))->val())).toBe("value_" + std::to_string(i*i));
					}
				});
			});
			
			DESCRIBE("Add 100 random items in 100 threads to the tree and check the value of all items in 100 threads", {
				unordered_set<int> check;
				bool good = true;
				BEFORE_ALL({
					vector<thread> trds;
					for(int i=0;i<100;i++){
						int rnd = rand()%10000 + 200;
						if(!check.count(rnd)){
							check.insert(rnd);
							thread t([](int ind){								
								forest::insert_leaf("test", "p"+std::to_string(ind), forest::leaf_value("val_" + std::to_string(ind*ind)));
							}, rnd);
							trds.push_back(move(t));
						}
					}
					for(auto &it : trds){
						it.join();
					}
					trds.resize(0);
					for(auto &it : check){
						thread t([&good](int ind){		
							auto it = forest::find_leaf("test", "p" + std::to_string(ind));						
							if( forest::read_leaf_item(it->val()) != "val_" + std::to_string(ind*ind) ){
								good = false;
							}
						}, it);
						trds.push_back(move(t));
					}
					for(auto &it : trds){
						it.join();
					}
				});
				
				IT("all leafs should exist in tree", {
					EXPECT(good).toBe(true);
				});
			});
			
			DESCRIBE("Add 150 items in random shuffle", {
				unordered_set<int> check;
				int num_of_items;
				BEFORE_EACH({
					
					check.clear();
					
					for(int i=0;i<150;i++){
						int rnd = rand()%10000 + 200;
						if(!check.count(rnd)){
							check.insert(rnd);
							//std::cout << "ADDED: p"+std::to_string(rnd) + "\n";
							forest::insert_leaf("test", "p"+std::to_string(rnd), forest::leaf_value("val_" + std::to_string(rnd*rnd)));
						}
					}
					num_of_items = check.size();
					
					//std::cout << std::to_string(num_of_items) + " ITEMS INSERTED\n";
					//assert(false);
				});
				
				
				DESCRIBE("Move throug all of the items in 2 threads one to another", {
					vector<string> tree_keys_f, tree_keys_b;
					
					BEFORE_ALL({
						auto itf = forest::find_leaf("test", forest::RECORD_POSITION::BEGIN);
						auto itb = forest::find_leaf("test", forest::RECORD_POSITION::END);

						thread t1([&itf, &tree_keys_f](){	
							do{
								tree_keys_f.push_back(itf->key());
							}while(itf->move_forward());
						});
						
						thread t2([&itb, &tree_keys_b](){
							do{
								tree_keys_b.push_back(itb->key());
							}while(itb->move_back());
						});
						
						t1.join();
						t2.join();
					});
					
					IT("All keys should be iterated", {
						vector<string> set_keys;
						for(auto& it : check){
							set_keys.push_back("p"+std::to_string(it));
						}
						
						sort(set_keys.begin(), set_keys.end());
						sort(tree_keys_f.begin(), tree_keys_f.end());
						sort(tree_keys_b.begin(), tree_keys_b.end());
						
						EXPECT(set_keys.size()).toBe(tree_keys_f.size());
						EXPECT(set_keys.size()).toBe(tree_keys_b.size());
						
						EXPECT(set_keys).toBeIterableEqual(tree_keys_f);
						EXPECT(set_keys).toBeIterableEqual(tree_keys_b);
					});
				});
				
				DESCRIBE("Move throug all of the items in 100 threads in random shuffle", {
					
					int tests_count = 400;
					
					vector<vector<string> > tree_keys_check(tests_count);
					vector<thread> threads;
					
					BEFORE_ALL({
						for(int i=0;i<tests_count;i++){
							thread t([&tree_keys_check](int index){
								auto it = forest::find_leaf("test", (index%2 ? forest::RECORD_POSITION::BEGIN : forest::RECORD_POSITION::END));
								do{
									tree_keys_check[index].push_back(it->key());
									if( (index%2 == 1 && !it->move_forward()) || (index%2 == 0 && !it->move_back()) ){
										break;
									}
								}while(true);
							}, i);
							threads.push_back(move(t));
						}
						for(auto& it : threads){
							it.join();
						}
					});
					
					IT("All iterators should successfully move throug the all records", {
						vector<string> set_keys;
						for(auto& it : check){
							set_keys.push_back("p"+std::to_string(it));
						}
						sort(set_keys.begin(), set_keys.end());
						
						for(auto& it : tree_keys_check){
							sort(it.begin(), it.end());
							EXPECT(set_keys).toBeIterableEqual(it);
						}
					});
				});
				
				DESCRIBE("Do 200 operations in 200 threads", {
					int tests_count = 200;
					
					vector<vector<string> > tree_keys_check;
					vector<thread> threads;
					
					unordered_set<string> should_contains;
					queue<int> q;
					
					vector<string> complete;
					
					bool num_assert = true;
					
					BEFORE_ALL({
						for(auto& it : check){
							q.push(it);
							should_contains.insert("p"+std::to_string(it));
						}
						mutex m;
						
						for(int i=0;i<tests_count;i++){
							int rnd = rand()%10000 + 11200;
							thread t([&m, &q, &check, &tree_keys_check, &should_contains, &complete, &num_assert](int index, int rnd){
								int cs = index%8;
								if(cs == 1){
									//return;
									
									auto it = forest::find_leaf("test", forest::RECORD_POSITION::BEGIN);
									vector<string> keys;
									do{
										string key = it->key();
										keys.push_back(key);
										auto val = it->val();
										string sval = forest::read_leaf_item(val);
										
										if(sval.substr(0,3) == "new"){
											continue;
										}
										int k=0, v=0;
										try{
											k = std::stoi(key.substr(1));
											v = std::stoi(sval.substr(4));
										} catch (std::invalid_argument const &e) {
											std::cout << "K: " + key + " " + sval + "\n";
											assert(false);
										}
										if(k*k != v){
											num_assert = false;
										}
									}while(it->move_forward());
									lock_guard<mutex> lock(m);
									tree_keys_check.push_back(keys);
									complete.push_back("move");
								}
								else if(cs == 2){
									//return;
									
									auto it = forest::find_leaf("test", forest::RECORD_POSITION::END);
									vector<string> keys;
									do{
										string key = it->key();
										keys.push_back(key);
										auto val = it->val();
										string sval = forest::read_leaf_item(val);
										
										if(sval.substr(0,3) == "new"){
											continue;
										}
										int k=0, v=0;
										try{
											k = std::stoi(key.substr(1));
											v = std::stoi(sval.substr(4));
										} catch (std::invalid_argument const &e) {
											std::cout << "K: " + key + " " + sval + "\n";
											assert(false);
										}
										if(k*k != v){
											num_assert = false;
										}
									}while(it->move_back());
									lock_guard<mutex> lock(m);
									tree_keys_check.push_back(keys);
									complete.push_back("move");
								}
								else if(cs == 3){
									// doesn't matter
								}
								else if(cs == 4){
									//return; 
									int p;
									{
										lock_guard<mutex> lock(m);
										p = q.front();
										q.pop();
									}
									auto it = forest::find_leaf("test", "p"+std::to_string(p));
									do{
										it->val();
									}while(it->move_forward());
								}
								else if(cs == 5){
									//return;
									
									int p;
									{
										lock_guard<mutex> lock(m);
										p = q.front();
										q.pop();
									}
									auto it = forest::find_leaf("test", "p"+std::to_string(p));
									do{
										it->val();
									}while(it->move_back());
								}
								else if(cs == 6){
									//return;
									int p;
									{
										lock_guard<mutex> lock(m);
										p = q.front();
										q.pop();
									}
									forest::update_leaf("test", "p"+std::to_string(p), std::move(forest::leaf_value("new_" + std::to_string(rnd*rnd))) );
								}
								else if(cs == 7){
									//return;
									int p;
									{
										lock_guard<mutex> lock(m);
										p = q.front();
										q.pop();
									}
									forest::erase_leaf("test", "p"+std::to_string(p));
									lock_guard<mutex> lock(m);
									should_contains.erase("p"+std::to_string(p));
									complete.push_back("erase");
								}
								else{
									//return;
									{
										lock_guard<mutex> lock(m);
										if(check.count(rnd)){
											return;
										}
										check.insert(rnd);
									}
									forest::file_data_ptr tmp = forest::leaf_value("val_" + std::to_string(rnd*rnd));
									//forest::uint_t pt_int = (forest::uint_t)tmp.get();
									//std::cout << "TRY_TO_INSERT: " + std::to_string(pt_int) + "\n";
									forest::insert_leaf("test", "p" + std::to_string(rnd), std::move(tmp) );
									//std::cout << "LEAF_INSERT_DONE: " + std::to_string(pt_int) + "\n";
									lock_guard<mutex> lock(m);
									complete.push_back("insert");
								}
							}, i, rnd);
							threads.push_back(move(t));
						}
						for(auto& it : threads){
							it.join();
						}
					});
					
					IT("All expected records should be iterated", {
						for(auto& vec : tree_keys_check){
							unordered_set<string> keys;
							for(auto& it : vec){
								keys.insert(it);
							}
							for(auto& it : should_contains){
								EXPECT(keys.count(it)).toBe(1);
							}
						}
						EXPECT(num_assert).toBe(true);
					});
				});
			});
		});
		
		
		DESCRIBE("Performance Tests", {
					
			int time_free;
			chrono::high_resolution_clock::time_point p1,p2;
			//int time_to_create_value=0;
			
			BEFORE_ALL({
				forest::create_tree(forest::TREE_TYPES::KEY_STRING, "test_else", 500);
			});
			
			BEFORE_EACH({
				time_free =
				hook_d_enter_time =
				hook_d_leave_time =
				hook_d_insert_time =
				hook_d_remove_time =
				hook_d_reserve_time =
				hook_d_release_time =
				hook_d_insert_leaf_time =
				hook_d_split_time =
				hook_d_ref_time =
				hook_d_base_time =
				hook_remove_leaf =
				hook_read_leaf =
				hooks_time = 0;
			});
			
			AFTER_ALL({
				forest::delete_tree("test_else");
			});
			
			DESCRIBE_SKIP("Ordered requests", {
				
				int rec_count = 1000000;
				
				IT("Insert 100000 items [0,100000)", {
					p1 = chrono::system_clock::now();
					forest::tree_ptr tree = forest::find_tree("test_else");
					for(int i=0;i<rec_count;i++){
						int rnd = i;//rand()%1000000 + 11200;
						
						tree->insert(to_str(rnd), forest::leaf_value("some pretty basic value to insert into the database"));
						//forest::insert_leaf("test_else", to_str(rnd), forest::leaf_value("some pretty basic value to insert into the database"));
					}
					forest::leave_tree(tree);
					p2 = chrono::system_clock::now();
					time_free = chrono::duration_cast<chrono::milliseconds>(p2-p1).count();
					TEST_SUCCEED();
					INFO_PRINT("Time For Insert: " + to_string(time_free) + "ms");
					INFO_PRINT("d_enter Time: " + to_string(hook_d_enter_time/1000));
					INFO_PRINT("d_leave Time: " + to_string(hook_d_leave_time/1000));
					INFO_PRINT("d_insert Time: " + to_string(hook_d_insert_time/1000));
					INFO_PRINT("d_remove Time: " + to_string(hook_d_remove_time/1000));
					INFO_PRINT("d_reserve Time: " + to_string(hook_d_reserve_time/1000));
					INFO_PRINT("d_release Time: " + to_string(hook_d_release_time/1000));
					INFO_PRINT("d_leaf_insert Time: " + to_string(hook_d_insert_leaf_time/1000));
					INFO_PRINT("d_leaf_split Time: " + to_string(hook_d_split_time/1000));
					INFO_PRINT("d_ref Time: " + to_string(hook_d_ref_time/1000));
					INFO_PRINT("d_base Time: " + to_string(hook_d_base_time/1000));
					INFO_PRINT("remove_leaf Time: " + to_string(hook_remove_leaf/1000));
					INFO_PRINT("read_leaf Time: " + to_string(hook_read_leaf/1000));
					INFO_PRINT("HOOKS_TIME: " + to_string(hooks_time/1000));
				});
				
				IT("Get all items independently", {
					p1 = chrono::system_clock::now();
					for(int i=0;i<rec_count;i++){
						int rnd = i;
						auto rc = forest::find_leaf("test_else", to_str(rnd));
						EXPECT(forest::read_leaf_item(rc->val())).toBe("some pretty basic value to insert into the database");
					}
					p2 = chrono::system_clock::now();
					time_free = chrono::duration_cast<chrono::milliseconds>(p2-p1).count();
					TEST_SUCCEED();
					INFO_PRINT("Time For Find: " + to_string(time_free) + "ms");
					INFO_PRINT("d_enter Time: " + to_string(hook_d_enter_time/1000));
					INFO_PRINT("d_leave Time: " + to_string(hook_d_leave_time/1000));
					INFO_PRINT("d_insert Time: " + to_string(hook_d_insert_time/1000));
					INFO_PRINT("d_remove Time: " + to_string(hook_d_remove_time/1000));
					INFO_PRINT("d_reserve Time: " + to_string(hook_d_reserve_time/1000));
					INFO_PRINT("d_release Time: " + to_string(hook_d_release_time/1000));
					INFO_PRINT("d_leaf_insert Time: " + to_string(hook_d_insert_leaf_time/1000));
					INFO_PRINT("d_leaf_split Time: " + to_string(hook_d_split_time/1000));
					INFO_PRINT("d_ref Time: " + to_string(hook_d_ref_time/1000));
					INFO_PRINT("d_base Time: " + to_string(hook_d_base_time/1000));
					INFO_PRINT("remove_leaf Time: " + to_string(hook_remove_leaf/1000));
					INFO_PRINT("read_leaf Time: " + to_string(hook_read_leaf/1000));
					INFO_PRINT("HOOKS_TIME: " + to_string(hooks_time/1000));
				});
				
				IT("Move through the all values", {
					p1 = chrono::system_clock::now();
					auto rc = forest::find_leaf("test_else", forest::RECORD_POSITION::BEGIN);
					int cnt = 0;
					do{
						EXPECT(forest::read_leaf_item(rc->val())).toBe("some pretty basic value to insert into the database");
						cnt++;
					}while(rc->move_forward());
					EXPECT(cnt).toBe(rec_count);
					INFO_PRINT("CNT: " + std::to_string(cnt));
					p2 = chrono::system_clock::now();
					time_free = chrono::duration_cast<chrono::milliseconds>(p2-p1).count();
					TEST_SUCCEED();
					INFO_PRINT("Time For Move: " + to_string(time_free) + "ms");
					INFO_PRINT("d_enter Time: " + to_string(hook_d_enter_time/1000));
					INFO_PRINT("d_leave Time: " + to_string(hook_d_leave_time/1000));
					INFO_PRINT("d_insert Time: " + to_string(hook_d_insert_time/1000));
					INFO_PRINT("d_remove Time: " + to_string(hook_d_remove_time/1000));
					INFO_PRINT("d_reserve Time: " + to_string(hook_d_reserve_time/1000));
					INFO_PRINT("d_release Time: " + to_string(hook_d_release_time/1000));
					INFO_PRINT("d_leaf_insert Time: " + to_string(hook_d_insert_leaf_time/1000));
					INFO_PRINT("d_leaf_split Time: " + to_string(hook_d_split_time/1000));
					INFO_PRINT("d_ref Time: " + to_string(hook_d_ref_time/1000));
					INFO_PRINT("d_base Time: " + to_string(hook_d_base_time/1000));
					INFO_PRINT("remove_leaf Time: " + to_string(hook_remove_leaf/1000));
					INFO_PRINT("read_leaf Time: " + to_string(hook_read_leaf/1000));
					INFO_PRINT("HOOKS_TIME: " + to_string(hooks_time/1000));
				});
				
				IT("Remove all records", {
					p1 = chrono::system_clock::now();
					for(int i=0;i<rec_count;i++){
						int rnd = i;
						forest::erase_leaf("test_else", to_str(rnd));
					}
					p2 = chrono::system_clock::now();
					time_free = chrono::duration_cast<chrono::milliseconds>(p2-p1).count();
					TEST_SUCCEED();
					INFO_PRINT("Time For Remove: " + to_string(time_free) + "ms");
					INFO_PRINT("d_enter Time: " + to_string(hook_d_enter_time/1000));
					INFO_PRINT("d_leave Time: " + to_string(hook_d_leave_time/1000));
					INFO_PRINT("d_insert Time: " + to_string(hook_d_insert_time/1000));
					INFO_PRINT("d_remove Time: " + to_string(hook_d_remove_time/1000));
					INFO_PRINT("d_reserve Time: " + to_string(hook_d_reserve_time/1000));
					INFO_PRINT("d_release Time: " + to_string(hook_d_release_time/1000));
					INFO_PRINT("d_leaf_insert Time: " + to_string(hook_d_insert_leaf_time/1000));
					INFO_PRINT("d_leaf_split Time: " + to_string(hook_d_split_time/1000));
					INFO_PRINT("d_ref Time: " + to_string(hook_d_ref_time/1000));
					INFO_PRINT("d_base Time: " + to_string(hook_d_base_time/1000));
					INFO_PRINT("remove_leaf Time: " + to_string(hook_remove_leaf/1000));
					INFO_PRINT("read_leaf Time: " + to_string(hook_read_leaf/1000));
					INFO_PRINT("HOOKS_TIME: " + to_string(hooks_time/1000));
				});
			});
		});
	});
	
	DESCRIBE_SKIP("Initialize forest at tmp/mem", {
		
		AFTER_ALL({
			int a;
			cout << "COMPLETE" << endl;
			cin >> a;
		});
		
		DESCRIBE("TEST FOR MEMORY LEAKS", {
		
			BEFORE_ALL({
				forest::bloom("tmp/mem");
				forest::create_tree(forest::TREE_TYPES::KEY_STRING, "test", 10);
			});
			
			IT_ONLY("Should add 10000 records, and the memory should not increase infinitely", {
				for(int i=0;i<10000;i++){
					forest::insert_leaf("test", "t"+std::to_string(i), forest::leaf_value("test_values"));
					//if(i%100 == 0){
					//	cout << i << ": " << ccp << "-" << ccm << " " << cip << "-" << cim << " " << ffp << "-" << ffm << " | " << active_nodes_count << endl;
					//}
				}
			});
			
			IT_ONLY("Should move through 10000 records, and the memory should not increase infinitely", {
				auto it = forest::find_leaf("test", forest::RECORD_POSITION::BEGIN);
				//int i=0;
					
				do{					
					string sval = forest::read_leaf_item(it->val());
					//if(i++%100 == 0){
					//	cout << i << ": " << ccp << "-" << ccm << " " << cip << "-" << cim << " " << ffp << "-" << ffm << endl;
					//}
				}while(it->move_forward());
			
			});
			
			IT_ONLY("Should delete 10000 records, and the memory should not increase infinitely", {
				for(int i=0;i<10000;i++){
					forest::erase_leaf("test", "t"+std::to_string(i));
					//if(i%100 == 0){
					//	cout << i << ": " << ccp << "-" << ccm << " " << cip << "-" << cim << " " << ffp << "-" << ffm << endl;
					//}
				}
			});
			
			IT_ONLY("Should create 1000 trees, and the memory should not increase infinitely", {
				for(int i=0;i<1000;i++){
					forest::create_tree(forest::TREE_TYPES::KEY_STRING, "test_"+to_string(i), 10);
					//if(i%100 == 0){
					//	cout << i << ": " << ccp << "-" << ccm << " " << cip << "-" << cim << " " << ffp << "-" << ffm << endl;
					//}
				}
			});
			
			IT_ONLY("Should delete 1000 trees, and the memory should not increase infinitely", {
				for(int i=0;i<1000;i++){
					forest::delete_tree("test_"+to_string(i));
					//if(i%100 == 0){
					//	cout << i << ": " << ccp << "-" << ccm << " " << cip << "-" << cim << " " << ffp << "-" << ffm << endl;
					//}
				}
			});
			
			AFTER_ALL({
				forest::delete_tree("test");
				forest::fold();
				// Remove dirs?
			});
		
		});
		
	});
});

SCENARIO_END

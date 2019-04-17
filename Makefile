test: test.cpp epoch_based.hpp allocation_tracker.hpp concurrent_ptr.hpp deletable_object.hpp guard_ptr.hpp marked_ptr.hpp port.hpp thread_block_list.hpp
	g++ test.cpp -std=c++17 -o test
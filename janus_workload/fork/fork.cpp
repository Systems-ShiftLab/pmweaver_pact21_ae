#include "../m5ops.h"
#include <stdint.h>
#include "../common/common.h"
#include <cstdio>
#include <cstdlib>
#include <iostream>

//#include "gem5/m5ops.h"

int main(int argc, char **argv) {
	size_t id = fork();
	if (id == 0) {
		execl("/temp/gem5-pmdk/pmdk/src/examples/libpmemobj/map/data_store", "/temp/gem5-pmdk/pmdk/src/examples/libpmemobj/map/data_store", argv[1], "/mnt/pmem0/something.ctr", "256", "0");
		std::cout << "Done with id = " << id << std::endl;
	} else {
		execl("/temp/gem5-pmdk/pmdk/src/examples/libpmemobj/map/data_store", "/temp/gem5-pmdk/pmdk/src/examples/libpmemobj/map/data_store", argv[1], "/mnt/pmem0/something.ctr", "256", "1");
		std::cout << "Done with id = " << id << std::endl;
	}/*
	if (id == 0) {
		execl("/home/smahar/git/isca2020/fifo_simple_example_paper/unit_test1", "/home/smahar/git/isca2020/fifo_simple_example_paper/unit_test1","/mnt/pmem0/unit_test_gem5", "0");
		std::cout << "id = " << id << std::endl;
	} else {
		execl("/home/smahar/git/isca2020/fifo_simple_example_paper/unit_test1", "/home/smahar/git/isca2020/fifo_simple_example_paper/unit_test1", "/mnt/pmem0/unit_test_gem5", "1");
		std::cout << "id = " << id << std::endl;
	}*/
//	m5_dump_stats(0,0);
}

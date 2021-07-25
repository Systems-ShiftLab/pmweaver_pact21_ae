all: ../m5_mmap.o ../m5op_x86.o
	$(CXX) -ggdb -O0 $(CFLAGS) -mclwb -I/home/smahar/git/gem5-pmdk/gem5/include -o singly_linked_hash singly_linked_hash.cpp ../common/common.c  ../m5_mmap.o ../m5op_x86.o

../m5_mmap.o: ../m5_mmap.h

../m5op_x86.o:
	$(CC) -ggdb -O0 -I/home/smahar/git/gem5-pmdk/gem5/include -c ../m5op_x86.S  ${CFLAGS} -o ../m5op_x86.o

clean:
	rm -f *.o singly_linked_hash

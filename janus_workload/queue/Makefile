all: ../m5_mmap.o ../m5op_x86.o
	$(CXX) ${CFLAGS} -O0 -mclwb -I/home/smahar/git/gem5-pmdk/gem5/include -o queue queue.cpp ../common/common.c  ../m5_mmap.o ../m5op_x86.o

../m5_mmap.o: ../m5_mmap.h

../m5op_x86.o:
	$(CC) -ggdb -O0 -I/home/smahar/git/gem5-pmdk/gem5/include -c ../m5op_x86.S  ${CFLAGS} -o ../m5op_x86.o

clean:
	rm -f *.o queue

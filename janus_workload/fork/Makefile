all: ../m5_mmap.o ../m5op_x86.o
	$(CXX) -O0 -mclwb -I../asm -o fork fork.cpp ../common/common.c  ../m5_mmap.o ../m5op_x86.o

../m5_mmap.o: ../m5_mmap.h

../m5op_x86.o:
	$(CC) -I../asm -c ../m5op_x86.S  ${CFLAGS} -o ../m5op_x86.o

clean:
	rm -f *.o fork

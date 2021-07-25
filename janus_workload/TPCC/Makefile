CC=g++
CFLAGS=-mclwb
SOURCES= tpcc_db.cc tpcc_nvm.cc ../common/common.c
EXECUTABLE=tpcc_nvm

all:  ../m5_mmap.o ../m5op_x86.o
	$(CXX) $(SOURCES) $(EXTRA_CFLAGS) -ggdb -O0 -mclwb -I../asm -o ${EXECUTABLE} $(CFLAGS) ../m5_mmap.o ../m5op_x86.o

../m5_mmap.o: ../m5_mmap.h

../m5op_x86.o:
	$(CC) -I../asm -c ../m5op_x86.S  ${CFLAGS} -o ../m5op_x86.o

clean:
	rm -f *.o $(EXECUTABLE)

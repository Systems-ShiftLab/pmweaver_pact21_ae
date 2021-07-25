CC=g++
CFLAGS=-mclwb
LDFLAGS=
SOURCES=tatp_db.cc tatp_nvm.cc ../common/common.c
EXECUTABLE=tatp_nvm

all:
	$(CXX) -ggdb -O0 -mclwb -I../asm $(SOURCES) -o ${EXECUTABLE} $(CFLAGS)  ../m5_mmap.o ../m5op_x86.o


../m5_mmap.o: ../m5_mmap.h

../m5op_x86.o:
	$(CC) -I../asm -c ../m5op_x86.S  ${CFLAGS} -o ../m5op_x86.o

clean:
	rm -f *.o $(EXECUTABLE)

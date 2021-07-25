all: multithreaded

CFLAGS += -DSHIFTLAB_MULTITHREADED
CFLAGS += -DGEM5

CFLAGS += -I../
CFLAGS += -I../hashmap
CFLAGS += -I../tree_map
CFLAGS += -I../list_map
CFLAGS += -Wno-error -I/home/smahar/git/gem5-pmdk/gem5/include  -I../../../include -I. -I../../../examples

depend:
	$(MAKE) -C ../map clean
	$(MAKE) -C ../map CFLAGS+="-DSHIFTLAB_MULTITHREADED -DGEM5 -I../list_map -I../tree_map -I../ -I../hashmap -fPIC -I/home/smahar/git/gem5-pmdk/gem5/include" COMPILE_LIB="y" data_store.a
	
multithreaded: depend multithreaded.o
	$(CXX) multithreaded.o ../map/data_store.a

multithreaded.o:
	$(CXX) -c multithreaded.cpp -o multithreaded.o 

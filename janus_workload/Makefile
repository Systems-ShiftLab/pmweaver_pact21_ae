all: m5op_x86.o
	$(MAKE) sub_dirs

sub_dirs: make_arr_swap make_hash make_queue make_TATP make_TPCC

m5op_x86.o:
	$(CC) -ggdb -O0 -c m5op_x86.S  ${CFLAGS} -o m5op_x86.o

make_%:
	$(MAKE) -C $(subst make_,,$@)

clean_%:
	$(MAKE) -C $(subst clean_,,$@) clean

clean: clean_arr_swap clean_btree clean_fork clean_hash clean_queue clean_rbtree clean_TATP clean_TPCC
	rm -f *.o

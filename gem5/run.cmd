export USE_PREDICTOR=1
export ENABLE_EV=1
/home/smahar/git/gem5-pmdk/gem5/build/X86/gem5.fast /home/smahar/git/gem5-pmdk/gem5/configs/example/se.py  -F11000000 --mem-size=8GB --mem-type=DDR4_2400_8x8 --l2cache --cpu-type=DerivO3CPU --caches --l1i_size=32kB --l1d_size=64kB  --l2_size=2MB -c /temp/gem5-pmdk/pmdk/src/examples/libpmemobj/linkedlist/fifo_bulk -o "/mnt/pmem0/data_gen_fifo_bulk 256"

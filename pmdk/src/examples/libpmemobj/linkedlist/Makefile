#
# Copyright 2016, Intel Corporation
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#
#     * Neither the name of the copyright holder nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# examples/libpmemobj/linkedlist/Makefile -- build the tail queue examples
#
PROGS = fifo fifo_bulk

# CFLAGS += "/home/smahar/git/gem5-pmdk/gem5/util/m5/m5op_x86.o"  "/home/smahar/git/gem5-pmdk/gem5/util/m5/m5.o"


include ../../Makefile.inc
CFLAGS += -Wno-error -I/home/smahar/git/gem5-pmdk/gem5/include  -I../../../include -I. -I../../../examples
LIBS =  -lpmemobj -lpmem -pthread

fifo: fifo.o

fifo.o: pmemobj_list.h

fifo_bulk: fifo_bulk.o m5_mmap.o m5op_x86.o

fifo_bulk.o: pmemobj_list.h

m5_mmap.o: m5_mmap.h

m5op_x86.o:
	$(CC) -c m5op_x86.S  ${CFLAGS} -o m5op_x86.o

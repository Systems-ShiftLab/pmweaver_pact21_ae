# Copyright (c) 2012-2013, 2015-2016 ARM Limited
# All rights reserved
#
# The license below extends only to copyright in the software and shall
# not be construed as granting a license to any other intellectual
# property including but not limited to intellectual property relating
# to a hardware implementation of the functionality of the software
# licensed hereunder.  You may use the software subject to the license
# terms below provided that you ensure that this notice is replicated
# unmodified and in its entirety in all distributions of the software,
# modified or unmodified, in source code or in binary form.
#
# Copyright (c) 2010 Advanced Micro Devices, Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met: redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer;
# redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution;
# neither the name of the copyright holders nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
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
# Authors: Lisa Hsu

# Configure the M5 cache hierarchy config in one place
#

from __future__ import print_function
from __future__ import absolute_import

import m5
from m5.objects import *
from .Caches import *
from common import ObjectList

def config_cache(options, system):
    if options.external_memory_system and (options.caches or options.l2cache):
        print("External caches and internal caches are exclusive options.\n")
        sys.exit(1)

    if options.external_memory_system:
        ExternalCache = ExternalCacheFactory(options.external_memory_system)

    if options.cpu_type == "O3_ARM_v7a_3":
        try:
            import cores.arm.O3_ARM_v7a as core
        except:
            print("O3_ARM_v7a_3 is unavailable. Did you compile the O3 model?")
            sys.exit(1)

        dcache_class, icache_class, l2_cache_class, walk_cache_class = \
            core.O3_ARM_v7a_DCache, core.O3_ARM_v7a_ICache, \
            core.O3_ARM_v7aL2, \
            core.O3_ARM_v7aWalkCache
    elif options.cpu_type == "HPI":
        try:
            import cores.arm.HPI as core
        except:
            print("HPI is unavailable.")
            sys.exit(1)

        dcache_class, icache_class, l2_cache_class, walk_cache_class = \
            core.HPI_DCache, core.HPI_ICache, core.HPI_L2, core.HPI_WalkCache
    else:
        dcache_class, icache_class, l2_cache_class, l3_cache_class, walk_cache_class = \
            L1_DCache, L1_ICache, L2Cache, L3Cache, None

        if buildEnv['TARGET_ISA'] == 'x86':
            walk_cache_class = PageTableWalkerCache

    # Set the cache line size of the system
    system.cache_line_size = options.cacheline_size

    # If elastic trace generation is enabled, make sure the memory system is
    # minimal so that compute delays do not include memory access latencies.
    # Configure the compulsory L1 caches for the O3CPU, do not configure
    # any more caches.
    if options.l2cache and options.elastic_trace_en:
        fatal("When elastic trace is enabled, do not configure L2 caches.")

    if not options.l2cache or not options.l3cache:
        assert(0)

    if options.l2cache and options.l3cache:
        # Provide a clock for the L2 and the L1-to-L2 bus here as they
        # are not connected using addTwoLevelCacheHierarchy. Use the
        # same clock as the CPUs.
        system.l2 = l2_cache_class(clk_domain=system.cpu_clk_domain,
                                   size=options.l2_size,
                                   assoc=options.l2_assoc)
        
        system.l3 = l3_cache_class(clk_domain=system.cpu_clk_domain,
                                    size=options.l3_size,
                                    assoc=options.l3_assoc)
        use_pb = 2

        # Attach L2 to L1 using coherent bar
        system.tol2bus = L2XBar(clk_domain = system.cpu_clk_domain)
        system.tol3bus = L3XBar(clk_domain = system.cpu_clk_domain)
        system.l2.cpu_side = system.tol2bus.master
        system.l2.mem_side = system.tol3bus.slave

        system.l3.cpu_side = system.tol3bus.master

        # Attach the predictor backend
        if use_pb == 1:
            print("Attaching predictor backend")
            system.pb = PredictorBackend()
            system.pb.cache_side = system.l2.mem_side
            system.pb.mem_side   = system.membus.slave
        elif use_pb == 0:
            system.l2.mem_side = system.membus.slave
        elif use_pb == 2:
            system.pb = PredictorBackend()
            system.pb.slave = system.l3.mem_side
            system.pb.master = system.membus.slave

        if options.l2_hwp_type:
            hwpClass = ObjectList.hwp_list.get(options.l2_hwp_type)
            if system.l2.prefetcher != "Null":
                print("Warning: l2-hwp-type is set (", hwpClass, "), but",
                      "the current l2 has a default Hardware Prefetcher",
                      "of type", type(system.l2.prefetcher), ", using the",
                      "specified by the flag option.")
            system.l2.prefetcher = hwpClass()

    if options.memchecker:
        system.memchecker = MemChecker()

    for i in range(options.num_cpus):
        if options.caches:
            icache = icache_class(size=options.l1i_size,
                                  assoc=options.l1i_assoc)
            dcache = dcache_class(size=options.l1d_size,
                                  assoc=options.l1d_assoc)

            # If we have a walker cache specified, instantiate two
            # instances here
            if walk_cache_class:
                iwalkcache = walk_cache_class()
                dwalkcache = walk_cache_class()
            else:
                iwalkcache = None
                dwalkcache = None

            if options.memchecker:
                dcache_mon = MemCheckerMonitor(warn_only=True)
                dcache_real = dcache

                # Do not pass the memchecker into the constructor of
                # MemCheckerMonitor, as it would create a copy; we require
                # exactly one MemChecker instance.
                dcache_mon.memchecker = system.memchecker

                # Connect monitor
                dcache_mon.mem_side = dcache.cpu_side

                # Let CPU connect to monitors
                dcache = dcache_mon

            if options.l1d_hwp_type:
                hwpClass = ObjectList.hwp_list.get(options.l1d_hwp_type)
                if dcache.prefetcher != m5.params.NULL:
                    print("Warning: l1d-hwp-type is set (", hwpClass, "), but",
                          "the current l1d has a default Hardware Prefetcher",
                          "of type", type(dcache.prefetcher), ", using the",
                          "specified by the flag option.")
                dcache.prefetcher = hwpClass()

            if options.l1i_hwp_type:
                hwpClass = ObjectList.hwp_list.get(options.l1i_hwp_type)
                if icache.prefetcher != m5.params.NULL:
                    print("Warning: l1i-hwp-type is set (", hwpClass, "), but",
                          "the current l1i has a default Hardware Prefetcher",
                          "of type", type(icache.prefetcher), ", using the",
                          "specified by the flag option.")
                icache.prefetcher = hwpClass()

            # When connecting the caches, the clock is also inherited
            # from the CPU in question

            # SM: Add predictor here with connections on one side to the CPU
            #     and to the L1 cache on the other side.
            print("Adding predictor frontend for cpu " + str(i))
            system.cpu[i].pf =  PredictorFrontend()
            
            # Set the core id for multithreading
            # system.cpu[i].pf.coreid = i

            system.cpu[i].icache = icache
            system.cpu[i].dcache = dcache

            ### Organization
            # CPU <---> Predictor frontend <-> L1 Cache <-> L2 bus <-> L2 <-> predictor bus <-> Predictor backend <-> mem bus
            #       ^                                    ^
            #       |                                    |       
            #       ----- iTB/dTB ------------------------
            #  
            # CPU <-> Predictor frontend
            # system.cpu[i].icache_port = system.pf.inst_port_cpu_side
            # system.cpu[i].dcache_port = system.pf.data_port_cpu_side
                        
            # # Predictor frontend <-> L1 cache
            # system.cpu[i].icache.cpu_side = system.pf.inst_port_l1_side
            # system.cpu[i].dcache.cpu_side = system.pf.data_port_l1_side
            
            # # L1 Cache <-> L2 bus
            # system.cpu[i].icache.mem_side = system.tol2bus.slave
            # system.cpu[i].dcache.mem_side = system.tol2bus.slave
            
            system.cpu[i].icache_port = system.cpu[i].icache.cpu_side
            system.cpu[i].dcache_port = system.cpu[i].pf.slave
                        
            # Predictor frontend <-> L1 cache
            system.cpu[i].dcache.cpu_side = system.cpu[i].pf.master
            
            # L1 Cache <-> L2 bus
            system.cpu[i].icache.mem_side = system.tol2bus.slave
            system.cpu[i].dcache.mem_side = system.tol2bus.slave

            # Add the iTB and dTB
            print("Adding iwalkcache: " + str(iwalkcache))

            # system.cpu[i].itb_walker_cache = iwalkcache
            # system.cpu[i].dtb_walker_cache = dwalkcache

            system.cpu[i].itb.walker.port = system.tol2bus.slave #iwalkcache.cpu_side
            system.cpu[i].dtb.walker.port = system.tol2bus.slave #dwalkcache.cpu_side
            
            # NOTE: iTB/dTB connects with the l2 bus later

            # system.cpu[i].addPrivateSplitL1Caches(icache, dcache,
            #                                       iwalkcache, dwalkcache)

            if options.memchecker:
                # The mem_side ports of the caches haven't been connected yet.
                # Make sure connectAllPorts connects the right objects.
                system.cpu[i].dcache = dcache_real
                system.cpu[i].dcache_mon = dcache_mon

        elif options.external_memory_system:
            # These port names are presented to whatever 'external' system
            # gem5 is connecting to.  Its configuration will likely depend
            # on these names.  For simplicity, we would advise configuring
            # it to use this naming scheme; if this isn't possible, change
            # the names below.
            if buildEnv['TARGET_ISA'] in ['x86', 'arm']:
                system.cpu[i].addPrivateSplitL1Caches(
                        ExternalCache("cpu%d.icache" % i),
                        ExternalCache("cpu%d.dcache" % i),
                        ExternalCache("cpu%d.itb_walker_cache" % i),
                        ExternalCache("cpu%d.dtb_walker_cache" % i))
            else:
                system.cpu[i].addPrivateSplitL1Caches(
                        ExternalCache("cpu%d.icache" % i),
                        ExternalCache("cpu%d.dcache" % i))

        system.cpu[i].createInterruptController()
        if options.l2cache:
            print("Trying to add l2 bus")
            # SM: All the ports are already connected, skipping this, 
            # FIXME: Fix this     
            # system.cpu.createInterruptController()
            system.cpu[i].interrupts[0].pio = system.membus.master
            system.cpu[i].interrupts[0].int_master = system.membus.slave
            system.cpu[i].interrupts[0].int_slave = system.membus.master
            
            # system.cpu[i].itb_walker_cache.mem_side = system.tol2bus.slave
            # system.cpu[i].dtb_walker_cache.mem_side = system.tol2bus.slave

            # system.cpu[i].connectAllPorts(system.tol2bus, system.membus)

            # system.system_port = system.membus.slave
        elif options.external_memory_system:
            system.cpu[i].connectUncachedPorts(system.membus)
        else:
            system.cpu[i].connectAllPorts(system.membus)

    return system

# ExternalSlave provides a "port", but when that port connects to a cache,
# the connecting CPU SimObject wants to refer to its "cpu_side".
# The 'ExternalCache' class provides this adaptation by rewriting the name,
# eliminating distracting changes elsewhere in the config code.
class ExternalCache(ExternalSlave):
    def __getattr__(cls, attr):
        if (attr == "cpu_side"):
            attr = "port"
        return super(ExternalSlave, cls).__getattr__(attr)

    def __setattr__(cls, attr, value):
        if (attr == "cpu_side"):
            attr = "port"
        return super(ExternalSlave, cls).__setattr__(attr, value)

def ExternalCacheFactory(port_type):
    def make(name):
        return ExternalCache(port_data=name, port_type=port_type,
                             addr_ranges=[AllMemory])
    return make

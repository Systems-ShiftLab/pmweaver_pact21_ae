#! /usr/bin/env python3

from collections import namedtuple
from datetime import datetime
from typing import List, Dict, Set

import multiprocessing as mp
import os
import subprocess
import sys

GEM5_PATH = "/pmweaver_ae/gem5"
# GEM5_PATH = "/home/smahar/git/gem5-pmdk/gem5"
GEM5_MT_PATH = "/home/smahar/git/gem5-pmdk-mt/gem5"
GEM5_BIN  = "build/X86/gem5.opt"

GEM5_CFG  = GEM5_PATH + "/configs/example/se.py"
GEM5_MT_CFG  = GEM5_MT_PATH + "/configs/example/se.py"
GEM5_PARAMS = ""

# PMDK_EX = "/temp/gem5-pmdk/pmdk/src/examples/libpmemobj"
PMDK_EX = "/pmweaver_ae/pmdk/src/examples/libpmemobj"

HW_THREADS = mp.cpu_count()

ITER = 512

GEM5_ARGS = [(GEM5_PATH + "/" + GEM5_BIN), GEM5_CFG, "--cpu-clock=3GHz", "--mem-size=8GB", "--mem-type=DDR4_2400_8x8", "--l2cache", "--l3cache", "--cpu-type=DerivO3CPU", "--caches", "--l1i_size=32kB", "--l1d_size=32kB", "--l2_size=256kB", "--l3_size=2MB"]
# GEM5_ARGS = [(GEM5_PATH + "/" + GEM5_BIN), GEM5_CFG, "--cpu-clock=3GHz", "--mem-size=8GB", "--mem-type=DDR4_2400_8x8", "--l2cache", "--l3cache", "--cpu-type=DerivO3CPU", "--caches", "--l1i_size=32kB", "--l1d_size=32kB", "--l2_size=128kB", "--l3_size=2MB"]
GEM5_MT_ARGS = [(GEM5_MT_PATH + "/" + GEM5_BIN), GEM5_MT_CFG, "--cpu-clock=4GHz", "--mem-size=8GB", "--mem-type=DDR4_2400_8x8", "--l2cache", "--cpu-type=DerivO3CPU", "--caches", "--l1i_size=32kB", "--l1d_size=64kB", "--l2_size=4MB"]

MT_FWD_ARGS = {1:'-F11000000', 2:'-F10311500', 4:'-F11311500', 8:'-F10000'}

Workload = namedtuple("Workload", "name binary arguments stdFwd")

ExecResult = namedtuple('ExecResult', 'stdout')

def exec_shell(cmd: List[str], get_output: bool = False, stdout: str = "/dev/null", env={}, cwd=None, dry_run=False) -> int:
    result = ExecResult('')
    
    if dry_run:
        print("Exec: ", ' '.join(cmd), " with env: ", str(env))
    else:
        if cwd == None:
            result = subprocess.run(cmd, env=env)
        else:
            try:
                os.makedirs(cwd, exist_ok=True)
            except OSError:
                print("Unable to create %s" % cwd)
        
            print('env: ' + str(env))
            result = subprocess.run(cmd, env=env, cwd=cwd, stdout=subprocess.PIPE, universal_newlines=True)
            print("Got output = ", result.stdout.strip())

    return result

def create_workload_dir(workload: Workload, run_name: str, eval_type: str="") -> str:
    if eval_type != "":
        eval_type = eval_type + '/'
        
    dir_path = "/tmp/workload_ae/" + eval_type + run_name + "/" + workload.name
    try:
        os.mkdir(dir_path)
    except OSError:
        print ("Creation of the directory %s failed" % dir_path)
    return dir_path

def create_gem5_workloads() -> None:
    exec_shell(cmd=['zsh', '-c', "make -C /home/smahar/git/gem5-pmdk-mt/pmdk/src/examples/libpmemobj clean -j100; make -C /home/smahar/git/gem5-pmdk/pmdk/src/examples/libpmemobj -j100 EXTRA_CFLAGS+=-DGEM5"])    

def get_gem5_cmd(run_name: str, workload: Workload, cpucount:int = 1, gem5_args=GEM5_ARGS, stdFwd: bool = False) -> List[str]:
    gem5_args_adj = [elem for elem in gem5_args]
    
    if stdFwd:
        gem5_args_adj.append("-F11000000")

    # tracing = ['--debug-flags=ExecEnable,ExecAll', '--debug-file=/nvme/smahar/smahar-from-08/git/instruction_traces_pmweaver/' + workload.name]
    tracing = []
    gem5_args_adj = [gem5_args_adj[0]] + tracing + gem5_args_adj[1:]

    return ['tsp', '-L', run_name] + gem5_args_adj + ['-c', workload.binary, '-e', '/pmweaver_ae/gem5/env', '-o', workload.arguments + ' 0', '-n', str(cpucount)]

""" Returns a multithreading version of the command """
def get_gem5_mt_cmd(run_name: str, workload: Workload, cpucount:int = 1, gem5_args=GEM5_MT_ARGS, stdFwd: bool = False) -> List[str]:
    gem5_args_adj = []

    for elem in gem5_args:
        if '4MB' in elem:
            elem = '--l2_size=' + str(4*cpucount) + 'MB'
        gem5_args_adj.append(elem) 
    
    workload_bin = str(workload.binary + ';')*cpucount
    # Remove the extra semicolon at the end
    workoad_bin = workload_bin[:-1]

    workload_args = ""
    for i in range(cpucount):
        workload_args += workload.arguments  + ' ' + str(i)
        if i != cpucount-1: # Don't add extra semicolon at the end
            workload_args += ';'

    fwd_args = []
    if stdFwd:
        fwd_args = [ MT_FWD_ARGS[cpucount] ]
    if workload.name == "tatp_nvm" and cpucount > 1:
        fwd_args = [ '-F9872037' ]
    if workload.name == "tpcc_nvm" and cpucount > 1:
        fwd_args = [ '-F9872037' ]
        
    return ['tsp', '-L', run_name] + gem5_args_adj + fwd_args + ['-c', workload_bin, '-o', workload_args, '-n', str(cpucount)]

def set_threads(thread_cnt: int) -> None:
    exec_shell(cmd=(('tsp -S ' + str(thread_cnt)).split()))

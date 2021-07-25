#!/usr/bin/env python3

"""
Run predictor with performance gains from address and data selectively disabled 
"""

from common import *

JANUS_WORKLOAD = "/pmweaver_ae/janus_workload"

WORKLOADS: List[Workload] = [
    Workload('btree',           PMDK_EX + '/map/data_store',                        'btree /mnt/pmem0/data_gen_btree '                      + str(ITER), True),
    Workload('ctree',           PMDK_EX + '/map/data_store',                        'ctree /mnt/pmem0/data_gen_ctree '                      + str(ITER), True),
    Workload('rbtree',          PMDK_EX + '/map/data_store',                        'rbtree /mnt/pmem0/data_gen_rbtree '                    + str(ITER), True),
    Workload('hashmap_tx',      PMDK_EX + '/map/data_store',                        'hashmap_tx /mnt/pmem0/data_gen_hashmap_tx '            + str(ITER), True),
    Workload('skiplist',        PMDK_EX + '/map/data_store',                        'skiplist /mnt/pmem0/data_gen_skiplist '                + str(ITER), True),
    Workload('hashmap_ll',      JANUS_WORKLOAD + '/hash/singly_linked_hash',       '', False),
    Workload('arr_swap',        JANUS_WORKLOAD + '/arr_swap/arr_swap',              '', False),
    Workload('queue',           JANUS_WORKLOAD + '/queue/queue',                    '', False),
    Workload('tatp_nvm',        JANUS_WORKLOAD + '/TATP/tatp_nvm',                  '', False),
    Workload('tpcc_nvm',        JANUS_WORKLOAD + '/TPCC/tpcc_nvm',                  '', False),
]


default_env = {
    "DISABLE_CONFIDENCE":"1", 
    "STALE_ENTRY_AGE_THRESHOLD":"8",
    "PATH_HISTORY_SIZE": "32",
    "DISABLE_PER_PC_CONFIDENCE":"1",
    "DISABLE_FANCY_ADDR_PRED": "1",
}

def execute_workloads(run_name: str, workloads: List[Workload], env: Dict = {}, directory: str = "comparison_workload_results") -> None:
    for workload in workloads:
        cwd = create_workload_dir(workload, run_name, 'run_comparison_addr_data_disable')
        print("Currently on the workload", workload.name, "stored at", cwd)
        stdFwd = workload.stdFwd
        exec_shell(get_gem5_cmd(run_name=run_name, workload=workload, cpucount=1, stdFwd=stdFwd), env=env, cwd=cwd)

def run_bmo_predictor_first(workloads: List[Workload]) -> None:
    env = dict(default_env)
    env["ENABLE_DW"] = "1"
    env["USE_PREDICTOR"] = "1"

    execute_workloads('with-dw-predictor', workloads, env, 'with-dw-predictor')

def run_bmo_predictor_second(workloads: List[Workload]) -> None:
    env = dict(default_env)
    env["ENABLE_EV"] = "1"
    env["USE_PREDICTOR"] = "1"

    execute_workloads('with-ev-predictor', workloads, env, 'with-ev-predictor')

def run_bmo_first(workloads: List[Workload]) -> None:
    env = dict(default_env)
    env["ENABLE_DW"] = "1"
    env["USE_PREDICTOR"] = "0"

    execute_workloads('with-dw', workloads, env, 'with-dw')

def run_bmo_second(workloads: List[Workload]) -> None:
    env = dict(default_env)
    env["ENABLE_EV"] = "1"
    env["USE_PREDICTOR"] = "0"

    execute_workloads('with-ev', workloads, env, 'with-ev')

def run_data_disabled_first(workloads: List[Workload]) -> None:
    env = dict(default_env)
    env["ENABLE_DW"] = "1"
    env["USE_PREDICTOR"] = "1"
    env["DISABLE_DATA_PRED_PERF"] = "1"

    execute_workloads('with-dw-data-disabled', workloads, env, 'with-dw-data-disabled')

def run_data_disabled_second(workloads: List[Workload]) -> None:
    env = dict(default_env)
    env["ENABLE_EV"] = "1"
    env["USE_PREDICTOR"] = "1"
    env["DISABLE_DATA_PRED_PERF"] = "1"

    execute_workloads('with-ev-data-disabled', workloads, env, 'with-ev-data-disabled')

def main() -> None:
    set_threads(HW_THREADS)

    run_bmo_first(WORKLOADS)
    run_bmo_second(WORKLOADS)
    
    run_bmo_predictor_first(WORKLOADS)
    run_bmo_predictor_second(WORKLOADS)
    
    run_data_disabled_first(WORKLOADS)
    run_data_disabled_second(WORKLOADS)

if __name__ == '__main__':
    main()

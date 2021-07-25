#!/usr/bin/env python3

from common import *

disable_fig_save = False 
save_loc = 'plots'
save_prefix="ae"

config_common(disable_fig_save=disable_fig_save, save_loc=save_loc, save_prefix=save_prefix)

import matplotlib.pyplot as plt
import numpy as np
import os
import pandas as pd
import re
import warnings

from os import path
from typing import Dict, List, Set

import matplotlib.ticker as mtick
import matplotlib as mpl
from scipy.stats.mstats import gmean
from matplotlib.ticker import (MultipleLocator, FormatStrFormatter,
                               AutoMinorLocator, ScalarFormatter, LogLocator)
import matplotlib.patches as patches
import matplotlib.patches as mpatches
from matplotlib import rc

import seaborn as sns

init_notebook()
figsize = (7,1.65)
fontsize = 11

RESULT_LOC = ["/tmp/workload_ae/run_comparison/with-ev", 
              "/tmp/workload_ae/run_comparison/with-dw", 
              "/tmp/workload_ae/run_comparison/without-bmo-or-predictor", 
              "/tmp/workload_ae/run_comparison/with-dw-predictor",
              "/tmp/workload_ae/run_comparison/with-ev-predictor"]

################################################################################
## Setup
################################################################################

print("Found following workloads: ")

categories = []
for loc in RESULT_LOC:
    workloads = [dI for dI in os.listdir(loc) if os.path.isdir(os.path.join(loc,dI))]
    category = loc.split('/')[-1]
    categories.append(category)
    print("\t",category,":",workloads)

functioning_workloads = {}
idx = 0
for category in categories:
    result_to_append = []
    loc = RESULT_LOC[idx]
    for workload in workloads:
        stats_path = loc + "/" + workload + "/m5out/stats.txt"
        stats_size = os.path.getsize(stats_path)
        if stats_size != 0:
            result_to_append.append(workload)
        else:
            warnings.warn(category + ":" + workload + " does not have a stats file")
    idx += 1
    functioning_workloads[category] = result_to_append

stats = {}
prog = 0
idx = 0
total = len(functioning_workloads) * len(list(functioning_workloads.items())[0][1])

for category in functioning_workloads:
    loc = RESULT_LOC[idx]
    stats[category] = {}
    for workload in functioning_workloads[category]:
        prog += 1
        print("\rProcessing " + str(prog) + " of " + str(total), end="", flush=True)
        
        cur_stat = stats_to_dict(os.path.join(loc, workload, "m5out/stats.txt"))
        stats[category][workload] = cur_stat
    idx += 1

print("")

exec_time                    = {}
avg_bmo                      = {}
bmoFinishDist                = {}
bmoFinishDistMin             = {}
bmoFinishDistMax             = {}
correctlyPredictedPWrites    = {}
totalPWrites                 = {}
coverage                     = {}
addr_coverage                = {}
predictedWriteCount          = {}
correctlyPredictedFreeWrites = {} # system.pb.correctlyPredictedFreeWrites
cacheLineNotInWHB            = {}
accuracyCorrect              = {}
pmWriteMatchDistance         = {}
addrPmWriteMatchDistance     = {}
pfWritebackDistMap           = {}
pbWritebackDistMap           = {}
pfWritebackDistMapMicro      = {}
pbWritebackDistMapMicro      = {}
bmoLatency                   = {}
timelinessCtrl               = {}
addrChangesBwClwb            = {}
writeAmplification           = {}
capacityEviction             = {}
whbAddrPred                  = {}
whbDataPred                  = {}
whbBothAddrDataPred          = {}
clwbCount                    = {}
freeAddrPrediction           = {}
sim_insts                    = {}
pmStoresPF                   = {}
bmoFinishAfter               = {}
bmoFinishBefore              = {}
invalidatedPredictions       = {}
addrNotPredictedPb           = {}
untimelyPrediction           = {}

for category in stats:
    exec_time[category]                    = {}
    avg_bmo[category]                      = {}
    bmoFinishDist[category]                = {}
    correctlyPredictedPWrites[category]    = {}
    totalPWrites[category]                 = {}
    coverage[category]                     = {}
    addr_coverage[category]                = {}
    predictedWriteCount[category]          = {}
    correctlyPredictedFreeWrites[category] = {}
    cacheLineNotInWHB[category]            = {}
    accuracyCorrect[category]              = {}
    pmWriteMatchDistance[category]         = {}
    addrPmWriteMatchDistance[category]     = {}
    bmoLatency[category]                   = {}
    bmoFinishDistMin[category]             = {}
    bmoFinishDistMax[category]             = {}
    timelinessCtrl[category]               = {}
    addrChangesBwClwb[category]            = {}
    writeAmplification[category]           = {}
    capacityEviction[category]             = {}
    bmoFinishAfter[category]               = {}
    bmoFinishBefore[category]              = {}
    addrNotPredictedPb[category]           = {}
    
    whbAddrPred[category]                  = {}
    whbDataPred[category]                  = {}
    whbBothAddrDataPred[category]          = {}
    clwbCount[category]                    = {}
    freeAddrPrediction[category]           = {}
    sim_insts[category]                    = {}
    pmStoresPF[category]                   = {}
    invalidatedPredictions[category]       = {}
    untimelyPrediction[category]           = {}
    
    for workload in stats[category]:
        clwbCount[category][workload] = float(stats[category][workload]['system.cpu.pf.clwbCountInclInv'])
        exec_time[category][workload] = float(stats[category][workload]['system.work_item_type0::mean'])
        
        addrChangesBwClwb[category][workload] = float(stats[category][workload]['system.cpu.pf.consecutiveWritesToCL::mean'])
        
        avg_bmo[category][workload] = float(stats[category][workload]['system.tol2bus.avgBmoLatency::mean'])
        avg_bmo[category][workload] = float(stats[category][workload]['system.tol2bus.avgBmoLatency::mean'])
        
        bmoFinishDist[category][workload] = float(stats[category][workload]['system.mem_ctrls1.bmoFinishDist::mean'])
        bmoFinishDistMin[category][workload] = np.abs(float(stats[category][workload]['system.mem_ctrls1.bmoFinishDist::min_value'])-float(stats[category][workload]['system.mem_ctrls1.bmoFinishDist::mean']))/1000
        bmoFinishDistMax[category][workload] = np.abs(float(stats[category][workload]['system.mem_ctrls1.bmoFinishDist::max_value'])-float(stats[category][workload]['system.mem_ctrls1.bmoFinishDist::mean']))/1000
        
        pmStoresPF[category][workload] = float(stats[category][workload]['system.cpu.pf.pmStores'])
        
        bmoFinishAfter[category][workload] = float(stats[category][workload]['system.mem_ctrls1.bmoFinishAfter'])
        bmoFinishBefore[category][workload] = float(stats[category][workload]['system.mem_ctrls1.bmoFinishBefore'])

        correctlyPredictedPWrites[category][workload] = float(stats[category][workload]['system.pb.correctlyPredictedPWrites'])
        totalPWrites[category][workload] = float(stats[category][workload]['system.pb.totalPWrites'])
        if totalPWrites[category][workload] == 0:
            totalPWrites[category][workload] = 1
        coverage[category][workload] = correctlyPredictedPWrites[category][workload]/totalPWrites[category][workload]
        addr_coverage[category][workload] = float(stats[category][workload]['system.pb.addrPredictions'])/totalPWrites[category][workload]
        
        predictions = float(stats[category][workload]['system.cpu.pf.predictedWriteCount'])
        correctFreePredictions = float(stats[category][workload]['system.pb.correctlyPredictedFreeWrites'])

        predictedWriteCount[category][workload] = predictions/totalPWrites[category][workload]
        correctlyPredictedFreeWrites[category][workload] = correctFreePredictions/totalPWrites[category][workload]
        
        cacheLineNotInWHB[category][workload] = float(stats[category][workload]['system.cpu.pf.cacheLineNotInWHB'])/totalPWrites[category][workload]
        
        correctPredictions = correctlyPredictedPWrites[category][workload] if correctlyPredictedPWrites[category][workload] != 0 else 1
        accuracyCorrect[category][workload] = predictions/correctPredictions
        
        pmWriteMatchDistance[category][workload] = float(stats[category][workload]['system.pb.pmWriteMatchDistance::mean'])
        addrPmWriteMatchDistance[category][workload] = float(stats[category][workload]['system.pb.pmWriteMatchDistance::mean'])

        sim_insts[category][workload] = float(stats[category][workload]['sim_insts'])

        bmoLatency[category][workload] = {}
        
        invalidatedPredictions[category][workload] = float(stats[category][workload]['system.pb.invalidatedPredictions'])
        untimelyPrediction[category][workload] = float(stats[category][workload]['system.mem_ctrls1.untimelyPrediction'])
        
        pfWritebackDistMap[workload] = {}
        pbWritebackDistMap[workload] = {}
        pfWritebackDistMapMicro[workload] = {}
        pbWritebackDistMapMicro[workload] = {}
        
        addrNotPredictedPb[category][workload] = float(stats[category][workload]['system.mem_ctrls1.addrNotPredicted'])
        
        bmoLatencyTotal = float(stats[category][workload]['system.mem_ctrls1.bmoFinishDist::samples'])
        
        pfWritebackTotal = float(stats[category][workload]['system.cpu.pf.writebackDistStat::samples'])
        pbWritebackTotal = float(stats[category][workload]['system.pb.writebackDistStat::samples'])
        
        timelinessCtrl[category][workload] = float(stats[category][workload]['system.mem_ctrls1.timeliness::mean'])
        if bmoLatencyTotal == 0:
            bmoLatencyTotal = 1
        if pfWritebackTotal == 0:
            pfWritebackTotal = 1
        if pbWritebackTotal == 0:
            pbWritebackTotal = 1
        
        pfWritebackDistMap[workload][1] = 0.0
        pbWritebackDistMap[workload][1] = 0.0
                
        bucket_size = 100
        for i in range(0,100000-1):
            pfWritebackDistMap[workload][(i+1)*bucket_size-1] = float(stats[category][workload]['system.cpu.pf.writebackDistStat::' + str(i*bucket_size) + '-' + str((i+1)*bucket_size-1)])/pfWritebackTotal*100
            pbWritebackDistMap[workload][(i+1)*bucket_size-1] = float(stats[category][workload]['system.pb.writebackDistStat::' + str(i*bucket_size) + '-' + str((i+1)*bucket_size-1)])/pbWritebackTotal*100
            
        bucket_size = 100
        for i in range(0,100):
            bmoLatency[category][workload][(i+1)*bucket_size-1] = float(stats[category][workload]['system.mem_ctrls1.bmoFinishDist::' + str(i*bucket_size) + '-' + str((i+1)*bucket_size-1)])/bmoLatencyTotal*100
            
        for i in range(0,1000-1):
            pfWritebackDistMap[workload][i] = float(stats[category][workload]['system.cpu.pf.writebackDistStatMicro::' + str(i)])/pfWritebackTotal*100
            pbWritebackDistMap[workload][i] = float(stats[category][workload]['system.pb.writebackDistStatMicro::' + str(i)])/pbWritebackTotal*100
        
        capacityEviction[category][workload] = float(stats[category][workload]['system.pb.resultBufferCapacityEvictions'])

predicted_df = pd.DataFrame(bmoFinishAfter) + pd.DataFrame(bmoFinishBefore)

bmoFinishAfter_df = (pd.DataFrame(bmoFinishAfter)/predicted_df)
bmoFinishBefore_df = (pd.DataFrame(bmoFinishBefore)/predicted_df)
invalidatedPredictions_df = (pd.DataFrame(invalidatedPredictions)/predicted_df)
untimelyPrediction_df = pd.DataFrame(untimelyPrediction)/predicted_df

bmoBreakdownEV_df = pd.DataFrame()
bmoBreakdownEV_df['Untimely Partial BMOs'] = untimelyPrediction_df['with-ev-predictor']
bmoBreakdownEV_df['Timely Partial BMOs'] = bmoFinishAfter_df['with-ev-predictor'] - untimelyPrediction_df['with-ev-predictor']
bmoBreakdownEV_df['Complete BMOs'] = bmoFinishBefore_df['with-ev-predictor']
bmoBreakdownEV_df['Invalidations'] = invalidatedPredictions_df['with-ev-predictor']
print(bmoBreakdownEV_df)

bmoBreakdownDW_df = pd.DataFrame()
# bmoBreakdown_df['Not predicted'] = addrNotPredictedPb_df['with-ev-predictor']
bmoBreakdownDW_df['Untimely Partial BMOs'] = untimelyPrediction_df['with-dw-predictor']
bmoBreakdownDW_df['Timely Partial BMOs'] = bmoFinishAfter_df['with-dw-predictor'] - untimelyPrediction_df['with-dw-predictor']
bmoBreakdownDW_df['Complete BMOs'] = bmoFinishBefore_df['with-dw-predictor']
bmoBreakdownDW_df['Invalidations'] = invalidatedPredictions_df['with-dw-predictor']
print(bmoBreakdownDW_df)

bmoBreakdownFmtEV_df = bmoBreakdownEV_df.T.drop('Invalidations').T
bmoBreakdownFmtDW_df = bmoBreakdownDW_df.T.drop('Invalidations').T

bmoBreakdownFmtEV_df = bmoBreakdownFmtEV_df.T
bmoBreakdownFmtEV_df['am'] = [
    np.mean(bmoBreakdownFmtEV_df.T['Untimely Partial BMOs']), 
    np.mean(bmoBreakdownFmtEV_df.T['Timely Partial BMOs']),
    np.mean(bmoBreakdownFmtEV_df.T['Complete BMOs']), 
]
bmoBreakdownFmtEV_df = bmoBreakdownFmtEV_df.T

bmoBreakdownFmtDW_df = bmoBreakdownFmtDW_df.T
bmoBreakdownFmtDW_df['am'] = [
    np.mean(bmoBreakdownFmtDW_df.T['Untimely Partial BMOs']), 
    np.mean(bmoBreakdownFmtDW_df.T['Timely Partial BMOs']),
    np.mean(bmoBreakdownFmtDW_df.T['Complete BMOs']), 
]
bmoBreakdownFmtDW_df = bmoBreakdownFmtDW_df.T

bmoBreakdownFmtDW_df = bmoBreakdownFmtDW_df.rename(columns={
    'Untimely Partial BMOs': 'Not precomp. (Dw)',
    'Timely Partial BMOs': 'Partially precomp.',
    'Complete BMOs': 'Completely precomp. (DW)',
})

bmoBreakdownFmtEV_df = bmoBreakdownFmtEV_df.rename(columns={
    'Untimely Partial BMOs': 'Not precomp.',
    'Timely Partial BMOs': 'Partially precomp.',
    'Complete BMOs': 'Completely precomp.',
})

color=['#808080', '#C0C0C0', 'white'][::-1]

ax = fix_names(bmoBreakdownFmtEV_df).plot.bar(
    zorder=3, 
    edgecolor='black', 
    color=color,
    stacked=True, 
    figsize=figsize,
    fontsize=1,
    width=0.25,
    position=1,
)

ax = fix_names(bmoBreakdownFmtDW_df).plot.bar(
    zorder=3, 
    edgecolor='black', 
    color=color,
    stacked=True, 
    figsize=figsize,
    fontsize=1,
    ax=ax,
    hatch='///',
    width=0.25,
    position=0,
)

ax.minorticks_on()
ax.grid(linestyle='-', zorder=0, axis='y', which='major')


box = ax.get_position()
set_ax_font(ax, fontsize)
ax.set_position([box.x0, box.y0, box.width, box.height * 0.8])


handles, labels = ax.get_legend_handles_labels()

ax.legend(
    loc='upper center', 
    bbox_to_anchor=(0.46, 1.6), 
    ncol=3,
    facecolor='white', 
    framealpha=0, 
    edgecolor='black', 
    fancybox=False,
    fontsize=fontsize-0.5,
    handles=handles[:3],
)

ax.yaxis.set_ticks(np.arange(0, 1.01, 0.25))
ax.set_xticklabels(ax.get_xticklabels(), rotation=40, ha='right')
plt.axvline(x=9.5, color='k', ls='--', lw=1)
ax.set_xlim(-0.5,10.5)

ax.set_ylim(0,1)
_ = ax.axes.set_ylabel('% predicted writes     ', fontweight='bold', fontsize=fontsize-1)

ax.arrow(1, 0, 0, -1, length_includes_head=True,
          head_width=1, head_length=1)

annotation_props = {'arrowprops': dict(arrowstyle="-|>", ec='black', color='black'), 'fontsize': fontsize, 'zorder': 10, 'fontweight': 'bold', 'color': 'black'}
plt.annotate("Enc+Veri", xy=(-0.08, 0.98), xytext=(-1.65, 1.15), **annotation_props)
plt.annotate("Dedup", xy=(0.08, 0.98), xytext=(0.3, 1.15), **annotation_props)

#save_fig('bmo_completion_breakdown')

indices = [key for key in categories]
print (indices)
df = pd.DataFrame(exec_time)

norm_df = pd.DataFrame()
for i in df:
    norm_df[i] = (df[i]/df['without-bmo-or-predictor'])
#norm_df

red_norm_df = norm_df[['with-ev', 'with-ev-predictor', 'with-dw', 'with-dw-predictor']]

ev_df = red_norm_df.T
ev_df['gm'] = [
    gmean(ev_df.T['with-ev']), 
    gmean(ev_df.T['with-ev-predictor']), 
    gmean(ev_df.T['with-dw']),
    gmean(ev_df.T['with-dw-predictor']),
]
ev_df = ev_df.T

ev_df = fix_names(ev_df)
print(ev_df)
ax = ev_df.plot.bar(
    zorder=3, 
    edgecolor='black', 
    cmap='Greys', 
    stacked=False, 
    figsize=figsize,
    fontsize=fontsize,
    width=0.7
)

_ = ax.axes.set_ylabel('Execution Time\nNorm. to Original', fontweight='bold', fontsize=fontsize)

ax.set_xticklabels(ax.get_xticklabels(), rotation=40, ha='right')

ulim = 7
ax.yaxis.set_ticks(np.arange(0, ulim+0.1, 1))
ax.set_ylim(0, ulim)

ax.minorticks_on()
ax.grid(linestyle='-', zorder=0, axis='y', which='major')
set_ax_font(ax, fontsize)
ax.legend(
    loc='upper center', 
    bbox_to_anchor=(0.465, 1.35),
    ncol=5,
    facecolor='white', 
#     framealpha=0, 
    edgecolor='white', 
    fancybox=False,
    fontsize=fontsize-2
)

plt.axvline(x=9.5, color='k', ls='--', lw=0.5)

save_fig('fig_18')

intr_norm_df = norm_df[['with-ev', 'without-bmo-or-predictor']]

intr_df = intr_norm_df.T
intr_df['gm'] = [
    gmean(intr_df.T['with-ev']), 
    gmean(intr_df.T['without-bmo-or-predictor']), 
]
intr_df = intr_df.T

intr_df = fix_names(intr_df)

ren = {'Enc+Veri': 'Encryption+Verification'}
intr_df = intr_df.rename(ren)
intr_df = intr_df.rename(ren, axis='columns')

print(intr_df)
ax = intr_df.plot.bar(
    zorder=3, 
    edgecolor='black', 
    cmap='Greys', 
    stacked=False, 
    figsize=(7,1.2),
    fontsize=fontsize
)

_ = ax.axes.set_ylabel('Execution Time\nNorm. to Orignal', fontweight='bold')

ax.set_xticklabels(ax.get_xticklabels(), rotation=40, ha='right')

ulim = 8
ax.yaxis.set_ticks(np.arange(0, ulim+0.1, 2))
ax.set_ylim(0, ulim)

# ax.minorticks_on()
ax.grid(linestyle='-', zorder=0, axis='y', which='major')
set_ax_font(ax, fontsize)
ax.legend(
    loc='upper center', 
    bbox_to_anchor=(0.48, 1.45),
    ncol=5,
    facecolor='white', 
    edgecolor='white', 
    fancybox=False,
    fontsize=fontsize+1
)

plt.axvline(x=9.5, color='k', ls='--', lw=0.5)

cat_map_2 = {
    'with-dw'                  : 'Dedup',
    'with-ev'                  : 'Enc+Veri',     
    'with-dw-predictor'        : 'Dedup w/ PMWeaver',
    'with-ev-predictor'        : 'Enc+Veri w/ PMWeaver',
    'without-bmo-or-predictor' : 'Orignal System',
    'btree'                    : 'B-Tree',
    'skiplist'                 : 'Skiplist',
    'tatp_nvm'                 : 'TATP',
    'hashmap_tx'               : 'Hashmap TX',
    'ctree'                    : 'C-Tree',
    'fifo_bulk'                : 'Linked List',
    'hashmap_atomic'           : 'Hashmap Atomic',
    'tpcc_nvm'                 : 'TPCC',
    'rbtree'                   : 'RB-Tree',
    'arr_swap'                 : 'Array Swap',
    'queue'                    : 'Linked List',
    'gm'                       : 'Geo Mean',
    'am'                       : 'AM',
}

save_fig('fig_1')

temp = pd.DataFrame(norm_df)

temp = temp.T
temp['gm'] = [
    gmean(temp.T['with-ev']), 
    gmean(temp.T['with-ev-predictor']), 
    gmean(temp.T['with-dw']),
    gmean(temp.T['with-dw-predictor']),
    gmean(temp.T['without-bmo-or-predictor']),
]
temp = temp.T
temp

speedup_df = pd.DataFrame({
    'with-ev':norm_df['with-ev']/norm_df['with-ev-predictor'], 
    'with-dw':norm_df['with-dw']/norm_df['with-dw-predictor'], 
})
speedup_df = speedup_df.T
speedup_df['gm'] = [gmean(speedup_df.T['with-ev']), gmean(speedup_df.T['with-dw'])]
speedup_df = speedup_df.T

print(speedup_df)

ax = fix_names(speedup_df).plot.bar(
    zorder=3, 
    edgecolor='black', 
    cmap='Greys', 
    stacked=False, 
    figsize=figsize,
    fontsize=1
)

ax.minorticks_on()
ax.grid(linestyle='-', zorder=0, axis='y', which='major')
# ax.grid(linestyle='--', alpha=0.8, zorder=0, axis='y', which='minor')


box = ax.get_position()
set_ax_font(ax, fontsize)
ax.set_position([box.x0, box.y0, box.width, box.height * 0.8])
ax.legend(
    loc='upper center', 
    bbox_to_anchor=(0.48, 1.35), 
    ncol=5,
    facecolor='white', 
    framealpha=0, 
    edgecolor='black', 
    fancybox=False,
    fontsize=fontsize-0.5
)

ax.yaxis.set_ticks(np.arange(0, 2.7, 1))
ax.set_xticklabels(ax.get_xticklabels(), rotation=40, ha='right')
plt.axvline(x=9.5, color='k', ls='--', lw=0.5)

_ = ax.axes.set_ylabel('Speedup over \n Designs with BMO', fontweight='bold', fontsize=fontsize-1)

#save_fig('perf_impr')

from io import StringIO

contents = StringIO("""Encr+Veri PMWeaver,Encr+Veri Janus,_empty,Dedup PMWeaver,Dedup Janus
btree,1.368247,1.094054,,1.138948,1.068221
skiplist,1.353945,1.093322,,1.092576,1.061604
tatp_nvm,1.826777,1.248472,,1.467685,0.998733
hashmap_tx,1.380751,1.073633,,1.102982,1.054222
ctree,1.276776,1.089646,,1.109087,1.059451
arr_swap,2.267234,1.57229,,1.970681,1.501253
hashmap_ll,1.453554,1.337351,,1.027192,1.306687
queue,2.249967,1.355766,,1.403685,1.306121
tpcc_nvm,1.515527,1.792495,,1.251748,1.030174
rbtree,1.424133,1.031828,,1.09981,1.023772
gm,1.577575,1.248629,,1.242079,1.130787
""")

comparison_df = pd.read_csv(contents, sep=",")
ax = fix_names(comparison_df).plot.bar(
    legend=False, 
    color=['grey', 'grey', 'black', 'white', 'white'],
    fontsize=fontsize,
    figsize=figsize,
    edgecolor='black',
#     edgewidth=0.5,
    zorder=3,
    width=0.8,
)

bars = ax.patches
patterns = ['/', '|']  # set hatch patterns in the correct order
hatches = []  # list for hatches in the order of the bars
for h in patterns:  # loop over patterns to create bar-ordered hatches
    for i in range(int(len(bars) / len(patterns))):
        hatches.append(h)
cnt = 11
pattern = '////'
hatches = ['']*cnt + [pattern]*cnt + ['']*cnt + ['']*cnt + [pattern]*cnt
for bar, hatch in zip(bars, hatches):  # loop over bars and hatches to set hatches in correct order
    bar.set_hatch(hatch)

ax.minorticks_on()
ax.grid(linestyle='-', zorder=0, axis='y', which='major')

box = ax.get_position()
set_ax_font(ax, fontsize)
ax.set_position([box.x0, box.y0, box.width, box.height * 0.8])
ax.legend(
    loc='upper center', 
    bbox_to_anchor=(0.48, 1.6), 
    ncol=2,
    facecolor='white', 
    framealpha=0, 
    edgecolor='black', 
    fancybox=False,
    fontsize=fontsize
)

ax.yaxis.set_ticks(np.arange(0, 2.7, 1))
ax.set_xticklabels(ax.get_xticklabels(), rotation=40, ha='right')
plt.axvline(x=9.5, color='k', ls='--', lw=0.5)

_ = ax.axes.set_ylabel('Speedup over \n Designs with BMO', fontweight='bold', fontsize=fontsize-1)
objs = ax.get_children()
iter = 0
print(len(objs))
for obj in objs[22:33]:
    obj.set_width(0)
    iter+=1
for obj in objs[0:22]:
    obj.set_x(obj.get_x() + 0.03)
    iter+=1
for obj in objs[33:55]:
    obj.set_x(obj.get_x() - 0.03)
    iter+=1
    
#save_fig('pmweaver_vs_janus')

speedup_df = pd.DataFrame({
    'with-ev':norm_df['with-ev']/norm_df['with-ev-predictor'], 
    'with-dw':norm_df['with-dw']/norm_df['with-dw-predictor'], 
})
speedup_df = speedup_df.T
speedup_df['gm'] = [gmean(speedup_df.T['with-ev']), gmean(speedup_df.T['with-dw'])]
speedup_df = speedup_df.T

print(speedup_df)

ax = fix_names(speedup_df).plot.bar(
    zorder=3, 
    edgecolor='black', 
    cmap='Greys', 
    stacked=False, 
    figsize=figsize,
    fontsize=1
)

ax.minorticks_on()
ax.grid(linestyle='-', zorder=0, axis='y', which='major')
# ax.grid(linestyle='--', alpha=0.8, zorder=0, axis='y', which='minor')


box = ax.get_position()
set_ax_font(ax, fontsize)
ax.set_position([box.x0, box.y0, box.width, box.height * 0.8])
ax.legend(
    loc='upper center', 
    bbox_to_anchor=(0.48, 1.3), 
    ncol=5,
    facecolor='white', 
    framealpha=0, 
    edgecolor='black', 
    fancybox=False,
    fontsize=fontsize
)

ax.yaxis.set_ticks(np.arange(0, 2.7, 1))
ax.set_xticklabels(ax.get_xticklabels(), rotation=40, ha='right')
plt.axvline(x=9.5, color='k', ls='--', lw=0.5)

_ = ax.axes.set_ylabel('Speedup over \n Designs with BMO', fontweight='bold', fontsize=fontsize-1)

save_fig('perf_impr')

################################################################################
# BMO Completion Latency
################################################################################

bmoFinishDist_df = pd.DataFrame()
bmoFinishDist_df_temp = pd.DataFrame(bmoFinishDist)
bmoFinishDist_df_temp = bmoFinishDist_df_temp.T.drop('without-bmo-or-predictor').T
bmoFinishDist_df_temp = bmoFinishDist_df_temp[['with-ev', 'with-ev-predictor', 'with-dw', 'with-dw-predictor']]

bmoFinishDist_df = bmoFinishDist_df_temp

print(bmoFinishDist_df)

bmoFinishDist_df = bmoFinishDist_df.T
bmoFinishDist_df = bmoFinishDist_df.T
#bmoFinishDist_df

err = [
    [list(bmoFinishDistMin['with-ev'].values()          ) , list(bmoFinishDistMax['with-ev'].values())            ,], 
    [list(bmoFinishDistMin['with-ev-predictor'].values()) , list(bmoFinishDistMax['with-dw-predictor'].values())  ,], 
    [list(bmoFinishDistMin['with-dw'].values()          ) , list(bmoFinishDistMax['with-dw'].values())            ,], 
    [list(bmoFinishDistMin['with-dw-predictor'].values()) , list(bmoFinishDistMax['with-dw-predictor'].values())  ,], 
]
#print(str(np.array(err)))
#print('Shape:' + str(np.shape(err)))

# colors = ['#2874a6', '#85c1e9', '#b9770e', '#f8c471']
mpl.rcParams["errorbar.capsize"] = 2.0
# mpl.rcParams["errorbar.ecolor"] = 'blue'

bmoFinishDist_df = bmoFinishDist_df.T
bmoFinishDist_df['am'] = [
    np.mean(bmoFinishDist_df.T['with-ev']),
    np.mean(bmoFinishDist_df.T['with-ev-predictor']), 
    np.mean(bmoFinishDist_df.T['with-dw']),
    np.mean(bmoFinishDist_df.T['with-dw-predictor']), 
]
bmoFinishDist_df = bmoFinishDist_df.T

print(bmoFinishDist_df)

ax = fix_names(bmoFinishDist_df/1000).plot.bar(
    zorder=3, 
    edgecolor='black', 
    stacked=False,
    cmap='Greys',
    figsize=figsize,
    width=0.7,
#     yerr=err,
    ecolor='blue'
)

ax.minorticks_on()
ax.grid(linestyle='-', zorder=0, axis='y', which='major')

box = ax.get_position()
ax.set_position([box.x0, box.y0, box.width, box.height * 0.8])

set_ax_font(ax, fontsize)


ax.legend(
#     title='BMO type', 
    loc='upper center', 
    bbox_to_anchor=(0.48, 1.3), 
    ncol=4, 
    facecolor='white', 
    framealpha = 0, 
    edgecolor='black', 
    fancybox=False,
    fontsize=fontsize-2
)

ax.set_xticklabels(ax.get_xticklabels(), rotation=40, ha='right')
ax.yaxis.get_offset_text().set_fontsize(fontsize)
ax.yaxis.set_major_formatter(ScalarFormatter(useMathText=True))
plt.axvline(x=9.5, color='k', ls='--', lw=0.5)

# ax.yaxis.set_major_formatter(mtick.FormatStrFormatter('%.2e'))
ax.yaxis.set_ticks(np.arange(0, 601, 200))
plt.ticklabel_format(style='plain', axis='y', scilimits=(0,0))
_ = ax.axes.set_ylabel('Avg. PM Support \n Latency (ns)', fontweight='bold', fontsize=fontsize-1)

plt.text(7.09/10, 0.15, "%.0f" % (bmoFinishDist_df['with-ev-predictor']['tatp_nvm']/1000), fontsize=fontsize-3, transform=plt.gcf().transFigure, zorder=15)
plt.text(7.35/10, 0.15, "%.0f" % (bmoFinishDist_df['with-ev-predictor']['tatp_nvm']/1000), fontsize=fontsize-3, transform=plt.gcf().transFigure, zorder=15)

save_fig('fig_24')

################################################################################
## Timeliness
################################################################################
pmWriteMatchDistance_df = pd.DataFrame(pmWriteMatchDistance)[['with-ev-predictor','with-dw-predictor']]
pmWriteMatchDistance_df = pmWriteMatchDistance_df.T
pmWriteMatchDistance_df['am'] = [np.mean(pmWriteMatchDistance_df.T['with-ev-predictor']), np.mean(pmWriteMatchDistance_df.T['with-dw-predictor'])]
pmWriteMatchDistance_df = pmWriteMatchDistance_df.T

timelinessCtrl_df = pd.DataFrame(timelinessCtrl)[['with-ev-predictor','with-dw-predictor']]
timelinessCtrl_df = timelinessCtrl_df.T
timelinessCtrl_df['am'] = [np.mean(timelinessCtrl_df.T['with-ev-predictor']), np.mean(timelinessCtrl_df.T['with-dw-predictor'])]
timelinessCtrl_df = timelinessCtrl_df.T
print(timelinessCtrl_df)

ax = fix_names(timelinessCtrl_df).plot.bar(
    zorder=3, 
    edgecolor='black', 
    cmap='Greys', 
    stacked=False, 
    figsize=figsize
)
ax.set_yscale('log')

ax.minorticks_on()
ax.grid(linestyle='-', zorder=0, axis='y', which='major')
ax.grid(linestyle='--', zorder=0, axis='y', which='minor')
_ = ax.axes.set_ylabel('Time gap  before \n write-back (ns)', fontweight='bold')


box = ax.get_position()
ax.set_position([box.x0, box.y0, box.width, box.height * 0.8])


# _ = ax.set_title('Time Between Prediction and Write', 
#                  loc        = 'center', 
#                  y          = 1.2, 
#                  x          = 0.48)

labels = ax.set_xticklabels(ax.get_xticklabels(), rotation=40, ha='right')

lbl_pos = ax.xaxis.get_major_ticks()[-1].label.get_position()
x, y = lbl_pos
x+=100
ax.xaxis.get_major_ticks()[-1].label.set_position((x, y))
                                                
set_ax_font(ax, fontsize)

ax.yaxis.set_major_formatter(ScalarFormatter(useMathText=True))

# ax.yaxis.set_ticks(np.arange(0,ulim*1.2, ulim/5))
plt.ticklabel_format(style='sci', axis='y', scilimits=(0,0))

ax.legend(
          loc               = 'upper center', 
          bbox_to_anchor    = (0.48, 1.3), 
          ncol              = 7, 
          facecolor         = 'white', 
          framealpha        = 0, 
          edgecolor         = 'black', 
          fancybox          = False,
          fontsize          = fontsize
)

ulim = int(np.max(pmWriteMatchDistance_df.max()))
ax.yaxis.set_major_formatter(mtick.LogFormatterMathtext(base=10.0))

ax.set_ylim(1, 10**5)
ax.yaxis.set_ticks([10**i for i in range(0, 6)])
plt.axvline(x=9.5, color='k', ls='--', lw=0.5)

save_fig('fig_23')

################################################################################
## Coverage Breakdown
################################################################################
coverage_df = pd.DataFrame(coverage).T.drop('with-ev').drop('with-dw').T*100
coverage_df = coverage_df.T.drop('without-bmo-or-predictor').T
coverage_df = coverage_df[['with-ev-predictor', 'with-dw-predictor']]

coverage_df = coverage_df.T
coverage_df['am'] = [np.mean(coverage_df.T['with-ev-predictor']), np.mean(coverage_df.T['with-dw-predictor'])]
coverage_df = coverage_df.T

predictedWriteCount_df = pd.DataFrame(predictedWriteCount).T.drop('with-ev').drop('with-dw').T
predictedWriteCount_df = predictedWriteCount_df.T.drop('without-bmo-or-predictor').T
predictedWriteCount_df = predictedWriteCount_df[['with-ev-predictor', 'with-dw-predictor']]

predictedWriteCount_df = predictedWriteCount_df.T
predictedWriteCount_df['gm'] = [gmean(predictedWriteCount_df.T['with-ev-predictor']), gmean(predictedWriteCount_df.T['with-dw-predictor'])]
predictedWriteCount_df = predictedWriteCount_df.T

correctlyPredictedFreeWrites_df = pd.DataFrame(correctlyPredictedFreeWrites).T.drop('with-ev').drop('with-dw').T
correctlyPredictedFreeWrites_df = correctlyPredictedFreeWrites_df.T.drop('without-bmo-or-predictor').T
correctlyPredictedFreeWrites_df = correctlyPredictedFreeWrites_df[['with-ev-predictor', 'with-dw-predictor']]*100

correctlyPredictedFreeWrites_df = correctlyPredictedFreeWrites_df.T
correctlyPredictedFreeWrites_df['AM'] = [np.mean(correctlyPredictedFreeWrites_df.T['with-ev-predictor']), np.mean(correctlyPredictedFreeWrites_df.T['with-dw-predictor'])]
correctlyPredictedFreeWrites_df = correctlyPredictedFreeWrites_df.T

predictedWriteCount_am_df = pd.DataFrame(predictedWriteCount_df) #.drop('am')
predictedWriteCount_am_df = predictedWriteCount_am_df.T
predictedWriteCount_am_df['AM'] = [np.mean(predictedWriteCount_am_df.T['with-ev-predictor']), np.mean(predictedWriteCount_am_df.T['with-dw-predictor'])]
predictedWriteCount_am_df = predictedWriteCount_am_df.T

coverage_am_df = coverage_df.drop('am')
coverage_am_df = coverage_am_df.T
coverage_am_df['AM'] = [np.mean(coverage_am_df.T['with-ev-predictor']), np.mean(coverage_am_df.T['with-dw-predictor'])]
coverage_am_df = coverage_am_df.T

trad_pred_dw_name = 'Store-instruction-based (Dedup)'
free_pred_dw_name = 'Timeout-based (Dedup)'

trad_pred_ev_name = 'Store-instruction-based (Enc+Veri)'
free_pred_ev_name = 'Timeout-based (Enc+Veri)'

correctPredictions_dw = pd.DataFrame({
        free_pred_dw_name: correctlyPredictedFreeWrites_df['with-dw-predictor'],
        trad_pred_dw_name: coverage_df['with-dw-predictor']-correctlyPredictedFreeWrites_df['with-dw-predictor'],
})

# Add airthmetic mean
correctPredictions_dw = correctPredictions_dw.drop('am')
correctPredictions_dw = correctPredictions_dw.T
correctPredictions_dw['am'] = [np.mean(correctPredictions_dw.T[free_pred_dw_name]), np.mean(correctPredictions_dw.T[trad_pred_dw_name])]
correctPredictions_dw = correctPredictions_dw.T

# Sort it
correctPredictions_dw.sort_index(ascending=False)

correctPredictions_ev = pd.DataFrame({
        free_pred_ev_name: correctlyPredictedFreeWrites_df['with-ev-predictor'],
        trad_pred_ev_name: coverage_df['with-ev-predictor']-correctlyPredictedFreeWrites_df['with-ev-predictor'],
})

# Add airthmetic mean
correctPredictions_ev = correctPredictions_ev.drop('am')
correctPredictions_ev = correctPredictions_ev.T
correctPredictions_ev['am'] = [np.mean(correctPredictions_ev.T[free_pred_ev_name]), np.mean(correctPredictions_ev.T[trad_pred_ev_name])]
correctPredictions_ev = correctPredictions_ev.T

#Sort it
correctPredictions_ev = correctPredictions_ev.sort_index(ascending=False)
correctPredictions_dw = correctPredictions_dw.sort_index(ascending=False)

fig, ax = plt.subplots(figsize=figsize)
cmap = "Greys_r"

fix_names(correctPredictions_dw).plot.bar(
    zorder=3, 
    stacked=True, 
    width=0.3, 
    position=0, 
    color=[(0.4, 0.4, 0.4, 1), (0.4, 0.4, 0.4, 1)],
    ax=ax, 
    alpha=1, 
    edgecolor='black'
)

fix_names(correctPredictions_ev).plot.bar(
    zorder=3, 
    stacked=True, 
    width=0.3, 
    position=+1, 
    ax=ax, 
    color=[(1,1,1,1), (1,1,1,1)], 
    edgecolor="black",
)

pattern = '/////'
bars_dw = [rect for rect in ax.get_children() if isinstance(rect, mpl.patches.Rectangle)]
for bar in bars_dw:
    rounded_dw = [round(val,2) for val in correctPredictions_dw['Timeout-based (Dedup)']]
    rounded_ev = [round(val,2) for val in correctPredictions_ev['Timeout-based (Enc+Veri)']]
    rounded = rounded_dw + rounded_ev

    if round(bar.get_height(), 2) in rounded:
        bar.set_hatch(pattern)

ax.grid(linestyle='-', zorder=0, axis='y', which='major')

box = ax.get_position()
ax.set_position([box.x0, box.y0, box.width*1.2, box.height * 0.8])

# _ = ax.set_title('Data Coverage breakdown', loc='center', y=1.3, x=0.48)
_ = ax.axes.set_ylabel('Data Prediction \n Coverage', fontweight='bold')
ax.set_xlim(-0.5,10.5)
ax.set_ylim(0,100)
ax.yaxis.set_ticks(np.arange(0, 101, 20))

set_ax_font(ax, fontsize)

ax.legend(loc               = 'upper center', 
          bbox_to_anchor    = (0.450, 1.6), 
          ncol              = 2, 
          facecolor         = 'white', 
          framealpha        = 0, 
          edgecolor         = 'black', 
          fancybox          = False,
          fontsize          = fontsize
)


ax.set_xticklabels(ax.get_xticklabels(), rotation=40, ha='right', fontsize=fontsize+1)
ax.yaxis.set_major_formatter(mtick.PercentFormatter())
plt.axvline(x=9.5, color='k', ls='--', lw=0.5)


save_fig('fig_22')

################################################################################
## Time between CLWB and last write
################################################################################
print("Generating figure 8, this may take a few minutes...", flush=True)

x, y = figsize
figsize_dist = (x*1.25, y*1.2)

pfWritebackDistMap_df = pd.DataFrame(pfWritebackDistMap).sort_index()

pfWritebackDistMap_df = pfWritebackDistMap_df[[
    'btree','queue',
    'ctree', 'hashmap_ll', 
    'rbtree', 'arr_swap', 
    'skiplist', 'tatp_nvm', 
    'hashmap_tx', 'tpcc_nvm',]]

markers = [
    '<', 'o',
    '*', 's',
    'p', 'h',
    '>', '*',
    '|', '8',
]

linestyles = [
    '-',  '--',
    '-.',  '--', 
    ':',  ':', 
    '--', '-.', 
    '--', '-',
]

colors = ['#4169E1', '#D2691E']*5

fig = plt.figure(figsize=figsize_dist)
ax = plt.axes()

fixed_df = fix_names(pfWritebackDistMap_df, 2)

wrkld_iter = 0
for workload in fixed_df:
    plt.plot(
        fixed_df.cumsum()[workload].index, fixed_df.cumsum()[workload], 
        zorder=3,
        color=colors[wrkld_iter],
        linestyle=linestyles[wrkld_iter],
        marker=markers[wrkld_iter],
        label=str(workload),
        markevery=0.1,
    )
    wrkld_iter += 1

ax.minorticks_on()
_ = ax.axes.set_ylabel('% Write-backs', fontweight='bold', fontsize=fontsize+1)
_ = ax.axes.set_xlabel('Time between last store to a cache line and fence (ns)', fontweight='bold', fontsize=fontsize+1)


box = ax.get_position()
_ = ax.set_position([box.x0, box.y0, box.width, box.height * 0.8])


_ = set_ax_font(ax, fontsize+1)
plt.legend()

# where some data has already been plotted to ax
handles, labels = ax.get_legend_handles_labels()

# manually define a new patch 
lowLevel = mpatches.Patch(color='white', label=r'Low Level:')
transaction = mpatches.Patch(color='white', label=r'Transaction:')
# handles is a list, so append manual patch
handles.insert(0, transaction) 
handles.insert(1, lowLevel) 

l = ax.legend(
    loc='upper left',
    bbox_to_anchor=(-0.21, 1.50),
    ncol=6,
    facecolor='none',
    framealpha=0,
    edgecolor='black',
    fancybox=False,
    fontsize=fontsize-1,
    columnspacing=1,
    handles=handles,
)
l.set_zorder(1)

_ = ax.yaxis.set_major_formatter(mtick.PercentFormatter())
_ = ax.set_xscale('log')
_ = ax.set_xlim(1, 10000000)

_ = ax.set_ylim(0,120)
_ = ax.yaxis.set_ticks(np.arange(0, 120, 25))

_ = ax.set_xlim(1, 10**7)
_ = ax.xaxis.set_ticks(np.logspace(0, 7, 8))

# Create a Rectangle patch
rect = patches.Rectangle(
    (0,0),
    400,200, 
    linewidth=0,
    facecolor='#ffa6a65f', 
    zorder=1)

# Add the patch to the Axes
_ = ax.add_patch(rect)

locmin = mpl.ticker.LogLocator(base=10.0, subs=np.arange(2, 10) * .1,
                                      numticks=100)
_ = ax.xaxis.set_minor_locator(locmin)
_ = ax.xaxis.set_minor_formatter(mpl.ticker.NullFormatter())

_ = ax.yaxis.set_major_formatter(mtick.PercentFormatter())

font = {'family': 'serif',
        'color':  'darkred',
        'weight': 'normal',
        'size': fontsize,
        }

_ = ax.text(1.2, 102,'Time-critical',verticalalignment='bottom',horizontalalignment='left',color='Red', fontdict=font, fontweight='bold')
trans = ax.get_xaxis_transform()

_ = save_fig('fig_8', usepdfbound=True)

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

from common import *
from os import path
from typing import Dict, List, Set

import matplotlib.ticker as mtick
import matplotlib as mpl
from scipy.stats.mstats import gmean
from matplotlib.ticker import (MultipleLocator, FormatStrFormatter,
                               AutoMinorLocator, ScalarFormatter, LogLocator)
import matplotlib.patches as patches

import seaborn as sns

RESULT_LOC = ["/tmp/workload_ae/run_comparison_addr_data_disable/with-ev", 
              "/tmp/workload_ae/run_comparison_addr_data_disable/with-dw", 
              "/tmp/workload_ae/run_comparison_addr_data_disable/with-dw-predictor",
              "/tmp/workload_ae/run_comparison_addr_data_disable/with-ev-predictor",
              "/tmp/workload_ae/run_comparison_addr_data_disable/with-dw-data-disabled",
              "/tmp/workload_ae/run_comparison_addr_data_disable/with-ev-data-disabled"]

categories = []
for loc in RESULT_LOC:
    workloads = [dI for dI in os.listdir(loc) if os.path.isdir(os.path.join(loc,dI))]
    category = loc.split('/')[-1]
    categories.append(category)
    print(category,":",workloads)



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
idx = 0
for category in functioning_workloads:
    loc = RESULT_LOC[idx]
    stats[category] = {}
    for workload in functioning_workloads[category]:
        cur_stat = stats_to_dict(os.path.join(loc, workload, "m5out/stats.txt"))
        stats[category][workload] = cur_stat
    idx += 1

exec_time = {}
accuracyCorrect = {}
correctlyPredictedPWrites = {}

for category in stats:
    exec_time[category] = {}
    accuracyCorrect[category] = {}
    correctlyPredictedPWrites[category] = {}
    
    for workload in stats[category]:
        exec_time[category][workload] = float(stats[category][workload]['system.work_item_type0::mean'])
        
        correctlyPredictedPWrites[category][workload] = float(stats[category][workload]['system.pb.correctlyPredictedPWrites'])

        predictions = float(stats[category][workload]['system.cpu.pf.predictedWriteCount'])

        correctPredictions = correctlyPredictedPWrites[category][workload] if correctlyPredictedPWrites[category][workload] != 0 else 1
        accuracyCorrect[category][workload] = predictions/correctPredictions

indices = [key for key in categories]
df = pd.DataFrame(exec_time)

norm_df_1 = pd.DataFrame()
norm_df_2 = pd.DataFrame()

norm_df_1['with-ev-data-disabled'] = 1/(df['with-ev-data-disabled']/df['with-ev'])
norm_df_1['with-dw-data-disabled'] = 1/(df['with-dw-data-disabled']/df['with-dw'])

norm_df_2['with-ev-predictor'] = 1/(df['with-ev-predictor']/df['with-ev'])
norm_df_2['with-dw-predictor'] = 1/(df['with-dw-predictor']/df['with-dw'])

norm_df_1 = norm_df_1.T
norm_df_1['gm'] = [gmean(norm_df_1.T['with-ev-data-disabled']), gmean(norm_df_1.T['with-dw-data-disabled'])]
norm_df_1 = norm_df_1.T

norm_df_2 = norm_df_2.T
norm_df_2['gm'] = [gmean(norm_df_2.T['with-ev-predictor']), gmean(norm_df_2.T['with-dw-predictor'])]
norm_df_2 = norm_df_2.T

figsize = (7,1.65)
fontsize = 11



ax = fix_names(norm_df_2).plot.bar(
    zorder=3, 
    edgecolor='black', 
    color=[(1,1,1,1), (0.4,0.4,0.4,1)], 
    stacked=False, 
    figsize=figsize,
    fontsize=fontsize
)

handles, labels = ax.get_legend_handles_labels()

fix_names(norm_df_1).plot.bar(
    ax=ax,
    zorder=3, 
    edgecolor='black', 
    facecolor='none',
    hatch='///',
    stacked=False, 
    figsize=figsize,
    fontsize=fontsize
)

# ax.yaxis.set_ticks(np.arange(0, 6, 2))
ax.minorticks_on()
ax.grid(linestyle='-', zorder=0, axis='y', which='major')
set_ax_font(ax, fontsize)

ax.set_xticklabels(ax.get_xticklabels(), rotation=40, ha='right')
plt.axvline(x=9.5, color='k', ls='--', lw=0.5)

red_patch = patches.Patch(facecolor='none', edgecolor='k', hatch='///', label='Addr ')

handles.append(red_patch)

ax.set_ylim(1, 2.7)
ax.yaxis.set_ticks(np.arange(1, 2.7, 0.5))

ax.legend(
    labels=['Enc+Veri w/ PMWeaver \n(Addr+Data)', 'Dedup w/ PMWeaver \n(Addr+Data)', 'Addr-only'],
    loc='upper center',
    bbox_to_anchor=(0.44, 1.4),
    ncol=3,
    facecolor='white',
    framealpha=0,
    edgecolor='black',
    fancybox=False,
    fontsize=fontsize-1,
    handles=handles,
    markerscale=20,
#     handleheight=2.5,
#     handletextpad=0.3,
#     labelspacing=0.1
)


_ = ax.axes.set_ylabel('Speedup over\nno-PMWeaver', fontweight='bold', fontsize=fontsize+1)

save_fig('fig_19')


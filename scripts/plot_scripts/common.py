import matplotlib.pyplot as plt
import numpy as np
import os
import pandas as pd
import re
import warnings


from IPython.display import Markdown, display
from os import path
from typing import Dict, List, Set

import matplotlib.ticker as mtick
import matplotlib as mpl
from scipy.stats.mstats import gmean
from matplotlib.ticker import (MultipleLocator, FormatStrFormatter,
                               AutoMinorLocator, ScalarFormatter, LogLocator)
import matplotlib.patches as patches

import seaborn as sns



def stats_to_dict(stats_path: str) -> Dict:
    f = open(stats_path)
    result = {}
    for line in f:
        if not line.strip() == '' and not line.strip()[0] == '-':
            clean = re.sub(r'\s+', ' ', line).strip()
            split = clean.strip().split()
            result[split[0]] = split[1]
    return result

def set_ax_font(ax, size):
    for item in ([ax.title, ax.xaxis.label, ax.yaxis.label] +
             ax.get_xticklabels() + ax.get_yticklabels() +  ax.legend().get_texts()):
        item.set_fontsize(size)
        
def fix_names(df, col_index=0):
    result = pd.DataFrame(df)
    cols = result.index.values.tolist()
    if 'btree' in cols:
        if 'gm' in cols:
            result = result.reindex(['btree', 'ctree', 'rbtree', 'skiplist', 'hashmap_tx', 'queue', 'hashmap_ll', 'arr_swap', 'tatp_nvm', 'tpcc_nvm', 'gm'])
        elif 'GM' in cols:
            result = result.reindex(['btree', 'ctree', 'rbtree', 'skiplist', 'hashmap_tx', 'queue', 'hashmap_ll', 'arr_swap', 'tatp_nvm', 'tpcc_nvm', 'GM'])
        elif 'am' in cols:
            result = result.reindex(['btree', 'ctree', 'rbtree', 'skiplist', 'hashmap_tx', 'queue', 'hashmap_ll', 'arr_swap', 'tatp_nvm', 'tpcc_nvm', 'am'])
        elif 'AM' in cols:
            result = result.reindex(['btree', 'ctree', 'rbtree', 'skiplist', 'hashmap_tx', 'queue', 'hashmap_ll', 'arr_swap', 'tatp_nvm', 'tpcc_nvm', 'AM'])
        else:
            result = result.reindex(['btree', 'ctree', 'rbtree', 'skiplist', 'hashmap_tx', 'queue', 'hashmap_ll', 'arr_swap', 'tatp_nvm', 'tpcc_nvm'])
            
    cat_map_0 = {
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
        'hashmap_atomic'           : 'Hashmap\nAtomic',
        'hashmap_ll'               : 'Hashmap\nLow-level',
        'tpcc_nvm'                 : 'TPCC',
        'rbtree'                   : 'RB-Tree',
        'arr_swap'                 : 'Array\nSwap',
        'queue'                    : 'Linked List ',
        'gm'                       : 'GMean',
        'am'                       : 'AMean',
    }

    cat_map_1 = {
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
        'hashmap_atomic'           : 'Hashmap\nAtomic',
        'tpcc_nvm'                 : 'TPCC',
        'rbtree'                   : 'RB-Tree',
        'arr_swap'                 : 'Array\nSwap',
        'queue'                    : 'Linked List',
        'hashmap_ll'               : 'Hashmap\nLow-level',
        'gm'                       : 'GMean',
        'am'                       : 'AMean',
    }
    
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
        'hashmap_ll'               : 'Hashmap Low-level',
        'gm'                       : 'GMean',
        'am'                       : 'AMean',
    }
    
    
    cat_map_3 = {
        'with-dw'                  : 'Dedup',
        'with-ev'                  : 'Enc+Veri',     
        'with-dw-predictor'        : 'Dedup w/ PMWeaver',
        'with-ev-predictor'        : 'Enc+Veri w/ PMWeaver',
        'without-bmo-or-predictor' : 'Orignal System',
        'btree'                    : 'B-Tree',
        'skiplist'                 : 'Skiplist',
        'tatp_nvm'                 : 'TATP',
        'hashmap_tx'               : 'Hashmap\nTX   ',
        'ctree'                    : 'C-Tree',
        'fifo_bulk'                : 'Linked\nList ',
        'hashmap_atomic'           : 'Hashmap\nAtomic',
        'tpcc_nvm'                 : 'TPCC',
        'rbtree'                   : 'RB-Tree',
        'arr_swap'                 : 'Array\nSwap',
        'queue'                    : 'Linked List',
        'hashmap_ll'               : 'Hashmap\nLow-level',
        'gm'                       : 'GMean',
        'am'                       : 'AMean',
    }
    
    
    

    if (col_index == 0):
        result = result.rename(cat_map_0)
        result = result.rename(cat_map_0, axis='columns')
    if (col_index == 1):
        result = result.rename(cat_map_1)
        result = result.rename(cat_map_1, axis='columns')
    if (col_index == 2):
        result = result.rename(cat_map_2)
        result = result.rename(cat_map_2, axis='columns')
    if (col_index == 3):
        result = result.rename(cat_map_3)
        result = result.rename(cat_map_3, axis='columns')
    
    return result

def init_notebook():
    from IPython.core.display import display, HTML
    # display(HTML("<style>.container { width:80% !important; }</style>"))

    pd.set_option('display.max_rows', 500)
    pd.set_option('display.max_columns', 500)
    pd.set_option('display.width', 150)

    font = {
        'family' : 'serif',
        'weight' : 'normal',
        'size'   : 22
    }
    mpl.rc('font', **font)

def printmd(string):
    print(string)
    
c_disable_fig_save = False
c_save_loc = '/tmp/'
c_save_prefix="undefined"

def config_common(disable_fig_save=False, save_loc='/tmp', save_prefix='undefined'):
    global c_disable_fig_save
    global c_save_loc
    global c_save_prefix
    
    c_disable_fig_save = disable_fig_save
    c_save_loc = save_loc
    c_save_prefix = save_prefix

def save_fig(name, usepdfbound=True):
    if c_disable_fig_save:
        return
    
    save_dir = c_save_loc + '/' + c_save_prefix
    dest = save_dir + '/' + name + '.png'

    if os.path.exists(save_dir):
        if not os.path.isdir(c_save_loc):
            printmd('`' + c_save_loc + '` : <span style=\'color:red\'>is not a directory, FATAL</span>')
            return 
    else:
        os.makedirs(save_dir)
        
    plt.savefig(dest, bbox_inches='tight', dpi=400, transparent=False)    
    printmd('Plot saved as `' + dest + '`')
    
    plt.savefig(dest[:-4] + '.pdf', bbox_inches='tight', dpi=400, transparent=False, pad_inches=0)
    printmd('Plot saved as `' + dest + '`')
    
    if usepdfbound and False:
        cmd = 'pdfcrop %s.pdf %s.pdf' % (dest[:-4], dest[:-4])
        printmd('Using pdfcrop on `' + dest + '` with command `' + cmd + '`') 
        os.system(cmd)

    printmd('\n\n')
    
def save_df(df, name):
    if c_disable_fig_save:
        return
    
    save_dir = c_save_loc + '/' + c_save_prefix
    dest = save_dir + '/' + name + '.html'

    if os.path.exists(save_dir):
        if not os.path.isdir(c_save_loc):
            printmd('`' + c_save_loc + '` : <span style=\'color:red\'>is not a directory, FATAL</span>')
            return
    else:
        os.makedirs(save_dir)
        
    df.to_html(dest)    
    printmd('DataFrame saved as `' + dest + '`')

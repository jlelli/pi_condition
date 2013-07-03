#!/bin/bash
# Make sure only root can run our script
: ${2?"Usage: $0 RESULTS_PATH PLOTS_PATH"}

RESULTS_PATH=$1
PLOTS_PATH=$2

mkdir -p ${PLOTS_PATH}

for p in `seq 1 3`; do
    for c in `seq 1 3`; do
        for a in `seq 1 2`; do
            ./scripts/histogram.py -a ${RESULTS_PATH}/stat_no_pi_${p}prod_${c}cons_${a}annoy.dat\
            -b ${RESULTS_PATH}/stat_pi_${p}prod_${c}cons_${a}annoy.dat -f func_names.txt
            mv durations.eps ${PLOTS_PATH}/${p}prod_${c}cons_${a}annoy.eps
	done
    done
done

# vim: tabstop=8 expandtab shiftwidth=4 softtabstop=4

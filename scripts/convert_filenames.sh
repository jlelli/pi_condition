#!/bin/bash
: ${1?"Usage: $0 STATS_PATH"}

STATS_PATH=$1

for p in `seq 1 3`; do
    for c in `seq 1 3`; do
        for a in `seq 1 2`; do
            mv ${STATS_PATH}/stat_no_pi_${p}prod_${c}cons_${a}annoy.dat\
               ${STATS_PATH}/stat_no_pi_${p}prod_${c}cons_${a}annoy_a_f0.dat
	done
    done
done

for p in `seq 1 3`; do
    for c in `seq 1 3`; do
        for a in `seq 1 2`; do
            mv ${STATS_PATH}/stat_pi_${p}prod_${c}cons_${a}annoy.dat\
               ${STATS_PATH}/stat_pi_${p}prod_${c}cons_${a}annoy_a_f0.dat
	done
    done
done
# vim: tabstop=8 expandtab shiftwidth=4 softtabstop=4


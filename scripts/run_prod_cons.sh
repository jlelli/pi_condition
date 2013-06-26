#!/bin/bash
# Make sure only root can run our script
if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root" 1>&2
   exit 1
fi
: ${2?"Usage: $0 DURATION RESULTS_PATH"}

DURATION=$1
RESULTS_PATH=$2

mkdir -p ${RESULTS_PATH}

for p in `seq 1 3`; do
    for c in `seq 1 3`; do
        for a in `seq 1 2`; do
	    # without PI-cond
	    printf "${p} prod, ${c} cons, ${a} annoy, without PI"
            echo 1 > /debug/tracing/function_profile_enabled
            ./prod_cons -p ${p} -c ${c} -a ${a} -d ${DURATION}
            echo 0 > /debug/tracing/function_profile_enabled
            cat /debug/tracing/trace_stat/function0 > ${RESULTS_PATH}/stat_no_pi_${p}prod_${c}cons_${a}annoy.dat
	    
	    sleep 2

	    # with PI-cond
	    printf "${p} prod, ${c} cons, ${a} annoy, with PI"
            echo 1 > /debug/tracing/function_profile_enabled
            ./prod_cons -P -p ${p} -c ${c} -a ${a} -d ${DURATION}
            echo 0 > /debug/tracing/function_profile_enabled
            cat /debug/tracing/trace_stat/function0 > ${RESULTS_PATH}/stat_pi_${p}prod_${c}cons_${a}annoy.dat
	done
    done
done

# vim: tabstop=8 expandtab shiftwidth=4 softtabstop=4

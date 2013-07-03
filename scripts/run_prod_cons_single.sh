#!/bin/bash
# Make sure only root can run our script
if [[ $EUID -ne 0 ]]; then
  echo "This script must be run as root" 1>&2
  exit 1
fi
: ${6?"Usage: $0 DURATION RESULTS_PATH PROD CONS ANNOY AFF"}

DURATION=$1
RESULTS_PATH=$2
PROD=$3
CONS=$4
ANNOY=$5
AFF=$6

mkdir -p ${RESULTS_PATH}

# without PI-cond
printf "${PROD} prod, ${CONS} cons, ${ANNOY} annoy, without PI"
echo 1 > /debug/tracing/function_profile_enabled
if [ ${AFF} -eq 1 ]; then
  printf "affinity set"
  ./prod_cons -p ${PROD} -c ${CONS} -a ${ANNOY} -d ${DURATION} -A
else
  ./prod_cons -p ${PROD} -c ${CONS} -a ${ANNOY} -d ${DURATION}
fi
echo 0 > /debug/tracing/function_profile_enabled

if [ ${AFF} -eq 1 ]; then
  cat /debug/tracing/trace_stat/function0 > ${RESULTS_PATH}/stat_no_pi_${PROD}prod_${CONS}cons_${ANNOY}annoy_a.dat
else
  cat /debug/tracing/trace_stat/function0 > ${RESULTS_PATH}/stat_no_pi_${PROD}prod_${CONS}cons_${ANNOY}annoy_na_f0.dat
  cat /debug/tracing/trace_stat/function1 > ${RESULTS_PATH}/stat_no_pi_${PROD}prod_${CONS}cons_${ANNOY}annoy_na_f1.dat
  cat /debug/tracing/trace_stat/function2 > ${RESULTS_PATH}/stat_no_pi_${PROD}prod_${CONS}cons_${ANNOY}annoy_na_f2.dat
  cat /debug/tracing/trace_stat/function3 > ${RESULTS_PATH}/stat_no_pi_${PROD}prod_${CONS}cons_${ANNOY}annoy_na_f3.dat
fi

sleep 2

# with PI-cond
printf "${PROD} prod, ${CONS} cons, ${ANNOY} annoy, with PI"
echo 1 > /debug/tracing/function_profile_enabled
if [ ${AFF} -eq 1 ]; then
  printf "affinity set"
  ./prod_cons -P -p ${PROD} -c ${CONS} -a ${ANNOY} -d ${DURATION} -A
else
  ./prod_cons -P -p ${PROD} -c ${CONS} -a ${ANNOY} -d ${DURATION}
fi
echo 0 > /debug/tracing/function_profile_enabled

if [ ${AFF} -eq 1 ]; then
  cat /debug/tracing/trace_stat/function0 > ${RESULTS_PATH}/stat_pi_${PROD}prod_${CONS}cons_${ANNOY}annoy_a.dat
else
  cat /debug/tracing/trace_stat/function0 > ${RESULTS_PATH}/stat_pi_${PROD}prod_${CONS}cons_${ANNOY}annoy_na_f0.dat
  cat /debug/tracing/trace_stat/function1 > ${RESULTS_PATH}/stat_pi_${PROD}prod_${CONS}cons_${ANNOY}annoy_na_f1.dat
  cat /debug/tracing/trace_stat/function2 > ${RESULTS_PATH}/stat_pi_${PROD}prod_${CONS}cons_${ANNOY}annoy_na_f2.dat
  cat /debug/tracing/trace_stat/function3 > ${RESULTS_PATH}/stat_pi_${PROD}prod_${CONS}cons_${ANNOY}annoy_na_f3.dat
fi

# vim: tabstop=8 expandtab shiftwidth=4 softtabstop=4

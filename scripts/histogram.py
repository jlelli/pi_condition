#!/usr/bin/python
import sys
import getopt
import itertools
import re
import numpy
import matplotlib.pyplot as plt
import math

def get_names(func_file):
    names = []
    for func in func_file:
        names.append(func.rstrip())

    return names

def parse(in_file, names):
    names_aux = []
    n = []
    avgs = []
    vars = []

    #skip header
    next(in_file)
    next(in_file)

    for line in in_file:
        sline = re.split('\s+', line)
        if sline[1] in names:
            sline.pop()
            #names are shuffled...
            names_aux.append(sline[1])
            n.append(int(sline[2]))
            avgs.append(float(sline[5]))
            vars.append(float(sline[7]))

    return zip(names_aux, n, avgs, vars)

def pooledstats(datasets):
    """
    datasets is a multi-array of tuples (name, avg, stddev)
    where
      len(datasets) is the size of the dataset component
      avg - mean of name function
      xvar - variance of name function
    returns an array of tuples for the pooled data sets
    (name, avg, stddev).
    """
    pooled_data = []
    nfunc = len(datasets[0])

    names = [''] * nfunc
    nT = [0] * nfunc
    meanX  = [0.0] * nfunc
    varX   = [0.0] * nfunc
    for i in range(len(datasets)):
        print datasets[i]
        j = 0
        for (name, n, xbar, xvar) in datasets[i]:
            names[j] = name
            nT[j] += n
            meanX[j] += n * xbar
            varX[j] += n * xvar
            j += 1

    for j in range(nfunc):
        meanX[j] /= nT[j]

    # Second pass for variance.
    for i in range(len(datasets)):
        j = 0
        for (name, n, xbar, xvar) in datasets[i]:
            varX[j] +=  n * (xbar - meanX[j])**2
            j += 1

    for j in range(nfunc):
        varX[j] /=  nT[j]
    
    print names    
    print nT
    print meanX
    print varX    

    return zip(names, meanX, varX)

def plot(stats1, stats2):
    n = len(stats1)
    ind = numpy.arange(n)
    width = 0.35
    names = ['a','b','c','d','e','f']

    stddevs1 = []
    names1, avgs1, vars1 = zip(*stats1)
    for i in range(len(vars1)):
        stddevs1.append(math.sqrt(vars1[i]))

    stddevs2 = []
    names2, avgs2, vars2 = zip(*stats2)
    for i in range(len(vars2)):
        stddevs2.append(math.sqrt(vars2[i]))

    fig = plt.figure()
    fig.set_size_inches(10,6)
    ax = fig.add_subplot(111)
    rects1 = ax.bar(ind, avgs1, width, color='r', yerr=stddevs1)
    rects2 = ax.bar(ind+width, avgs2, width, color='y', yerr=stddevs2)

    ax.set_ylabel('duration (us)')
    ax.set_ylim(0, 25)
    ax.set_xticks(ind+width)
    #ax.set_xticklabels(names1, rotation=60, size='medium')
    ax.set_xticklabels(names, size='medium')

    ax.legend((rects1[0], rects2[0]),('no_pi','pi'))

    plt.tight_layout()
    plt.savefig('durations.eps', dpi=80)

def usage():
    print "python histogram.py -a stat1.dat -b stat2.dat -f func_names.txt"

def main(argv):
    aff = "na"
    nproc = 1
    stats = "./stats"
    functions = "func_names.txt"
    nprod = 1
    ncons = 1
    nannoy = 1

    try:
        opts, args = getopt.getopt(argv, "ap:f:s:P:C:A:", ["affinity","stats_dir=","func_file=","processors="
                                                          ,"producers=","consumers=","annoyers="])

    except getopt.GetoptError:
        usage()
        sys.exit(2)

    for opt, arg in opts:
        if opt in ("-a", "--affinity"):
            aff = "a"
        if opt in ("-s", "--stats_dir"):
            stats = arg
        if opt in ("-f", "--func_file"):
            functions = arg
        if opt in ("-p", "--processors"):
            nproc = int(arg)
        if opt in ("-P", "--producers"):
            nprod = arg
        if opt in ("-C", "--consumers"):
            ncons = arg
        if opt in ("-A", "--annoyers"):
            nannoy = arg

    func_file = open(functions, "r")
    names = get_names(func_file)

    stats1 = []

    for i in range(nproc):
        file_name = (stats + "/stat_no_pi_" + nprod + "prod_" +
                    ncons + "cons_" + nannoy + "annoy_" +
                    aff + "_f" + str(i) + ".dat")
        in_file = open(file_name, "r")
        stats1.append(parse(in_file, names))
        in_file.close()

    print stats1

    pooled_stats1 = []
    pooled_stats1 = pooledstats(stats1)
    print pooled_stats1
    pooled_stats1.sort(key=lambda tup: tup[0])
    print pooled_stats1

    stats2 = []

    for i in range(nproc):
        file_name = (stats + "/stat_pi_" + nprod + "prod_" +
                    ncons + "cons_" + nannoy + "annoy_" +
                    aff + "_f" + str(i) + ".dat")
        in_file = open(file_name, "r")
        stats2.append(parse(in_file, names))
        in_file.close()

    print stats2

    pooled_stats2 = []
    pooled_stats2 = pooledstats(stats2)
    print pooled_stats2
    pooled_stats2.sort(key=lambda tup: tup[0])
    print pooled_stats2

    plot(pooled_stats1, pooled_stats2)

if __name__ == "__main__":
    main(sys.argv[1:])

# vim: tabstop=8 expandtab shiftwidth=4 softtabstop=4

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
    avgs = []
    stddevs = []

    #skip header
    next(in_file)
    next(in_file)

    for line in in_file:
        sline = re.split('\s+', line)
        if sline[1] in names:
            sline.pop()
            #names are shuffled...
            names_aux.append(sline[1])
            avgs.append(float(sline[5]))
            stddevs.append(math.sqrt(float(sline[7])))

    return zip(names_aux, avgs, stddevs)

def plot(stats1, stats2):
    n = len(stats1)
    ind = numpy.arange(n)
    width = 0.35

    names1, avgs1, stddevs1 = zip(*stats1)
    names2, avgs2, stddevs2 = zip(*stats2)

    fig = plt.figure()
    ax = fig.add_subplot(111)
    rects1 = ax.bar(ind, avgs1, width, color='r', yerr=stddevs1)
    rects2 = ax.bar(ind+width, avgs2, width, color='y', yerr=stddevs2)

    ax.set_ylabel('duration (us)')
    ax.set_xticks(ind+width)
    ax.set_xticklabels(names1, rotation=60, size='x-small')

    ax.legend((rects1[0], rects2[0]),('no_pi','pi'))

    plt.tight_layout()
    plt.savefig('durations.eps')

def usage():
    print "python histogram.py -a stat1.dat -b stat2.dat -f func_names.txt"

def main(argv):
    file1 = "stat1.dat"
    file2 = "stat2.dat"
    functions = "func_names.txt"

    try:
        opts, args = getopt.getopt(argv, "a:b:f:", ["file1=","file2=","func_file="])

    except getopt.GetoptError:
        usage()
        sys.exit(2)

    for opt, arg in opts:
        if opt in ("-a", "--file1"):
            file1 = arg
        if opt in ("-b", "--file2"):
            file2 = arg
        if opt in ("-f", "--func_file"):
            functions = arg

    func_file = open(functions, "r")
    names = get_names(func_file)

    in_file1 = open(file1, "r")
    stats1 = parse(in_file1, names)
    stats1.sort(key=lambda tup: tup[0])
    in_file1.close()

    in_file2 = open(file2, "r")
    stats2 = parse(in_file2, names)
    stats2.sort(key=lambda tup: tup[0])
    in_file2.close()

    plot(stats1, stats2)

if __name__ == "__main__":
    main(sys.argv[1:])

# vim: tabstop=8 expandtab shiftwidth=4 softtabstop=4

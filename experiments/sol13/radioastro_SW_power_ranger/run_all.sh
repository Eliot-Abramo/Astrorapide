#!/bin/bash

arg=${1:-0}

if [[ $EUID -ne 0 ]]; then
    echo "Please, run with sudo."
    exit -1
fi

rm -f metrics.csv
rm -f out_*.bin
sync
./radioastro -c 1 -s ../data_bin/signal_data.bin -o out_1.bin -p metrics.csv
#python plot_timings.py -f metrics.csv

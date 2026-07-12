#!/bin/bash

arg=${1:-0}

if [[ $EUID -ne 0 ]]; then
    echo "Please, run with sudo."
    exit -1
fi

rm -f metrics.csv
rm -f out_*.bin
sync
./radioastro -c 0 -s ../data_bin/signal_data.bin -o out_0.bin -p metrics.csv
./radioastro -c 1 -s ../data_bin/signal_data.bin -o out_1.bin -p metrics.csv
./radioastro -c 2 -s ../data_bin/signal_data.bin -o out_2.bin -p metrics.csv
./radioastro -c 3 -s ../data_bin/signal_data.bin -o out_3.bin -p metrics.csv
./radioastro -c 4 -s ../data_bin/signal_data.bin -o out_4.bin -p metrics.csv
./radioastro -c 5 -s ../data_bin/signal_data.bin -o out_5.bin -p metrics.csv
./radioastro -c 6 -s ../data_bin/signal_data.bin -o out_6.bin -p metrics.csv
./radioastro -c 7 -s ../data_bin/signal_data.bin -o out_7.bin -p metrics.csv
./radioastro -c 8 -s ../data_bin/signal_data.bin -o out_8.bin -p metrics.csv
./radioastro -c 9 -s ../data_bin/signal_data.bin -o out_9.bin -p metrics.csv
#python plot_timings.py -f metrics.csv

#!/bin/bash

if [[ $EUID -ne 0 ]]; then
    echo "Please, run with sudo."
    exit -1
fi

rm -f metrics.csv
rm -f out_o.bin
sync
./radioastro -c 0 -s ../data_bin/signal_data.bin -o out_0.bin -p metrics.csv

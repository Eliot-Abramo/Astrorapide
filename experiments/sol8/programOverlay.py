#!/usr/bin/python3

import sys, os

if (len(sys.argv) != 2):
  print("Usage: sudo ./programOverlay.py bitstream_name.bit")
  exit(-1)
if not 'SUDO_UID' in os.environ.keys():
  print("Please, run with sudo.")
  exit(-1)

print("Programming FPGA with bitstream [{}].".format(sys.argv[1]))
print("Initializing...")
# Delayed import to save time in case of invocation errors.
from pynq import Overlay
print("Programming...")

ol = Overlay(sys.argv[1])

print("Bitstream programmed.")


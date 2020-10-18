#!/usr/bin/env python
##
##  First test in evaluation software.
##  This will connect to serial and grab all the data.
##

import csv
import time

filename = "tritium-neu.csv"

values = {}

RELATIVE = False
total = 1

start = time.time()

i = 0
imax = 3.3
decimal = 3

while i < imax:
    i += 1/pow(10,decimal)
    values[round(i,3)] = 0

with open(filename,"r") as file:
    reader = csv.reader(file, delimiter=";")
    for row in reader:
        if len(row) > 2:
            data = float(row[2])
            
            values[round(data,3)] += 1

print_data = []

if RELATIVE:
    for key in values:
        total += values[key]
    total -= 1

for key in values:
    print_data += [ values[key] / total ]

strarr = filename.split(".")
write_file = strarr[0] + "-p." + strarr[1]

with open(write_file,"w", newline="") as f:
            writer = csv.writer(f, delimiter=";")
            for val in print_data:
                writer.writerow([val])

print(f"Done in {round(time.time()-start,1)}s.")

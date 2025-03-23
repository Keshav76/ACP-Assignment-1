#!/bin/bash

g++ newMerged.cpp -o a.out -O2 

NUM_RUNS=10
OUTPUT_FILE="benchmark_results.txt"

echo -e "Run\tDJIT (s)\tFastTrack (s)" > "$OUTPUT_FILE"

DJIT_TOTAL=0
FASTTRACK_TOTAL=0

# Run the benchmark multiple times
for ((i=1; i<=NUM_RUNS; i++)); do

    DJIT_TIME=$( { time -p ./a.out DJIT log.txt; } 2>&1 | awk '/real/ {print $2}' )
    FASTTRACK_TIME=$( { time -p ./a.out FastTrack log.txt; } 2>&1 | awk '/real/ {print $2}' )

    DJIT_TOTAL=$(echo "$DJIT_TOTAL + $DJIT_TIME" | bc)
    FASTTRACK_TOTAL=$(echo "$FASTTRACK_TOTAL + $FASTTRACK_TIME" | bc)

    echo -e "$i\t$DJIT_TIME\t$FASTTRACK_TIME" >> "$OUTPUT_FILE"
done

# Compute averages
DJIT_AVG=$(echo "scale=3; $DJIT_TOTAL / $NUM_RUNS" | bc)
FASTTRACK_AVG=$(echo "scale=3; $FASTTRACK_TOTAL / $NUM_RUNS" | bc)

echo -e "Avg\t$DJIT_AVG\t$FASTTRACK_AVG" >> "$OUTPUT_FILE"
cat "$OUTPUT_FILE"

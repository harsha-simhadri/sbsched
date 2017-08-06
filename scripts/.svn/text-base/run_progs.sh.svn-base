#!/bin/bash
prog=$1
shift 1
for size in "$@"
do
for sched in W P 2 5
do
    echo "========= "$prog $sched $size" ========="
./scripts/LLC_misses_allcores.sh 1 "$prog $sched $size";
done
done

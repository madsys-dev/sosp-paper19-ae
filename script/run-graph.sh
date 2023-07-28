#!/bin/sh

vertice=4847571
edge=68993773
path=/

echo "Intersect"

for i in 1 2 4 8 16 24 32
do
	sleep 1
	./cxlmalloc-benchmark-graph $i $vertice $edge $path 0 >> graph-0.txt
done

grep "Throughput" graph-0.txt

echo "2 Hops Query"

for i in 1 2 4 8 16 24 32
do
	sleep 1
	./cxlmalloc-benchmark-graph $i $vertice $edge $path 1 >> graph-1.txt
done

grep "Throughput" graph-1.txt
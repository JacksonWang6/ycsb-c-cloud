#/bin/bash

workload="./workloads/$1.spec"
dbpath="/home/ubuntu/ssd/data/log"
datapath1="/home/ubuntu/ssd/data/data1"
datapath2="/home/ubuntu/zyh/data/data2"
pcache="/home/ubuntu/ssd/data/pcache"

rm -rf $dbpath/*
rm -rf $datapath1/*
rm -rf $datapath2/*
#rm -rf $pcache/*

if [ -n "$dbpath" ];then
	    rm -f $dbpath/*
fi
./ycsbc -db rocksdb -dbpath $dbpath -threads 1 -P $workload -load true -dboption 3

du -sbm $datapath1
du -sbm $datapath2
#du -sbm $pcache
sudo ~/cleancache.sh
sleep 30

echo "run"
cgexec -g memory:mutant ./ycsbc -db rocksdb -dbpath $dbpath -threads 1 -P $workload -run true -dboption 3 -dbstatistics true
echo "run"


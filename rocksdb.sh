#/bin/bash

workload="./workloads/$1.spec"
dbpath="/home/ubuntu/ssd/data/log"
datapath1="/home/ubuntu/ssd/data/data1"
datapath2="/home/ubuntu/zyh/data/data2"
pcache="/home/ubuntu/ssd/data/pcache"

rm -rf $dbpath/*
rm -rf $datapath1/*
rm -rf $datapath2/*
rm -rf $pcache/*
rm -rf /home/ubuntu/ssd/data/data2/*
rm -rf /home/ubuntu/zyh/data/data1/*



if [ -n "$dbpath" ];then
	    rm -f $dbpath/*
fi
./ycsbc -db rocksdb -dbpath $dbpath -threads 1 -P $workload -load true -dboption $2

echo date
du -sbm $datapath1
du -sbm $datapath2
du -sbm $pcache

sleep 30

echo "run"
./ycsbc -db rocksdb -dbpath $dbpath -threads 1 -P $workload -run true -dboption $2 -dbstatistics true
echo "run"

sudo ./cleancache.sh && ./ycsbc -db rocksdb -dbpath /home/ubuntu/ssd_150g -threads 10 -P workloads/workloadc.spec -load true -dboption 2
sudo ./cleancache.sh && ./ycsbc -db rocksdb -dbpath /home/ubuntu/ssd_150g -threads 10 -P workloads/workloadc.spec -run true -dboption 2

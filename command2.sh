sudo rm -rf /home/ubuntu/ssd_150g/*
sudo rm -rf /home/ubuntu/gp2_50g_1/*
sudo rm -rf /home/ubuntu/gp2_50g_2/*
sudo rm -rf /home/ubuntu/gp3_100g_1/*
sudo rm -rf /home/ubuntu/gp3_100g_2/*
sudo rm -rf /home/ubuntu/gp3_100g_3/*
sudo rm -rf /home/ubuntu/raid0/*
sudo rm -rf /home/ubuntu/raid1/*
sudo rm -rf ./log/*
sudo mkdir /home/ubuntu/gp2_50g_1/pcache
sudo mkdir /home/ubuntu/gp2_50g_2/pcache
sleep 2

iostat -d /dev/nvme*n1 -d /dev/md*  -m 1 >./log/iostat_load.log &
sudo ./cleancache.sh
sudo ./ycsbc -db rocksdb -dbpath /home/ubuntu/ssd_150g -threads 10 -P workloads/workloadc.spec -load true -dboption 2 | tee ./log/load.log
sleep 2
pkill iostat



# sudo rm -rf ./log/*

iostat -d /dev/nvme*n1 -d /dev/md* -m 1 >./log/iostat_25read.log &
sudo ./cleancache.sh
sudo ./ycsbc_c -db rocksdb -dbpath /home/ubuntu/ssd_150g -threads 10 -P workloads/wire75read25.spec -run true -dboption 2 | tee ./log/read25.log
sleep 2
pkill iostat




iostat -d /dev/nvme*n1 -d /dev/md* -m 1 >./log/iostat_25read.log &
sudo ./cleancache.sh
sudo ./ycsbc -db rocksdb -dbpath /home/ubuntu/ssd_150g -threads 10 -P workloads/wire75read25.spec -run true -dboption 2 | tee ./log/read25.log
sleep 2
pkill iostat


iostat -d /dev/nvme*n1 -d /dev/md* -m 1 >./log/iostat_50read.log &
sudo ./cleancache.sh
sudo ./ycsbc -db rocksdb -dbpath /home/ubuntu/ssd_150g -threads 10 -P workloads/wire50read50.spec -run true -dboption 2 | tee ./log/read50.log
sleep 2
pkill iostat



iostat -d /dev/nvme*n1 -d /dev/md* -m 1 >./log/iostat_75read.log &
sudo ./cleancache.sh
sudo ./ycsbc -db rocksdb -dbpath /home/ubuntu/ssd_150g -threads 10 -P workloads/wire25read75.spec -run true -dboption 2 | tee ./log/read75.log
sleep 2
pkill iostat

iostat -d /dev/nvme*n1 -d /dev/md* -m 1 >./log/iostat_100read.log &
sudo ./cleancache.sh
sudo ./ycsbc -db rocksdb -dbpath /home/ubuntu/ssd_150g -threads 10 -P workloads/workloadc.spec -run true -dboption 2 | tee ./log/read100.log
sleep 2
pkill iostat








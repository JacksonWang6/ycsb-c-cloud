# sudo rm -rf /home/ubuntu/ssd_150g/*
# sudo rm -rf /home/ubuntu/gp2_50g_1/*
# sudo rm -rf /home/ubuntu/gp2_50g_2/*
# sudo rm -rf /home/ubuntu/gp3_100g_1/*
# sudo rm -rf /home/ubuntu/gp3_100g_2/*
# sudo rm -rf /home/ubuntu/gp3_100g_3/*
# sudo rm -rf /home/ubuntu/raid0/*
# sudo rm -rf /home/ubuntu/raid1/*
# sudo rm -rf ./log/*
# sudo mkdir /home/ubuntu/gp2_50g_1/pcache
# sudo mkdir /home/ubuntu/gp2_50g_2/pcache
# sleep 2

# iostat -d /dev/nvme*n1 -d /dev/md*  -m 1 >./log/iostat_load.log &
# sudo ./cleancache.sh
# sudo ./ycsbc -db rocksdb -dbpath /home/ubuntu/ssd_150g -threads 10 -P workloads/workloadc.spec -load true -dboption 2 | tee ./log/load.log
# sleep 2
# pkill iostat



# sudo rm -rf ./log/*




# iostat -d /dev/nvme*n1 -d /dev/md* -m 1 >./log/iostat_c.log &
# sudo ./cleancache.sh
# sudo ./ycsbc -db rocksdb -dbpath /home/ubuntu/ssd_150g -threads 10 -P workloads/workloadc.spec -run true -dboption 2 | tee ./log/c.log
# sleep 2
# pkill iostat


# iostat -d /dev/nvme*n1 -d /dev/md* -m 1 >./log/iostat_a.log &
# sudo ./cleancache.sh
# sudo ./ycsbc -db rocksdb -dbpath /home/ubuntu/ssd_150g -threads 10 -P workloads/workloada.spec -run true -dboption 2 | tee ./log/a.log
# sleep 2
# pkill iostat



# iostat -d /dev/nvme*n1 -d /dev/md* -m 1 >./log/iostat_b.log &
# sudo ./cleancache.sh
# sudo ./ycsbc -db rocksdb -dbpath /home/ubuntu/ssd_150g -threads 10 -P workloads/workloadb.spec -run true -dboption 2 | tee ./log/b.log
# sleep 2
# pkill iostat


iostat -d /dev/nvme*n1 -d /dev/md* -m 1 >./log/iostat_c.log &
sudo ./cleancache.sh
sudo ./ycsbc -db rocksdb -dbpath /home/ubuntu/ssd_150g -threads 10 -P workloads/workloadc.spec -run true -dboption 2 | tee ./log/c.log
sleep 2
pkill iostat

iostat -d /dev/nvme*n1 -d /dev/md* -m 1 >./log/iostat_d.log &
sudo ./cleancache.sh
sudo ./ycsbc -db rocksdb -dbpath /home/ubuntu/ssd_150g -threads 10 -P workloads/workloadd.spec -run true -dboption 2 | tee ./log/d.log
sleep 2
pkill iostat

iostat -d /dev/nvme*n1 -d /dev/md* -m 1 >./log/iostat_e.log &
sudo ./cleancache.sh
sudo ./ycsbc -db rocksdb -dbpath /home/ubuntu/ssd_150g -threads 10 -P workloads/workloade.spec -run true -dboption 2 | tee ./log/e.log
sleep 2
pkill iostat

iostat -d /dev/nvme*n1 -d /dev/md* -m 1 >./log/iostat_f.log &
sudo ./cleancache.sh
sudo ./ycsbc -db rocksdb -dbpath /home/ubuntu/ssd_150g -threads 10 -P workloads/workloadf.spec -run true -dboption 2 | tee ./log/f.log
sleep 2
pkill iostat





iostat -d /dev/nvme*n1 -d /dev/md* -m 1 >./log/iostat_test.log &
sudo ./cleancache.sh
sudo ./ycsbc -db rocksdb -dbpath /home/ubuntu/ssd_150g -threads 10 -P workload_test.spec -run true -dboption 2 | tee ./log/test.log
sleep 2
pkill iostat
nohup ./ycsbc -load true -db rocksdb-cloud -threads 8 -P ./workloads/workloadc.spec > ./weiyan/write.log 2>&1 &
nohup sudo ./ycsbc -run true -db rocksdb-cloud -threads 8 -P ./workloads/workloadc.spec > ./log/rocksdb_hyper_read.log 2>&1 &
# scan
./ycsbc -run true -db rocksdb-cloud -threads 8 -P ./workloads/workloade.spec
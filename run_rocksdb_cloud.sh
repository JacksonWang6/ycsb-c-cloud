trap 'kill $(jobs -p)' SIGINT

workloads="./workloads/workloada.spec ./workloads/workloadb.spec ./workloads/workloadd.spec ./workloads/workloadf.spec"

./ycsbc -load true -db rocksdb-cloud -threads 4 -P ./workload_test.spec
# ./ycsbc -run true -db rocksdb-cloud -threads 1 -P ./workloads/workloada.spec
for file_name in $workloads; do
  echo "Running Rocksdb with for $file_name"
  ./ycsbc -run true -db rocksdb-cloud -threads 1 -P $file_name
done

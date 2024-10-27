# Yahoo! Cloud System Benchmark
# Workload C: Read only
#   Application example: user profile cache, where profiles are constructed elsewhere (e.g., Hadoop)
#                        
#   Read/update ratio: 100/0
#   Default data size: 1 KB records (10 fields, 100 bytes each, plus key)
#   Request distribution: zipfian
fieldcount=1
fieldlength=256

   recordcount=200000000
operationcount=30000000
workload=com.yahoo.ycsb.workloads.CoreWorkload

readallfields=true

readproportion=0.75
updateproportion=0
scanproportion=0
insertproportion=0.25

#requestdistribution=uniform
requestdistribution=zipfian




#!/bin/bash

set -e
set -m

export AUTOCANCEL_HOME=$(git rev-parse --show-toplevel)
export MICROBENCHMARK=rally_benchmark
export START_TIME=$(date +%Y_%m_%d_%H_%M_%S)
export START_DATE=$(date +%Y_%m_%d)

client_num_list=(1 16 32 64 128)
test_times=1

sudo sysctl -w vm.max_map_count=262144

if [ ! -d "$AUTOCANCEL_HOME/scripts/data/rally" ]; then
    mkdir $AUTOCANCEL_HOME/scripts/data/rally
	cp $AUTOCANCEL_HOME/scripts/data/elasticsearch/liblagent.so $AUTOCANCEL_HOME/scripts/data/rally
fi

if [ ! -d "$AUTOCANCEL_HOME/scripts/data/rally_home" ]; then
    mkdir $AUTOCANCEL_HOME/scripts/data/rally_home
fi

if [ ! -d "$AUTOCANCEL_HOME/scripts/logs/$START_DATE" ]; then
    mkdir $AUTOCANCEL_HOME/scripts/logs/$START_DATE
fi

mkdir $AUTOCANCEL_HOME/scripts/logs/$START_DATE/${MICROBENCHMARK}_${START_TIME}

if [ ! -f "$AUTOCANCEL_HOME/autocancel_exp/elasticsearch_exp/query/boolean_search.json" ]; then
    docker run --rm --net=host -v $AUTOCANCEL_HOME/autocancel_exp/elasticsearch_exp:/root -w /root easonliu12138/es_py_env:v1.1 /root/performance_issues/complex_boolean_operations.py 2000 boolean_search.json
fi

function run_once {
    USER_ID=$(id -u) GROUP_ID=$(id -g) DEFAULT_POLICY=$1 PREDICT_PROGRESS=$2 CANCEL_ENABLE=$3 AUTOCANCEL_LOG=$4 AUTOCANCEL_START=$5 docker compose -f $AUTOCANCEL_HOME/scripts/microbenchmark/rally_benchmark/docker_config.yml down

    USER_ID=$(id -u) GROUP_ID=$(id -g) DEFAULT_POLICY=$1 PREDICT_PROGRESS=$2 CANCEL_ENABLE=$3 AUTOCANCEL_LOG=$4 AUTOCANCEL_START=$5 docker compose -f $AUTOCANCEL_HOME/scripts/microbenchmark/rally_benchmark/docker_config.yml up &
    sleep 90

    for j in $(seq 1 1 $test_times); do
        BENCHMARK_START_TIME=$(date +%Y_%m_%d_%H_%M_%S)
		docker run --rm --net=host -v $AUTOCANCEL_HOME/scripts/data/rally_home:/home/rally/.rally -t rally_exp:v1.0 \
			esrally race --track-revision=autocancel --track=random_vector --pipeline=benchmark-only --target-hosts=127.0.0.1:9200 --report-file=/home/rally/.rally/report-${5}-${BENCHMARK_START_TIME}.csv --report-format=csv \
			--track-params="index_clients:$6,index_iterations:100,index_bulk_size:100,search_iterations:100,search_clients:$7"

        cp $AUTOCANCEL_HOME/scripts/data/rally_home/report-${5}-${BENCHMARK_START_TIME}.csv $AUTOCANCEL_HOME/scripts/logs/$START_DATE/${MICROBENCHMARK}_${START_TIME}/report-${5}-${BENCHMARK_START_TIME}-index-${6}-search-${7}.csv

        sleep 10
    done

    USER_ID=$(id -u) GROUP_ID=$(id -g) DEFAULT_POLICY=$1 PREDICT_PROGRESS=$2 CANCEL_ENABLE=$3 AUTOCANCEL_LOG=$4 AUTOCANCEL_START=$5 docker compose -f $AUTOCANCEL_HOME/scripts/microbenchmark/rally_benchmark/docker_config.yml down
}

if [[ "$1" =~ ^[0-9]+$ ]]; then
	run_once multi_objective_policy true false normal true $1 $1
	sleep 10
	run_once multi_objective_policy true false normal false $1 $1
	sleep 10
fi

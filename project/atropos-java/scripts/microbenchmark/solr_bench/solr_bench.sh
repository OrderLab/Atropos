#!/bin/bash

set -e
set -m

export AUTOCANCEL_HOME=$(git rev-parse --show-toplevel)
export MICROBENCHMARK=solr_bench
export START_TIME=$(date +%Y_%m_%d_%H_%M_%S)
export START_DATE=$(date +%Y_%m_%d)

client_num_list=(1 16 32 64 128)
test_times=1

sudo sysctl -w vm.max_map_count=262144

if [ ! -d "$AUTOCANCEL_HOME/scripts/data/solr_bench" ]; then
    mkdir $AUTOCANCEL_HOME/scripts/data/solr_bench
fi

if [ ! -d "$AUTOCANCEL_HOME/scripts/logs/$START_DATE" ]; then
    mkdir $AUTOCANCEL_HOME/scripts/logs/$START_DATE
fi

mkdir $AUTOCANCEL_HOME/scripts/logs/$START_DATE/${MICROBENCHMARK}_${START_TIME}

function run_once {
    USER_ID=$(id -u) GROUP_ID=$(id -g) DEFAULT_POLICY=$1 PREDICT_PROGRESS=$2 CANCEL_ENABLE=$3 AUTOCANCEL_LOG=$4 AUTOCANCEL_START=$5 \
		docker compose -f $AUTOCANCEL_HOME/scripts/microbenchmark/solr_bench/docker_config.yml down

    USER_ID=$(id -u) GROUP_ID=$(id -g) DEFAULT_POLICY=$1 PREDICT_PROGRESS=$2 CANCEL_ENABLE=$3 AUTOCANCEL_LOG=$4 AUTOCANCEL_START=$5 \
		docker compose -f $AUTOCANCEL_HOME/scripts/microbenchmark/solr_bench/docker_config.yml up &
    sleep 90

	tmp=$(mktemp)
	jq '."task-types".indexing."index-benchmark"."min-threads" = '$6'' $AUTOCANCEL_HOME/scripts/data/solr_bench_home/stress-facets-local-autocancel-base.json | \
		jq '."task-types".indexing."index-benchmark"."max-threads" = '$6'' | \
		jq '."task-types".querying."query-benchmark"."min-threads" = '$7'' | \
		jq '."task-types".querying."query-benchmark"."max-threads" = '$7'' > $tmp

	mv $tmp $AUTOCANCEL_HOME/scripts/data/solr_bench_home/stress-facets-local-autocancel.json

    for j in $(seq 1 1 $test_times); do
        BENCHMARK_START_TIME=$(date +%Y_%m_%d_%H_%M_%S)
		docker run --rm --net=host -e START_TIME=$START_TIME -v $AUTOCANCEL_HOME/scripts/data/solr_bench_home:/solr-bench/suites easonliu12138/solr_bench_exp:v1.3
		mv $AUTOCANCEL_HOME/scripts/data/solr_bench_home/results-$START_TIME.json $AUTOCANCEL_HOME/scripts/logs/$START_DATE/${MICROBENCHMARK}_${START_TIME}/enable_${5}_index_${6}_search_${7}_test_${j}_result.json

        sleep 10
    done

    USER_ID=$(id -u) GROUP_ID=$(id -g) DEFAULT_POLICY=$1 PREDICT_PROGRESS=$2 CANCEL_ENABLE=$3 AUTOCANCEL_LOG=$4 AUTOCANCEL_START=$5 \
		docker compose -f $AUTOCANCEL_HOME/scripts/microbenchmark/solr_bench/docker_config.yml down
}

if [[ "$1" =~ ^[0-9]+$ ]]; then
	run_once multi_objective_policy true false normal true $1 $1
	sleep 10
	run_once multi_objective_policy true false normal false $1 $1
	sleep 10
fi


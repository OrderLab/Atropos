#!/bin/bash

set -e
set -m

export AUTOCANCEL_HOME=$(git rev-parse --show-toplevel)
export MICROBENCHMARK=interval_sensitivity
export START_TIME=$(date +%Y_%m_%d_%H_%M_%S)
export START_DATE=$(date +%Y_%m_%d)

declare -A case_to_script_map
case_to_script_map["c1"]="elasticsearch_exp multiclient_request_cache_evict 8.00 16 5 c1_cache_evict"
# case_to_script_map["c2"]="elasticsearch_exp multiclient_update_by_query 8.00 16 8 c2_byquery"
case_to_script_map["c3"]="elasticsearch_exp multiclient_nested_aggs 12.00 16 8 c3_nest_agg"
case_to_script_map["c4"]="elasticsearch_exp multiclient_complex_boolean 12.00 16 5 c4_complex_boolean"
case_to_script_map["c5"]="elasticsearch_exp multiclient_bulk_large_document 8.00 16 5 c5_bulk_document"
case_to_script_map["c6"]="solr_exp complex_boolean_script 1.50 16 2 c6_complex_request"
case_to_script_map["c7"]="solr_exp stat_fields 4.00 16 8 c7_stat_fields"

sudo sysctl -w vm.max_map_count=262144

if [ ! -d "$AUTOCANCEL_HOME/scripts/logs/$START_DATE" ]; then
    mkdir $AUTOCANCEL_HOME/scripts/logs/$START_DATE
fi

mkdir $AUTOCANCEL_HOME/scripts/logs/$START_DATE/${MICROBENCHMARK}_${START_TIME}

function run_once {
    local app_exp=$(echo ${case_to_script_map["$5"]} | awk '{print $1}')
    local case_name=$(echo ${case_to_script_map["$5"]} | awk '{print $2}')
    local core_num=$(echo ${case_to_script_map["$5"]} | awk '{print $3}')
    local heap_size=$(echo ${case_to_script_map["$5"]} | awk '{print $4}')
    local client_num=$(echo ${case_to_script_map["$5"]} | awk '{print $5}')
    local case_dir=$(echo ${case_to_script_map["$5"]} | awk '{print $6}')

	local env_args="USER_ID=$(id -u) GROUP_ID=$(id -g) DEFAULT_POLICY=$1 PREDICT_PROGRESS=$2 CANCEL_ENABLE=$3 AUTOCANCEL_LOG=$4 UPDATE_INTERVAL=$6 CORE_NUM=$core_num HEAP_SIZE=$heap_size CASE_DIR=$case_dir"

	bash -c "$env_args docker compose -f $AUTOCANCEL_HOME/scripts/microbenchmark/interval_sensitivity/${app_exp}_docker_config.yml down"

	bash -c "$env_args docker compose -f $AUTOCANCEL_HOME/scripts/microbenchmark/interval_sensitivity/${app_exp}_docker_config.yml up &"
    sleep 60

    docker run --rm --net=host -v $AUTOCANCEL_HOME/autocancel_exp/$app_exp:/root -w /root easonliu12138/es_py_env:v1.1 /root/scripts/warmup.sh
    sleep 10

	rm -f $AUTOCANCEL_HOME/autocancel_exp/$app_exp/autocancel_lib_log
	ln $AUTOCANCEL_HOME/scripts/logs/$START_DATE/${MICROBENCHMARK}_${START_TIME}/${4}.csv $AUTOCANCEL_HOME/autocancel_exp/$app_exp/autocancel_lib_log
    docker run --rm --net=host -v $AUTOCANCEL_HOME/autocancel_exp/$app_exp:/root -w /root easonliu12138/es_py_env:v1.1 /root/scripts/$case_name.sh $client_num ${4}_${START_TIME} $4
    sleep 10

	rm -f $AUTOCANCEL_HOME/autocancel_exp/$app_exp/autocancel_lib_log
	rm -f $AUTOCANCEL_HOME/scripts/logs/$START_DATE/${MICROBENCHMARK}_${START_TIME}/${4}.csv
    mv $AUTOCANCEL_HOME/autocancel_exp/$app_exp/autocancel_lib_log_strip $AUTOCANCEL_HOME/scripts/logs/$START_DATE/${MICROBENCHMARK}_${START_TIME}/${4}.csv

    mv $AUTOCANCEL_HOME/autocancel_exp/$app_exp/${4}_${START_TIME}_latency $AUTOCANCEL_HOME/scripts/logs/$START_DATE/${MICROBENCHMARK}_${START_TIME}/${4}_latency.csv
    mv $AUTOCANCEL_HOME/autocancel_exp/$app_exp/${4}_${START_TIME}_throughput $AUTOCANCEL_HOME/scripts/logs/$START_DATE/${MICROBENCHMARK}_${START_TIME}/${4}_throughput.csv

    mv $AUTOCANCEL_HOME/scripts/logs/$START_DATE/${MICROBENCHMARK}_${START_TIME}/${4}.csv $AUTOCANCEL_HOME/scripts/logs/$START_DATE/${MICROBENCHMARK}_${START_TIME}/${5}_${4}_${6}.csv
    mv $AUTOCANCEL_HOME/scripts/logs/$START_DATE/${MICROBENCHMARK}_${START_TIME}/${4}_latency.csv $AUTOCANCEL_HOME/scripts/logs/$START_DATE/${MICROBENCHMARK}_${START_TIME}/${5}_${4}_${6}_latency.csv
    mv $AUTOCANCEL_HOME/scripts/logs/$START_DATE/${MICROBENCHMARK}_${START_TIME}/${4}_throughput.csv $AUTOCANCEL_HOME/scripts/logs/$START_DATE/${MICROBENCHMARK}_${START_TIME}/${5}_${4}_${6}_throughput.csv

	bash -c "$env_args docker compose -f $AUTOCANCEL_HOME/scripts/microbenchmark/interval_sensitivity/${app_exp}_docker_config.yml down"
}

function run_case {
    run_once base_policy false true base_wo_predict $1 $2
    sleep 10

    run_once base_policy true true base_w_predict $1 $2
    sleep 10

    run_once multi_objective_policy false true moo_wo_predict $1 $2
    sleep 10

    run_once multi_objective_policy true true moo_w_predict $1 $2
    sleep 10

    run_once multi_objective_policy true false wo_cancel $1 $2
    sleep 10

    run_once multi_objective_policy true false normal $1 $2
    sleep 10
}

function run_round {
    run_case $1 50
    sleep 10
    run_case $1 100
    sleep 10
    run_case $1 200
    sleep 10
}

if [[ "$1" =~ ^c[1-7]$ ]]; then
	run_round $1
fi


#!/bin/bash

set -e
set -m

export AUTOCANCEL_HOME=$(git rev-parse --show-toplevel)
export CASE=c2
export START_TIME=$(date +%Y_%m_%d_%H_%M_%S)
export START_DATE=$(date +%Y_%m_%d)

client_num=8

sudo sysctl -w vm.max_map_count=262144

if [ ! -d "$AUTOCANCEL_HOME/scripts/logs/$START_DATE" ]; then
    mkdir $AUTOCANCEL_HOME/scripts/logs/$START_DATE
fi

mkdir $AUTOCANCEL_HOME/scripts/logs/$START_DATE/${CASE}_${START_TIME}

function run_once {
    env_args="USER_ID=$(id -u) GROUP_ID=$(id -g) DEFAULT_POLICY=$1 PREDICT_PROGRESS=$2 CANCEL_ENABLE=$3 AUTOCANCEL_LOG=$4"
	bash -c "$env_args docker compose -f $AUTOCANCEL_HOME/scripts/cases/c2_byquery/docker_config.yml down"

    docker stop single_node || true
    docker rm single_node || true

	bash -c "$env_args docker compose -f $AUTOCANCEL_HOME/scripts/cases/c2_byquery/docker_config.yml up &"
    sleep 90

    docker run --rm --net=host -v $AUTOCANCEL_HOME/autocancel_exp/elasticsearch_exp:/root -w /root easonliu12138/es_py_env:v1.1 /root/scripts/warmup.sh
    sleep 10

	rm -f $AUTOCANCEL_HOME/autocancel_exp/elasticsearch_exp/autocancel_lib_log
	ln $AUTOCANCEL_HOME/scripts/logs/$START_DATE/${CASE}_${START_TIME}/${4}.csv $AUTOCANCEL_HOME/autocancel_exp/elasticsearch_exp/autocancel_lib_log
    docker run --rm --net=host -v $AUTOCANCEL_HOME/autocancel_exp/elasticsearch_exp:/root -w /root easonliu12138/es_py_env:v1.1 /root/scripts/multiclient_update_by_query.sh $5 ${4}_${START_TIME} $4
    sleep 10

	rm -f $AUTOCANCEL_HOME/autocancel_exp/elasticsearch_exp/autocancel_lib_log
	rm -f $AUTOCANCEL_HOME/scripts/logs/$START_DATE/${CASE}_${START_TIME}/${4}.csv
    mv $AUTOCANCEL_HOME/autocancel_exp/elasticsearch_exp/autocancel_lib_log_strip $AUTOCANCEL_HOME/scripts/logs/$START_DATE/${CASE}_${START_TIME}/${4}.csv
    mv $AUTOCANCEL_HOME/autocancel_exp/elasticsearch_exp/${4}_${START_TIME}_latency $AUTOCANCEL_HOME/scripts/logs/$START_DATE/${CASE}_${START_TIME}/${4}_latency.csv
    mv $AUTOCANCEL_HOME/autocancel_exp/elasticsearch_exp/${4}_${START_TIME}_throughput $AUTOCANCEL_HOME/scripts/logs/$START_DATE/${CASE}_${START_TIME}/${4}_throughput.csv

	bash -c "$env_args docker compose -f $AUTOCANCEL_HOME/scripts/cases/c2_byquery/docker_config.yml down"
}

run_once base_policy false true base_wo_predict $client_num
sleep 10

# run_once base_policy true true base_w_predict $client_num
# sleep 10
# 
# run_once multi_objective_policy false true moo_wo_predict $client_num
# sleep 10
# 
# run_once multi_objective_policy true true moo_w_predict $client_num
# sleep 10
# 
# run_once multi_objective_policy true false wo_cancel $client_num
# sleep 10
# 
# run_once multi_objective_policy true false normal $client_num
# sleep 10

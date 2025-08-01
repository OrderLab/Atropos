#!/bin/bash

AUTOCANCELDIR=/home/cc/autocancel/autocancel-simulation
export LD_LIBRARY_PATH=$AUTOCANCELDIR/build/libs:$LD_LIBRARY_PATH

POSTGRE_ORIGINAL_PATH=/home/cc/autocancel/applications/postgres-original
POSTGRE_AUTOCANCEL_PATH=/home/cc/autocancel/applications/postgres-autocancel
SYSBENCH_PATH=/home/cc/autocancel/autocancel-sosp/sysbench
USER=cc
LOG_PATH=/home/cc/autocancel/autocancel-sosp/logs
CASE_PATH=/home/cc/autocancel/autocancel-sosp/cases/postgres-case/buffer-pool-case
PREPARE_SCRIPT=$CASE_PATH/../prepare.sh

function normal(){
    sleep 5
    cd $SYSBENCH_PATH
    ./src/sysbench ./src/lua/pgsql_buffer_pool_case.lua --pgsql-host=127.0.0.1 --db-driver=pgsql --pgsql-port=5432 --pgsql-user=$USER --pgsql-db=test --tables=24 --table-size=1000 --threads=16 --time=90 --report-interval=1 run > $LOG_PATH/bfc-normal-$1 2>&1 &
    sleep 115
}

function inter(){
    sleep 5
    cd $SYSBENCH_PATH
    ./src/sysbench ./src/lua/pgsql_buffer_pool_case.lua --pgsql-host=127.0.0.1 --db-driver=pgsql --pgsql-port=5432 --pgsql-user=$USER --pgsql-db=test --tables=24 --table-size=1000 --threads=16 --time=90 --report-interval=1 run > $LOG_PATH/bfc-inter-$1 2>&1 &
    sleep 60
    cd $CASE_PATH
    ./vacuum_table25.sh $POSTGRE_PATH
    sleep 35
}

# Run the prepare script
$PREPARE_SCRIPT

CUR_TIME=$(date +%Y-%m-%d-%H-%M)
LOG_PATH=$LOG_PATH/$CUR_TIME-buffer-pool-case-$1
mkdir -p $LOG_PATH

# normal or inter or autocancel or error out
if [ $1 == "normal" ]; then
    FUNCTION=normal
    POSTGRE_PATH=$POSTGRE_ORIGINAL_PATH
elif [ $1 == "inter" ]; then
    FUNCTION=inter
    # POSTGRE_PATH=$POSTGRE_ORIGINAL_PATH
    POSTGRE_PATH=$POSTGRE_AUTOCANCEL_PATH
elif [ $1 == "autocancel" ]; then
    FUNCTION=inter
    POSTGRE_PATH=$POSTGRE_AUTOCANCEL_PATH
    export SHOULD_KILL=1
    export AUTOCANCEL_POLICY=0
    export AUTOCANCEL_PORTION=0.8
    export AUTOCANCEL_CRASH=$LOG_PATH/autocancel-log
elif [ $1 == "protego" ]; then
    FUNCTION=inter
    POSTGRE_PATH=$POSTGRE_AUTOCANCEL_PATH
    export SHOULD_KILL=1
    export AUTOCANCEL_POLICY=4
    export AUTOCANCEL_PORTION=0.8
    export AUTOCANCEL_CRASH=$LOG_PATH/autocancel-log
else
    echo "error, please input normal or inter or autocancel or protego"
    exit 1
fi

cp $CASE_PATH/postgresql.conf $POSTGRE_ORIGINAL_PATH/dist/data/postgresql.conf

for i in $(seq 2 2)
do
    $POSTGRE_PATH/dist/bin/postgres -D $POSTGRE_ORIGINAL_PATH/dist/data > $POSTGRE_ORIGINAL_PATH/dist/logfile 2>&1 &
    sleep 10

    $FUNCTION $i

    sleep 10

    $POSTGRE_PATH/dist/bin/pg_ctl stop -D $POSTGRE_ORIGINAL_PATH/dist/data
done

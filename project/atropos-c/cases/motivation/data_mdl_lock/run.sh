#!/bin/bash

AUTOCANCELDIR=/home/cc/autocancel/autocancel-simulation
export LD_LIBRARY_PATH=$AUTOCANCELDIR/build/libs:$LD_LIBRARY_PATH

MYSQL_ORIGINAL_PATH=/home/cc/autocancel/applications/mysql-original
MYSQL_AUTOCANCEL_PATH=/home/cc/autocancel/applications/mysql-autocancel/mysql
SYSBENCH_PATH=/home/cc/autocancel/autocancel-sosp/sysbench
USER=cc
LOG_PATH=/home/cc/autocancel/autocancel-sosp/logs
CASE_PATH=/home/cc/autocancel/autocancel-sosp/cases/motivation/data_mdl_lock
PREPARE_SCRIPT=$CASE_PATH/../prepare.sh

function normal_long() {
    $MYSQL_PATH/dist/bin/mysqld --defaults-file=$CASE_PATH/../my.cnf &
    sleep 5
    for ((i=100; i<=2500; i+=500)); do
    # for ((i=100; i<=3000; i+=100)); do
        echo $i
        cd $SYSBENCH_PATH
        ./src/sysbench ./src/lua/oltp_read_write.lua \
        --mysql-socket=$MYSQL_ORIGINAL_PATH/dist/mysqld.sock \
        --db-driver=mysql \
        --mysql-host=127.0.0.1 \
        --mysql-port=3306 \
        --mysql-user=$USER \
        --mysql-db=test \
        --tables=16 \
        --threads=4 \
        --mysql-ignore-errors=all \
        --table-size=100000 \
        --report-interval=1 \
        --time=60 \
        --rate=$i run > $LOG_PATH/normal-long-$i 2>&1 &
        sleep 30
        cd $CASE_PATH
        ./update_table18.sh $MYSQL_PATH $MYSQL_ORIGINAL_PATH &
        sleep 1
        sleep 60
    done
}

function normal_back() {
    $MYSQL_PATH/dist/bin/mysqld --defaults-file=$CASE_PATH/../my.cnf &
    sleep 5
    for ((i=100; i<=2500; i+=500)); do
    # for ((i=100; i<=3000; i+=100)); do
        echo $i
        cd $SYSBENCH_PATH
        ./src/sysbench ./src/lua/oltp_read_write.lua \
        --mysql-socket=$MYSQL_ORIGINAL_PATH/dist/mysqld.sock \
        --db-driver=mysql \
        --mysql-host=127.0.0.1 \
        --mysql-port=3306 \
        --mysql-user=$USER \
        --mysql-db=test \
        --tables=16 \
        --threads=4 \
        --mysql-ignore-errors=all \
        --table-size=100000 \
        --report-interval=1 \
        --time=60 \
        --rate=$i run > $LOG_PATH/normal-back-$i 2>&1 &
        sleep 30
        cd $CASE_PATH
        ./flush.sh $MYSQL_PATH $MYSQL_ORIGINAL_PATH &
        sleep 1
        sleep 60
    done
}

function interference() {
    export SB_EVT_QUEUE_SIZE=1000000

    $MYSQL_PATH/dist/bin/mysqld --defaults-file=$CASE_PATH/../my.cnf &
    sleep 5
    for ((i=100; i<=3000; i+=1000)); do
    # for ((i=100; i<=3000; i+=100)); do
        echo $i
        cd $SYSBENCH_PATH
        ./src/sysbench ./src/lua/oltp_read_write.lua \
        --mysql-socket=$MYSQL_ORIGINAL_PATH/dist/mysqld.sock \
        --db-driver=mysql \
        --mysql-host=127.0.0.1 \
        --mysql-port=3306 \
        --mysql-user=$USER \
        --mysql-db=test \
        --tables=16 \
        --threads=4 \
        --mysql-ignore-errors=all \
        --table-size=100000 \
        --report-interval=1 \
        --time=60 \
        --rate=$i run > $LOG_PATH/inter2-$i 2>&1 &
        sleep 30
        cd $CASE_PATH
        ./update_table18.sh $MYSQL_PATH $MYSQL_ORIGINAL_PATH &
        sleep 1
        ./flush.sh $MYSQL_PATH $MYSQL_ORIGINAL_PATH &
        sleep 30
        
        sleep 30
    done
}

# Run the prepare script
$PREPARE_SCRIPT


CUR_TIME=$(date +%Y-%m-%d-%H-%M)
LOG_PATH=$LOG_PATH/$CUR_TIME-motivation-mdl-lock-case-$1
mkdir -p $LOG_PATH

# normal or inter or autocancel or error out
if [ $1 == "normal_long" ]; then
    FUNCTION=normal_long
    MYSQL_PATH=$MYSQL_ORIGINAL_PATH
elif [ $1 == "normal_back" ]; then
    FUNCTION=normal_back
    MYSQL_PATH=$MYSQL_ORIGINAL_PATH
elif [ $1 == "inter" ]; then
    FUNCTION=interference
    MYSQL_PATH=$MYSQL_ORIGINAL_PATH
elif [ $1 == "autocancel" ]; then
    FUNCTION=interference
    MYSQL_PATH=$MYSQL_AUTOCANCEL_PATH
    export SHOULD_KILL=1
    export AUTOCANCEL_POLICY=3
    export AUTOCANCEL_PORTION=0.8
    export AUTOCANCEL_CRASH=$LOG_PATH/autocancel-log
elif [ $1 == "protego" ]; then
    FUNCTION=interference
    MYSQL_PATH=$MYSQL_AUTOCANCEL_PATH
    export SHOULD_KILL=1
    export AUTOCANCEL_POLICY=4
    export AUTOCANCEL_PORTION=0.8
    export AUTOCANCEL_CRASH=$LOG_PATH/autocancel-log
else
    echo "error, please input normal-long or normal_back or inter or autocancel or protego"
    exit 1
fi

for i in $(seq 2 2)
do
    $FUNCTION $i
    sleep 5
    pkill -9 sysbench
    pkill -9 mysql
    sleep 5
done
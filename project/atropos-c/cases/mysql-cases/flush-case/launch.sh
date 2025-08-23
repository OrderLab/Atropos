#!/bin/bash

AUTOCANCELDIR=/home/cc/Atropos/project/atropos-c/atropos
export LD_LIBRARY_PATH=$AUTOCANCELDIR/build/libs:$LD_LIBRARY_PATH

MYSQL_AUTOCANCEL_PATH=/home/cc/Atropos/project/atropos-c/atropos-mysql
USER=cc
LOG_PATH=/home/cc/logs
CASE_PATH=/home/cc/Atropos/project/atropos-c/cases/mysql-cases/flush-case
PREPARE_SCRIPT=$CASE_PATH/../prepare.sh


function normal(){
    $MYSQL_PATH/dist/bin/mysqld --defaults-file=$CASE_PATH/../my.cnf &
    sleep 5
    cd $SYSBENCH_PATH
    ./src/sysbench ./src/lua/mysql_flush_case.lua --mysql-socket=$MYSQL_AUTOCANCEL_PATH/dist/mysqld.sock --db-driver=mysql --mysql-port=3306 --mysql-user=$USER --mysql-db=test --mysql-ignore-errors=all --tables=20 --table-size=100000 --threads=16 --time=80 --report-interval=1 run > $LOG_PATH/fc-normal-$1 2>&1 &
    sleep 25
    sleep 2
    sleep 60
    $MYSQL_PATH/dist/bin/mysqladmin -S $MYSQL_AUTOCANCEL_PATH/dist/mysqld.sock -u $USER shutdown
}


function inter(){
    $MYSQL_PATH/dist/bin/mysqld --defaults-file=$CASE_PATH/../my.cnf &
    sleep 5
    cd $SYSBENCH_PATH
    ./src/sysbench ./src/lua/mysql_flush_case.lua --mysql-socket=$MYSQL_PATH/dist/mysqld.sock --db-driver=mysql --mysql-port=3306 --mysql-user=$USER --mysql-db=test --mysql-ignore-errors=all --tables=20 --table-size=100000 --threads=16 --time=80 --report-interval=1 run > $LOG_PATH/fc-inter-$1 2>&1 &
    cd $CASE_PATH
    sleep 25
    ./update_table21.sh $MYSQL_PATH $MYSQL_PATH &
    sleep 1
    ./update_table24.sh $MYSQL_PATH $MYSQL_PATH &
    sleep 1
    ./flush.sh $MYSQL_PATH $MYSQL_PATH &
    sleep 60
    $MYSQL_PATH/dist/bin/mysqladmin -S $MYSQL_PATH/dist/mysqld.sock -u $USER shutdown
}

# Run the prepare script
# $PREPARE_SCRIPT


CUR_TIME=$(date +%Y-%m-%d-%H-%M)
LOG_PATH=$LOG_PATH/$CUR_TIME-mysql-flush-case-$1


mkdir -p $LOG_PATH

# normal or inter or autocancel or error out
if [ $1 == "normal" ]; then
    FUNCTION=normal
    MYSQL_PATH=$MYSQL_AUTOCANCEL_PATH
elif [ $1 == "inter" ]; then
    FUNCTION=inter
    export AUTOCANCEL_CRASH=$LOG_PATH/autocancel-log
    MYSQL_PATH=$MYSQL_AUTOCANCEL_PATH
elif [ $1 == "autocancel" ]; then
    FUNCTION=inter
    MYSQL_PATH=$MYSQL_AUTOCANCEL_PATH
    export SHOULD_KILL=1
    export AUTOCANCEL_POLICY=2
    export AUTOCANCEL_PORTION=0.8
    export AUTOCANCEL_CRASH=$LOG_PATH/autocancel-log
else
    echo "error, please input normal or inter or autocancel"
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
#!/bin/bash

POSTGRE_PATH=/home/cc/postgres
SYSBENCH_PATH=/home/cc/sysbench-autocancel-master

# Step 1: Clean and re-init PostgreSQL data directory
rm -r $POSTGRE_PATH/dist/data
$POSTGRE_PATH/dist/bin/initdb -D $POSTGRE_PATH/dist/data

sleep 5

# Step 2: Start PostgreSQL server
$POSTGRE_PATH/dist/bin/pg_ctl -D $POSTGRE_PATH/dist/data start > $POSTGRE_PATH/dist/logfile

# Step 3: Wait for server to start
sleep 10

# Step 4: Create test database
$POSTGRE_PATH/dist/bin/createdb -p 5432 -h localhost -U cc test

# Step 5: Prepare Sysbench workloads
cd $SYSBENCH_PATH
./src/sysbench ./src/lua/pgsql_read_write.lua --pgsql-host="localhost" --db-driver=pgsql --pgsql-port=5432 --pgsql-user=cc --pgsql-db=test --tables=24 --table-size=20000 --threads=16 --time=80 --report-interval=1 prepare
./src/sysbench ./src/lua/pgsql_read_write.lua --pgsql-host="localhost" --db-driver=pgsql --pgsql-port=5432 --pgsql-user=cc --pgsql-db=test --tables=25 --table-size=1500000 --threads=1 --time=80 --report-interval=1 fake_prepare
./src/sysbench ./src/lua/pgsql_read_write.lua --pgsql-host="localhost" --db-driver=pgsql --pgsql-port=5432 --pgsql-user=cc --pgsql-db=test --tables=26 --table-size=3000000 --threads=1 --time=80 --report-interval=1 fake_prepare
./src/sysbench ./src/lua/pgsql_read_write.lua --pgsql-host="localhost" --db-driver=pgsql --pgsql-port=5432 --pgsql-user=cc --pgsql-db=test --tables=27 --table-size=10000000 --threads=1 --time=80 --report-interval=1 fake_prepare

# Step 6: Stop PostgreSQL server
$POSTGRE_PATH/dist/bin/pg_ctl -D $POSTGRE_PATH/dist/data stop
sleep 5

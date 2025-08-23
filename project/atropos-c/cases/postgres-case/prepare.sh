#!/bin/bash

POSTGRE_PATH=/home/cc/postgres
SYSBENCH_PATH=/home/cc/Atropos/project/atropos-c/sysbench-atropos

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

# Function to check if table exists
check_table_exists() {
    local table_name=$1
    local exists=$($POSTGRE_PATH/dist/bin/psql -h localhost -U cc -d test -t -c "SELECT EXISTS (SELECT FROM information_schema.tables WHERE table_schema = 'public' AND table_name = '$table_name');" | tr -d ' ')
    echo "$exists"
}

# Prepare tables 1-24 if they don't exist
table_exists=$(check_table_exists "sbtest1")
if [ "$table_exists" != "t" ]; then
    echo "Tables sbtest1-24 do not exist. Creating..."
    ./src/sysbench ./src/lua/pgsql_read_write.lua --pgsql-host="localhost" --db-driver=pgsql --pgsql-port=5432 --pgsql-user=cc --pgsql-db=test --tables=24 --table-size=20000 --threads=16 --time=80 --report-interval=1 prepare
else
    echo "Tables sbtest1-24 already exist. Skipping creation."
fi

# Prepare table 25 if it doesn't exist
table_exists=$(check_table_exists "sbtest25")
if [ "$table_exists" != "t" ]; then
    echo "Table sbtest25 does not exist. Creating..."
    ./src/sysbench ./src/lua/pgsql_read_write.lua --pgsql-host="localhost" --db-driver=pgsql --pgsql-port=5432 --pgsql-user=cc --pgsql-db=test --tables=25 --table-size=1500000 --threads=1 --time=80 --report-interval=1 fake_prepare
else
    echo "Table sbtest25 already exists. Skipping creation."
fi

# Prepare table 26 if it doesn't exist
table_exists=$(check_table_exists "sbtest26")
if [ "$table_exists" != "t" ]; then
    echo "Table sbtest26 does not exist. Creating..."
    ./src/sysbench ./src/lua/pgsql_read_write.lua --pgsql-host="localhost" --db-driver=pgsql --pgsql-port=5432 --pgsql-user=cc --pgsql-db=test --tables=26 --table-size=3000000 --threads=1 --time=80 --report-interval=1 fake_prepare
else
    echo "Table sbtest26 already exists. Skipping creation."
fi

# Prepare table 27 if it doesn't exist
table_exists=$(check_table_exists "sbtest27")
if [ "$table_exists" != "t" ]; then
    echo "Table sbtest27 does not exist. Creating..."
    ./src/sysbench ./src/lua/pgsql_read_write.lua --pgsql-host="localhost" --db-driver=pgsql --pgsql-port=5432 --pgsql-user=cc --pgsql-db=test --tables=27 --table-size=10000000 --threads=1 --time=80 --report-interval=1 fake_prepare
else
    echo "Table sbtest27 already exists. Skipping creation."
fi

# Step 6: Stop PostgreSQL server
$POSTGRE_PATH/dist/bin/pg_ctl -D $POSTGRE_PATH/dist/data stop
sleep 5

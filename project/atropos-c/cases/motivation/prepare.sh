#!/bin/bash

set -e

MYSQL_PATH=/home/cc/autocancel/applications/mysql-original
SYSBENCH_PATH=/home/cc/autocancel/autocancel-sosp/sysbench
SCRIPT_PATH=$(cd "$(dirname "$0")"; pwd)

# Fill in my.cnf from template
sed "s#__MYSQL_PATH__#${MYSQL_PATH}#g" ${SCRIPT_PATH}/my.cnf.template > ${SCRIPT_PATH}/my.cnf

# Clean and re-init MySQL data directory
rm -rf ${MYSQL_PATH}/dist/data
mkdir -p ${MYSQL_PATH}/dist/data
# Start MySQL server for the first time to create system tables
cd ${MYSQL_PATH}/dist
${MYSQL_PATH}/dist/scripts/mysql_install_db --user=cc --datadir=${MYSQL_PATH}/dist/data

# Start MySQL server
${MYSQL_PATH}/dist/bin/mysqld --defaults-file=${SCRIPT_PATH}/my.cnf &

# Wait
sleep 5

# Create test DB
${MYSQL_PATH}/dist/bin/mysql -u cc -S ${MYSQL_PATH}/dist/mysqld.sock -e "CREATE DATABASE IF NOT EXISTS test;"

echo "MySQL started and test DB created."


# Prepare the database for sysbench
cd $SYSBENCH_PATH
./src/sysbench ./src/lua/oltp_read_write.lua --mysql-socket=$MYSQL_PATH/dist/mysqld.sock --db-driver=mysql --mysql-port=3306 --mysql-user=cc --mysql-db=test --tables=20 --table-size=100000 --threads=20 --time=300 --report-interval=1 prepare

# Close MySQL server
${MYSQL_PATH}/dist/bin/mysqladmin -u cc -S ${MYSQL_PATH}/dist/mysqld.sock shutdown
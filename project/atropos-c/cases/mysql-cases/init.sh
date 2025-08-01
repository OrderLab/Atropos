#!/bin/bash
set -e

MYSQL_PATH=/home/cc/autocancel/applications/mysql-original
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

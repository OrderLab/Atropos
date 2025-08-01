#!/bin/bash

USER=cc

time=$(date "+%Y%m%d-%H%M%S")
echo $time
$1/dist/bin/mysql -S $2/dist/mysqld.sock -u $USER --force << EOF
use test
update sbtest21 set k=k+1 where id < 15000;
update sbtest21 set k=k+1 where id = 1;
EOF
time=$(date "+%Y%m%d-%H%M%S")
echo $time
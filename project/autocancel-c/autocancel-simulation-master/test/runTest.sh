#!/bin/bash

sudo sysbench /usr/local/share/sysbench/oltp_read_only.lua --db-driver=mysql \
--mysql-user=root --mysql-password=adgjla \
--mysql-db=test --tables=3 --table-size=100000 --threads=2 --time=60 --report-interval=0.1 --percentile=50 run > ./output 2>&1 &

sudo ./test.o
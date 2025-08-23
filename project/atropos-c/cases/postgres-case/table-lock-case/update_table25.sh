#!/bin/bash

USER=cc

time=$(date "+%Y%m%d-%H%M%S")
echo $time
$1/dist/bin/psql  -h localhost -U $USER test -f update_table25.sql
# psql test -f update_table25.sql
time=$(date "+%Y%m%d-%H%M%S")
echo $time
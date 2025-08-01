#!/bin/bash

USER=cc

time=$(date "+%Y%m%d-%H%M%S")
echo $time
$1/dist/bin/mysql -S $2/dist/mysqld.sock -u $USER --force << EOF
flush table with read lock;
unlock tables;
EOF
time=$(date "+%Y%m%d-%H%M%S")
echo $time
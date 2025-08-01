#!/bin/bash

USER=cc

$1/dist/bin/mysql -S $2/dist/mysqld.sock -u $USER --force << EOF
use test
update sbtest18 set k=k+1 where id < 100000;
EOF
#!/bin/bash

USER=cc

$1/dist/bin/mysql -S $2/dist/mysqld.sock -u $USER --force << EOF
flush table with read lock;
unlock tables;
EOF
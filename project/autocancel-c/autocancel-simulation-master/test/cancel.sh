#!/bin/bash

rm /var/lib/mysql-files/cancel.txt
sudo mysql -u root -padgjla << EOF
use test;
select concat('KILL QUERY ',id,';') from INFORMATION_SCHEMA.processlist where Info like "select benchmark%" into outfile '/var/lib/mysql-files/cancel.txt';
source /var/lib/mysql-files/cancel.txt;
EOF
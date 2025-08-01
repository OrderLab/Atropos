mysql -S /data/software/pbox_mysql/dist/mysqld.sock -u root --force << EOF
use test
update sbtest30 set k=k+1 where id < 50000;
select sleep(30);
update sbtest30 set k=k+1 where id = 1;
EOF
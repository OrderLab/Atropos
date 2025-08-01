mysql -S /data/software/pbox_mysql/dist/mysqld.sock -u root --force << EOF
use test;
begin;
select c from sbtest1 limit 32;
select sleep(20);
commit;
begin;
select c from sbtest1 limit 32;
select sleep(120);
commit;
EOF
BEGIN;
lock table sbtest25 in exclusive mode;
update sbtest25 set k=k+1;
commit;
select pg_sleep(30);
update sbtest25 set k=k-1 where id = 1;
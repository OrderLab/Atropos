function normal(){
    mysqld --defaults-file=/users/gongxini/cases/mysql-cases/table-lock-case/my.cnf &
    sleep 5
    sysbench /data/software/sysbench-autocancel/dist/share/sysbench/oltp_read_write.lua --mysql-socket=/data/software/pbox_mysql/dist/mysqld.sock --db-driver=mysql --mysql-port=3306 --mysql-user=root --mysql-db=test --mysql-ignore-errors=all --tables=20 --table-size=30000 --threads=6 --time=120 --report-interval=1 run > ./bfc-normal-$1 2>&1 &
    sleep 125
    mysqladmin -S /data/software/pbox_mysql/dist/mysqld.sock -u root shutdown
}

function inter(){
    mysqld --defaults-file=/users/gongxini/cases/mysql-cases/table-lock-case/my.cnf &
    sleep 10
    sysbench /data/software/sysbench-autocancel/dist/share/sysbench/oltp_read_write.lua --mysql-socket=/data/software/pbox_mysql/dist/mysqld.sock --db-driver=mysql --mysql-port=3306 --mysql-user=root --mysql-db=test --mysql-ignore-errors=all --tables=20 --table-size=30000 --threads=6 --time=120 --report-interval=1 run > ./bfc-inter-$1 2>&1 &
    sleep 50
    echo "start inter"
    mysqldump -S /data/software/pbox_mysql/dist/mysqld.sock -u root test sbtest26 > sbtest26.tmp &
    sleep 75
    mysqladmin -S /data/software/pbox_mysql/dist/mysqld.sock -u root shutdown
}

function parties(){
    mysqld --defaults-file=/users/gongxini/cases/mysql-cases/table-lock-case/my.cnf &
    sleep 10
    sysbench /data/software/sysbench-autocancel/dist/share/sysbench/oltp_read_write.lua --mysql-socket=/data/software/pbox_mysql/dist/mysqld.sock --db-driver=mysql --mysql-port=3306 --mysql-user=root --mysql-db=test --mysql-ignore-errors=all --tables=20 --table-size=30000 --threads=6 --time=120 --report-interval=1 run > ./front_1/parties.log 2>&1 &

    TLIST=$(ps -e -T | grep mysqld | awk '{print $2}' | sort -h | tail -1)
    for T in $TLIST; do (echo "$T") | sudo tee /sys/fs/cgroup/cpuset/hu_front_1/tasks; done

    sleep 50
    echo "start inter"
    echo "Time: 0" >> ./back_1/parties.log
    mysqldump -S /data/software/pbox_mysql/dist/mysqld.sock -u root test sbtest26 > sbtest26.tmp &
    TLIST=$(ps -e -T | grep mysqldump | awk '{print $2}' | sort -h | tail -1)
    for T in $TLIST; do (echo "$T") | sudo tee /sys/fs/cgroup/cpuset/hu_back_1/tasks; done

    sleep 5
    core=$(nproc --all)
    sudo /data/software/parties/parties_for_native.py ./ $core &

    sleep 75
    sudo pkill -f parties_for_native.py
    mysqladmin -S /data/software/pbox_mysql/dist/mysqld.sock -u root shutdown
    cp ./front_1/parties.log ./front_1/parties-$i.log
}

for i in $(seq 2 6)
do
    parties $i
    pkill -9 sysbench
    rm -rf sbtest26.tmp
done
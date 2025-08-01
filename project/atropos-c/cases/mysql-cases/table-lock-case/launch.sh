function normal(){
    mysqld --defaults-file=/users/gongxini/cases/mysql-cases/table-lock-case/my.cnf &
    sleep 5
    sysbench /data/software/sysbench-autocancel/dist/share/sysbench/table_lock_case.lua --mysql-socket=/data/software/pbox_mysql/dist/mysqld.sock --db-driver=mysql --mysql-port=3306 --mysql-user=root --mysql-db=test --mysql-ignore-errors=all --tables=24 --table-size=100000 --threads=16 --time=100 --report-interval=1 run > ./tlc-normal-$1 2>&1 &
    sleep 125
    mysqladmin -S /data/software/pbox_mysql/dist/mysqld.sock -u root shutdown
}

function inter(){
    mysqld --defaults-file=/users/gongxini/cases/mysql-cases/table-lock-case/my.cnf &
    sleep 10
    sysbench /data/software/sysbench-autocancel/dist/share/sysbench/table_lock_case.lua --mysql-socket=/data/software/pbox_mysql/dist/mysqld.sock --db-driver=mysql --mysql-port=3306 --mysql-user=root --mysql-db=test --mysql-ignore-errors=all --tables=24 --table-size=100000 --threads=16 --time=100 --report-interval=1 run > ./tlc-inter-$1 2>&1 &
    sleep 30
    echo "start inter"
    ./update_table24.sh >> /dev/null &
    sleep 95
    mysqladmin -S /data/software/pbox_mysql/dist/mysqld.sock -u root shutdown
}

function parties(){
    mysqld --defaults-file=/users/gongxini/cases/mysql-cases/table-lock-case/my.cnf &
    sleep 10
    sysbench /data/software/sysbench-autocancel/dist/share/sysbench/table_lock_case.lua --mysql-socket=/data/software/pbox_mysql/dist/mysqld.sock --db-driver=mysql --mysql-port=3306 --mysql-user=root --mysql-db=test --mysql-ignore-errors=all --tables=24 --table-size=100000 --threads=16 --time=100 --report-interval=1 run > ./front_1/parties.log 2>&1 &

    TLIST=$(ps -e -T | grep mysqld | awk '{print $2}' | sort -h | tail -1)
    for T in $TLIST; do (echo "$T") | sudo tee /sys/fs/cgroup/cpuset/hu_front_1/tasks; done

    sleep 30
    echo "Time: 0" >> ./back_1/parties.log
    ./update_table24.sh >> /dev/null &
    TLIST=$(ps -e -T | grep mysqld | awk '{print $2}' | sort -h | tail -1)
    for T in $TLIST; do (echo "$T") | sudo tee /sys/fs/cgroup/cpuset/hu_back_1/tasks; done
    sleep 5
    core=$(nproc --all)
    sudo /data/software/parties/parties_for_native.py ./ $core &

    sleep 90
    sudo pkill -f parties_for_native.py
    mysqladmin -S /data/software/pbox_mysql/dist/mysqld.sock -u root shutdown
    cp ./front_1/parties.log ./front_1/parties-$i.log
}

for i in $(seq 3 6)
do
    parties $i
    pkill -9 sysbench
    sleep 10
done
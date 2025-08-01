function normal(){
    mysqld --defaults-file=/users/gongxini/cases/mysql-cases/table-lock-case/my.cnf &
    sleep 5
    sysbench /data/software/sysbench-autocancel/dist/share/sysbench/mysql_undo_log_case.lua --mysql-socket=/data/software/pbox_mysql/dist/mysqld.sock --db-driver=mysql --mysql-port=3306 --mysql-user=root --mysql-db=test --mysql-ignore-errors=all --tables=20 --table-size=100000 --threads=16 --time=150 --report-interval=1 run > ./ulc-normal-$1 2>&1 &
    sleep 25
    sleep 130
    mysqladmin -S /data/software/pbox_mysql/dist/mysqld.sock -u root shutdown
}

function inter(){
    mysqld --defaults-file=/users/gongxini/cases/mysql-cases/table-lock-case/my.cnf &
    sleep 5
    sysbench /data/software/sysbench-autocancel/dist/share/sysbench/mysql_undo_log_case.lua --mysql-socket=/data/software/pbox_mysql/dist/mysqld.sock --db-driver=mysql --mysql-port=3306 --mysql-user=root --mysql-db=test --tables=20 --table-size=100000 --threads=16 --time=150 --report-interval=1 run > ./ulc-inter-$1 2>&1 &
    sleep 20
    ./inter.sh >> /dev/null &
    sleep 135
    mysqladmin -S /data/software/pbox_mysql/dist/mysqld.sock -u root shutdown
}

function parties(){
    mysqld --defaults-file=/users/gongxini/cases/mysql-cases/table-lock-case/my.cnf &
    sleep 5
    sysbench /data/software/sysbench-autocancel/dist/share/sysbench/mysql_undo_log_case.lua --mysql-socket=/data/software/pbox_mysql/dist/mysqld.sock --db-driver=mysql --mysql-port=3306 --mysql-user=root --mysql-db=test --tables=20 --table-size=100000 --threads=16 --time=150 --report-interval=1 run > ./front_1/parties.log 2>&1 &
    TLIST=$(ps -e -T | grep mysqld | awk '{print $2}' | sort -h | tail -1)
    for T in $TLIST; do (echo "$T") | sudo tee /sys/fs/cgroup/cpuset/hu_front_1/tasks; done

    sleep 20
    echo "Time: 0" >> ./back_1/parties.log
    ./inter.sh >> /dev/null &
    TLIST=$(ps -e -T | grep mysqld | awk '{print $2}' | sort -h | tail -1)
    for T in $TLIST; do (echo "$T") | sudo tee /sys/fs/cgroup/cpuset/hu_back_1/tasks; done

    sleep 5
    core=$(nproc --all)
    sudo /data/software/parties/parties_for_native.py ./ $core &

    sleep 135

    sudo pkill -f parties_for_native.py
    mysqladmin -S /data/software/pbox_mysql/dist/mysqld.sock -u root shutdown
    cp ./front_1/parties.log ./front_1/parties-$i.log
}


for i in $(seq 2 6)
do
    parties $i
    pkill -9 sysbench
    pkill -9 mysql
    sleep 5
done
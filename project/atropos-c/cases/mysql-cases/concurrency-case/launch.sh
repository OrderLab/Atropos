function normal(){
    mysqld --defaults-file=/users/gongxini/cases/mysql-cases/concurrency-case/my.cnf &
    sleep 5
    sysbench /data/software/sysbench-autocancel/dist/share/sysbench/oltp_read_write.lua --mysql-socket=/data/software/pbox_mysql/dist/mysqld.sock --db-driver=mysql --mysql-port=3306 --mysql-user=root --mysql-db=test --mysql-ignore-errors=all --tables=20 --table-size=100000 --threads=4 --time=90 --report-interval=1 run > ./cc-normal-$1 2>&1 &
    sleep 95
    mysqladmin -S /data/software/pbox_mysql/dist/mysqld.sock -u root shutdown
}


function inter(){
    mysqld --defaults-file=/users/gongxini/cases/mysql-cases/concurrency-case/my.cnf &
    sleep 5
    sysbench /data/software/sysbench-autocancel/dist/share/sysbench/oltp_read_write.lua --mysql-socket=/data/software/pbox_mysql/dist/mysqld.sock --db-driver=mysql --mysql-port=3306 --mysql-user=root --mysql-db=test --mysql-ignore-errors=all --tables=20 --table-size=100000 --threads=4 --time=90 --report-interval=1 run > ./cc-inter-$1 2>&1 &
    sleep 25
    ./update_table30.sh >> /dev/null &
    ./update_table21.sh >> /dev/null &
    sleep 1
    ./update_table24.sh >> /dev/null &
    sleep 70
    mysqladmin -S /data/software/pbox_mysql/dist/mysqld.sock -u root shutdown
}


function parties(){
    mysqld --defaults-file=/users/gongxini/cases/mysql-cases/concurrency-case/my.cnf &
    sleep 5
    sysbench /data/software/sysbench-autocancel/dist/share/sysbench/oltp_read_write.lua --mysql-socket=/data/software/pbox_mysql/dist/mysqld.sock --db-driver=mysql --mysql-port=3306 --mysql-user=root --mysql-db=test --mysql-ignore-errors=all --tables=20 --table-size=100000 --threads=4 --time=90 --report-interval=1 run > ./front_1/parties.log 2>&1 &
    TLIST=$(ps -e -T | grep mysqld | awk '{print $2}' | sort -h | tail -1)
    for T in $TLIST; do (echo "$T") | sudo tee /sys/fs/cgroup/cpuset/hu_front_1/tasks; done

    sleep 25
    echo "Time: 0" >> ./back_1/parties.log
    ./update_table30.sh >> /dev/null &
    TLIST=$(ps -e -T | grep mysqld | awk '{print $2}' | sort -h | tail -1)
    for T in $TLIST; do (echo "$T") | sudo tee /sys/fs/cgroup/cpuset/hu_back_1/tasks; done


    ./update_table21.sh >> /dev/null &
    TLIST=$(ps -e -T | grep mysqld | awk '{print $2}' | sort -h | tail -1)
    for T in $TLIST; do (echo "$T") | sudo tee -a /sys/fs/cgroup/cpuset/hu_back_1/tasks; done
    sleep 1
    ./update_table24.sh >> /dev/null &
    TLIST=$(ps -e -T | grep mysqld | awk '{print $2}' | sort -h | tail -1)
    for T in $TLIST; do (echo "$T") | sudo tee -a /sys/fs/cgroup/cpuset/hu_back_1/tasks; done

    sleep 5
    core=$(nproc --all)
    sudo /data/software/parties/parties_for_native.py ./ $core &

    sleep 65
    sudo pkill -f parties_for_native.py
    mysqladmin -S /data/software/pbox_mysql/dist/mysqld.sock -u root shutdown
    cp ./front_1/parties.log ./front_1/parties-$i.log
}

for i in $(seq 2 6)
do
    parties $i
    pkill -9 sysbench
done
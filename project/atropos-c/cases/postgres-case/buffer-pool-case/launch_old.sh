function normal(){
    sleep 5
    sysbench /data/software/sysbench-autocancel/dist/share/sysbench/pgsql_buffer_pool_case.lua --pgsql-host=127.0.0.1 --db-driver=pgsql --pgsql-port=5432 --pgsql-user=gongxini --pgsql-db=test --tables=24 --table-size=1000 --threads=16 --time=90 --report-interval=1 run > ./bfc-normal-$1 2>&1 &
    sleep 115
}

function inter(){
    sleep 5
    sysbench /data/software/sysbench-autocancel/dist/share/sysbench/pgsql_buffer_pool_case.lua --pgsql-host=127.0.0.1 --db-driver=pgsql --pgsql-port=5432 --pgsql-user=gongxini --pgsql-db=test --tables=24 --table-size=1000 --threads=16 --time=90 --report-interval=1 run > ./bfc-inter-$1 2>&1 &
    sleep 60
    ./vacuum_table25.sh >> /dev/null &
    sleep 35
}

# function parties(){
#     sleep 5
#     sysbench /data/software/sysbench-autocancel/dist/share/sysbench/pgsql_buffer_pool_case.lua --pgsql-host=127.0.0.1 --db-driver=pgsql --pgsql-port=5432 --pgsql-user=gongxini --pgsql-db=test --tables=24 --table-size=1000 --threads=16 --time=90 --report-interval=1 run > ./front_1/parties.log 2>&1 &

#     TLIST=$(ps -e -T | grep postgre | awk '{print $2}' | sort -h | tail -1)
#     for T in $TLIST; do (echo "$T") | sudo tee /sys/fs/cgroup/cpuset/hu_front_1/tasks; done

#     sleep 60
#     echo "Time: 0" >> ./back_1/parties.log
#     ./vacuum_table25.sh >> /dev/null &
#     TLIST=$(ps -e -T | grep postgre | awk '{print $2}' | sort -h | tail -1)
#     for T in $TLIST; do (echo "$T") | sudo tee /sys/fs/cgroup/cpuset/hu_back_1/tasks; done
#     sleep 5
#     core=$(nproc --all)
#     sudo /data/software/parties/parties_for_native.py ./ $core &


#     sleep 35
#     sudo pkill -f parties_for_native.py
#     cp ./front_1/parties.log ./front_1/parties-$i.log
# }

cp ./postgresql.conf /data/software/autocancel-postgresql/dist/data/postgresql.conf

for i in $(seq 2 6)
do
    postgres -D /data/software/autocancel-postgresql/dist/data/ > /data/software/autocancel-postgresql/dist/logfile 2>&1 &

    sleep 10

    parties $i

    sleep 10

    pg_ctl stop -D /data/software/autocancel-postgresql/dist/data/
done
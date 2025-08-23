# Atropos in C++

This directory contains the C++ version of **Atropos**, along with three applications implemented in C: **MySQL**, **PostgreSQL**, and **Apache**. These applications are modified to integrate with Atropos for demonstrating its effectiveness. It also contains code for **sysbench** which is used to generate workloads.

We will walk through the setup of each application, autocancel and sysbench. And we will show example usage of Atropos with resource overload cases from MySQL and PostgreSQL.


## Build Instructions

**Note: If you chose to use our pre-configured machine, you can skip the build instructions below.**

Follow the instructions below to build autocancel, MySQL, PostgresSQL, Apache and sysbench. 

### Build Atropos

```shell
cd ~/Atropos/project/atropos-c/atropos
mkdir build && cd build
cmake ..
make -j $(nproc)
cd ..

# You could put these in bashrc for convenience
echo "AUTOCANCELDIR=$HOME/Atropos/project/atropos-c/atropos" >> ~/.bashrc
echo "LD_LIBRARY_PATH=$HOME/Atropos/project/atropos-c/atropos/build/libs:$LD_LIBRARY_PATH" >> ~/.bashrc
```

### Build MySQL

```shell
# You may need to install extra packages based on environment
sudo apt update
sudo apt install -y build-essential cmake libncurses5-dev libaio-dev \
  bison zlib1g-dev libtirpc-dev libevent-dev openssl pkg-config \
  libssl-dev 
cd ~/Atropos/project/atropos-c/atropos-mysql
./prepare.sh
echo "LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$HOME/Atropos/project/atropos-c/atropos-mysql/dist/lib" >> ~/.bashrc
```

### Build PostgreSQL

Revise `./configure` under PostgreSQL folder to adjust `AUTOCANCELDIR` path to be the path that contains `atropos`.

```shell
# You may need to install extra packages based on environment
sudo apt update
sudo apt install -y build-essential libreadline-dev zlib1g-dev flex bison \
  libssl-dev libxml2-dev libxslt1-dev libpam0g-dev libedit-dev

cd ~/Atropos/project/atropos-c/atropos-postgresql
./configure --prefix=$(pwd)/dist --with-openssl
make -j4
make install

mkdir ./dist/data
./dist/bin/initdb -D ./dist/data
./dist/bin/pg_ctl -D ./dist/data -l logfile start
./dist/bin/createdb test

./dist/bin/psql test
```

### Build Apache

```shell
cd ~/Atropos/project/atropos-c/atropos-apache
./compile.sh
```

### Build Sysbench

```shell
sudo apt-get install make automake libtool pkg-config libaio-dev libmysqlclient-dev libssl-dev libpq-dev
cd ~/Atropos/project/atropos-c/sysbench-atropos
./autogen.sh
echo "$MYSQL_HOME=$HOME/Atropos/project/atropos-c/atropos-mysql" >> ~/.bashrc
echo "$PGSQL_HOME=$HOME/Atropos/project/atropos-c/atropos-postgresql" >> ~/.bashrc
echo "$SYSBENCH_HOME=~/Atropos/project/atropos-c/sysbench-atropos" >> ~/.bashrc
./configure --prefix=$SYSBENCH_HOME/dist --with-mysql-includes=$MYSQL_HOME/dist/include/ --with-mysql-libs=$MYSQL_HOME/dist/lib/ --with-mysql --with-pgsql --with-pgsql-includes=$PGSQL_HOME/dist/include/ --with-pgsql-libs=$PGSQL_HOME/dist/lib/
make -j4
make install
```

### Setup Atropos

Atropos requires shared memory hash table (shmht) for multiprocess communication. There are four files needed to create for shmht to work. They are defined at `Atropos-c/Atropos/libs/include/shmht.h`

```
#define SHM_PATH "/tmp/map"
#define SHM_LOCK_MAP_PATH "/tmp/lockMap"
#define SHM_CTMAP_RWLOCK_PATH "/tmp/ctMapRWLock"
#define SHM_GLOBLOCKMAP_RWLOCK_PATH "/tmp/globLockMapRWLock"
```

Create these files.

```
touch /tmp/map 
touch /tmp/lockMap
touch /tmp/ctMapRWLock
touch /tmp/globLockMapRWLock
```


## Mitigating Resource Overload Cases

### MySQL Backup Lock Case

Below shows how to run MySQL with Atropos to mitigate resource overload for the backup lock case (case c1 in Table 2 of the paper), where a subtle interaction causes backup queries to hold write locks for a long time.

Navigate to `cases/mysql-cases/flush-case`.

`launch.sh` is a script that automatically runs the sysbench workload in normal, resource overload, and resource overload with Atropos. It will first call `prepare.sh` to prepare the application environment before running the workload.


Update `AUTOCANCELDIR`, `MYSQL_AUTOCANCEL_PATH`, and `SYSBENCH_PATH` in `launch.sh` and `prepare.sh` if needed. 

#### Resource Overload Without Atropos

Invoke the `launch.sh` script with mode `inter` for resource overload without Atropos. It runs an OLTP workload with a large backup query entering around 30 seconds after the workload starts.

```
./launch.sh inter
```

Below is an example log generated from the workload.

```
[ 27.0s ] thds: 16 tps: 589.02 qps: 11844.40 (r/w/o: 8312.28/2354.08/1178.04) lat (ms,95%): 70.55 err/s: 0.00 reconn/s: 0.00
[ 28.0s ] thds: 16 tps: 0.00 qps: 20.00 (r/w/o: 20.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 29.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 30.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 31.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 32.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 33.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 34.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 35.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 36.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 37.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 38.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 39.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 40.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 41.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 42.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 43.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 44.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 45.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 46.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 47.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 48.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 49.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 50.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 51.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 52.0s ] thds: 16 tps: 183.99 qps: 3634.80 (r/w/o: 2517.86/748.96/367.98) lat (ms,95%): 24548.75 err/s: 0.00 reconn/s: 0.00
[ 53.0s ] thds: 16 tps: 306.01 qps: 6112.19 (r/w/o: 4283.13/1217.04/612.02) lat (ms,95%): 121.08 err/s: 0.00 reconn/s: 0.00
[ 54.0s ] thds: 16 tps: 338.00 qps: 6767.05 (r/w/o: 4739.03/1352.01/676.00) lat (ms,95%): 112.67 err/s: 0.00 reconn/s: 0.00
```

We can see that the large backup query blocks all read and writes for more than 20 seconds, significantly affecting application performance.


#### Resource Overload With Atropos

Invoke the `launch.sh` script with mode `autocancel` for resource overload with Atropos to see how the resource overload is mitigated.

```
./launch.sh autocancel
```

Below is an example log generated from the workload.

```
[ 27.0s ] thds: 16 tps: 523.00 qps: 10521.00 (r/w/o: 7382.00/2093.00/1046.00) lat (ms,95%): 108.68 err/s: 0.00 reconn/s: 0.00
[ 28.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 29.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 30.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 31.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 32.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 33.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 34.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 35.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 36.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 37.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 38.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 39.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 40.0s ] thds: 16 tps: 330.49 qps: 6563.02 (r/w/o: 4570.12/1331.92/660.98) lat (ms,95%): 153.02 err/s: 0.00 reconn/s: 0.00
[ 41.0s ] thds: 16 tps: 362.00 qps: 7226.02 (r/w/o: 5058.02/1444.00/724.00) lat (ms,95%): 101.13 err/s: 0.00 reconn/s: 0.00
[ 42.0s ] thds: 16 tps: 427.99 qps: 8534.71 (r/w/o: 5974.80/1703.94/855.97) lat (ms,95%): 104.84 err/s: 0.00 reconn/s: 0.00
[ 43.0s ] thds: 16 tps: 485.00 qps: 9700.04 (r/w/o: 6792.03/1939.01/969.00) lat (ms,95%): 92.42 err/s: 0.00 reconn/s: 0.00
[ 44.0s ] thds: 16 tps: 510.01 qps: 10206.16 (r/w/o: 7141.11/2044.03/1021.02) lat (ms,95%): 90.78 err/s: 0.00 reconn/s: 0.00
[ 45.0s ] thds: 16 tps: 585.01 qps: 11685.14 (r/w/o: 8180.10/2335.03/1170.01) lat (ms,95%): 77.19 err/s: 0.00 reconn/s: 0.00
[ 46.0s ] thds: 16 tps: 565.99 qps: 11369.81 (r/w/o: 7970.87/2266.96/1131.98) lat (ms,95%): 82.96 err/s: 0.00 reconn/s: 0.00
[ 47.0s ] thds: 16 tps: 568.00 qps: 11321.96 (r/w/o: 7913.97/2271.99/1136.00) lat (ms,95%): 86.00 err/s: 0.00 reconn/s: 0.00
[ 48.0s ] thds: 16 tps: 547.02 qps: 10959.41 (r/w/o: 7672.28/2193.08/1094.04) lat (ms,95%): 82.96 err/s: 0.00 reconn/s: 0.00
[ 49.0s ] thds: 16 tps: 567.87 qps: 11346.37 (r/w/o: 7937.15/2273.48/1135.74) lat (ms,95%): 84.47 err/s: 0.00 reconn/s: 0.00
[ 50.0s ] thds: 16 tps: 548.13 qps: 10975.60 (r/w/o: 7694.83/2184.52/1096.26) lat (ms,95%): 95.81 err/s: 0.00 reconn/s: 0.00
[ 51.0s ] thds: 16 tps: 547.55 qps: 10943.91 (r/w/o: 7657.64/2191.18/1095.09) lat (ms,95%): 90.78 err/s: 0.00 reconn/s: 0.00
[ 52.0s ] thds: 16 tps: 530.74 qps: 10585.69 (r/w/o: 7404.27/2119.94/1061.47) lat (ms,95%): 87.56 err/s: 0.00 reconn/s: 0.00
[ 53.0s ] thds: 16 tps: 557.81 qps: 11146.19 (r/w/o: 7798.34/2232.24/1115.62) lat (ms,95%): 89.16 err/s: 0.00 reconn/s: 0.00
[ 54.0s ] thds: 16 tps: 567.83 qps: 11411.67 (r/w/o: 8003.66/2272.34/1135.67) lat (ms,95%): 86.00 err/s: 0.00 reconn/s: 0.00
[ 55.0s ] thds: 16 tps: 576.28 qps: 11492.65 (r/w/o: 8032.95/2307.13/1152.57) lat (ms,95%): 87.56 err/s: 0.00 reconn/s: 0.00
```

Atropos is able to cancel the blocking backup request and restore the normal operations in MySQL.

We can also compare the throughput and latency for resource overload without and with Atropos. Throughput is higher and latency is lower for the case with Atropos, showing the effectiveness of the approach.

```
### Resource Overload w/o Atropos
Throughput:
    events/s (eps):                      373.8742
    time elapsed:                        80.0510s
    total number of events:              29929

Latency (ms):
         min:                                    2.19
         avg:                                   42.78
         max:                                24453.47
         95th percentile:                       89.16
         sum:                              1280503.72
```

```
### Resource Overload w/ Atropos
Throughput:
    events/s (eps):                      476.9290
    time elapsed:                        80.0392s
    total number of events:              38173

Latency (ms):
         min:                                    2.23
         avg:                                   33.54
         max:                                12267.68
         95th percentile:                       86.00
         sum:                              1280228.27
```         


### PostgreSQL Table Lock Case

Below shows how to run PostgreSQL with Atropos to mitigate resource overload for the table lock case (case c6 in Table 2 of the paper), where a write operation slows down the other query due to MVCC.

#### Prepare PostgreSQL Original
The sysbench workload also needs to run on a prepared PostgreSQL environment. The PostgreSQL we've modified has known issues in bootstrapping, so we need to setup PostgreSQL original repo instead. We use version 14.0.

```
cd /home/cc/
git clone https://github.com/postgres/postgres
git checkout REL_14_0

cd postgres
./configure --prefix=$(pwd)/dist --with-openssl
make -j4
make install

mkdir ./dist/data
./dist/bin/initdb -D ./dist/data
./dist/bin/pg_ctl -D ./dist/data -l logfile start
./dist/bin/createdb test

./dist/bin/psql test

```


#### Resource Overload Without Atropos

Navigate to `cases/postgres-case/table-lock-case`.


Update `AUTOCANCELDIR`, `POSTGRE_ORIGINAL_PATH`, `POSTGRE_AUTOCANCEL_PATH`, and `SYSBENCH_PATH` in `launch.sh` and `prepare.sh` if needed. 

Invoke the `launch.sh` script with mode `inter` for resource overload without Atropos and with mode `atropos` for resource overload with Atropos. It runs an OLTP workload with a large write query entering around 40 seconds after the workload starts.

We can again see from the logs that Atropos is able to cancel the large write query in time so that the application workload resumes quickly.

```
### Resource overload without Atropos
[ 41.0s ] thds: 16 tps: 112.56 qps: 2047.93 (r/w/o: 1603.68/219.14/225.11) lat (ms,95%): 4.18 err/s: 0.00 reconn/s: 0.00
[ 42.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 43.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 44.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 45.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 46.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 47.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 48.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 49.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 50.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 51.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 52.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 53.0s ] thds: 16 tps: 5444.92 qps: 97928.56 (r/w/o: 76151.88/10886.84/10889.84) lat (ms,95%): 3.68 err/s: 0.00 reconn/s: 0.00
```

```
### Resource overload with Atropos
[ 41.0s ] thds: 16 tps: 21.01 qps: 410.14 (r/w/o: 323.11/45.02/42.01) lat (ms,95%): 2.52 err/s: 0.00 reconn/s: 0.00
[ 42.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 43.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 44.0s ] thds: 16 tps: 0.00 qps: 0.00 (r/w/o: 0.00/0.00/0.00) lat (ms,95%): 0.00 err/s: 0.00 reconn/s: 0.00
[ 45.0s ] thds: 16 tps: 2336.01 qps: 42003.10 (r/w/o: 32662.08/4669.01/4672.01) lat (ms,95%): 4.91 err/s: 0.00 reconn/s: 0.00
[ 46.0s ] thds: 16 tps: 6715.55 qps: 120878.85 (r/w/o: 94014.67/13433.09/13431.09) lat (ms,95%): 2.97 err/s: 0.00 reconn/s: 0.00
[ 47.0s ] thds: 16 tps: 7133.59 qps: 128395.53 (r/w/o: 99863.19/14265.17/14267.17) lat (ms,95%): 2.61 err/s: 0.00 reconn/s: 0.00
[ 48.0s ] thds: 16 tps: 7152.93 qps: 128765.66 (r/w/o: 100148.96/14311.85/14304.85) lat (ms,95%): 2.61 err/s: 0.00 reconn/s: 0.00
[ 49.0s ] thds: 16 tps: 7094.05 qps: 127682.87 (r/w/o: 99314.67/14179.10/14189.10) lat (ms,95%): 2.52 err/s: 0.00 reconn/s: 0.00
[ 50.0s ] thds: 16 tps: 7163.06 qps: 128915.11 (r/w/o: 100258.86/14330.12/14326.12) lat (ms,95%): 2.52 err/s: 0.00 reconn/s: 0.00
[ 51.0s ] thds: 16 tps: 7488.27 qps: 134794.78 (r/w/o: 104845.71/14973.53/14975.53) lat (ms,95%): 2.48 err/s: 0.00 reconn/s: 0.00
[ 52.0s ] thds: 16 tps: 7493.62 qps: 134899.15 (r/w/o: 104920.67/14990.24/14988.24) lat (ms,95%): 2.43 err/s: 0.00 reconn/s: 0.00
[ 53.0s ] thds: 16 tps: 7466.11 qps: 134348.98 (r/w/o: 104491.54/14925.22/14932.22) lat (ms,95%): 2.48 err/s: 0.00 reconn/s: 0.00
```

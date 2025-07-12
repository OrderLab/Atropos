# Autocancel C Language Project

This Directory include the source code implementation of autocancel, and three instrumented application: mysql, postgresql, apache.

## How to Build

### Build Autocancel

```shell
cd autocancel-simulation
mkdir build && cd build
cmake ..
make -j $(nproc)
cd ..

# You could put these in bashrc for convenience
export AUTOCANCELDIR=$(pwd)/autocancel-simulation
export LD_LIBRARY_PATH=$(pwd)/autocancel-simulation/build/libs:$LD_LIBRARY_PATH
```

### Build MySQL

```shell
# You may need to install extra package based on environment
sudo apt update
sudo apt install -y build-essential cmake libncurses5-dev libaio-dev \
  bison zlib1g-dev libtirpc-dev libevent-dev openssl pkg-config \
  libssl-dev 
cd autocancel-mysql
./prepare.sh
export LD_LIBRARY_PATH=$(pwd)/dist/lib:$LD_LIBRARY_PATH
```

### Build PostgreSQL

**You May Need to Revise ./autocancel-postgresql/configure to adjust AUTOCANCELDIR Path**

```shell
# You may need to install extra package based on environment
sudo apt update
sudo apt install -y build-essential libreadline-dev zlib1g-dev flex bison \
  libssl-dev libxml2-dev libxslt1-dev libpam0g-dev libedit-dev

cd autocancel-postgresql
./configure --prefix=./dist --with-openssl
make
make install

mkdir ./dist/data
./dist/bin/initdb -D ./dist/data
./dist/bin/pg_ctl -D ./dist/data -l logfile start
./dist/bin/createdb test

./dist/bin/psql test
```

### Build Apache

```shell
cd autocancel-apache
./compile.sh
```

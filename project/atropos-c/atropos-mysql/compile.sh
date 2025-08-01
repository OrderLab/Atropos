#!/bin/bash

mkdir -p {build,dist}
cd build

#cmake ../mysql  -DCMAKE_INSTALL_PREFIX=`pwd`/../dist \
#  -DMYSQL_DATADIR=`pwd`/../dist/data -DWITH_DEBUG=1 -DCMAKE_BUILD_TYPE=DEBUG \
#  -DCMAKE_C_FLAGS_DEBUG="-g -O0" -DCMAKE_CXX_FLAGS_DEBUG="-g -O0" \
#  -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DMYSQL_MAINTAINER_MODE=false -DSYSCONFDIR=`pwd`/../dist
cmake ../mysql  -DCMAKE_INSTALL_PREFIX=`pwd`/../dist \
  -DMYSQL_DATADIR=`pwd`/../dist/data \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DMYSQL_MAINTAINER_MODE=false -DSYSCONFDIR=`pwd`/../dist

make -j4
if [ $? -ne 0 ]; then
  echo "Failed to build mysql"
  exit 1
fi
make install

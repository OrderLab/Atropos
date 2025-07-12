#!/bin/bash


mkdir -p dist
cd 2.4.38

cd srclib
wget https://dlcdn.apache.org/apr/apr-1.7.0.tar.gz
tar -xzvf apr-1.7.0.tar.gz
mv apr-1.7.0 apr

wget https://dlcdn.apache.org//apr/apr-util-1.6.1.tar.gz
tar -xzvf apr-util-1.6.1.tar.gz
mv apr-util-1.6.1 apr-util

cd ..
./buildconf

./configure --prefix=$(pwd)/../dist --with-included-apr --enable-so --enable-cgi --with-mpm=prefork

make -j4
if [ $? -ne 0 ]; then
    echo "Failed to build apache"
    exit 1
fi
make install
cd ..

#git submodule update --init --recursive
#cd perfIsolation-Apache-mod_fcgid
#APXS=$(pwd)/../dist/bin/apxs ./configure.apxs
#make
#if [ $? -ne 0 ]; then
#    echo "Failed to build apache"
#    exit 1
#fi
#make install

#cd ..
cd php-7.4.23
./buildconf --force
./configure --with-apxs2=$(pwd)/../dist/bin/apxs --prefix=$(pwd)/../dist/php --enable-fpm
make -j4 
if [ $? -ne 0 ]; then
    echo "Failed to build apache"
    exit 1
fi
make install

cd ..
echo "export PSANDBOX_APACHE_DIR=`pwd`/dist" >> $HOME/.bashrc
echo "export PATH=`pwd`/dist/bin:$PATH" >> $HOME/.bashrc

#!/bin/bash

# install etcd
echo "Starting setup etcd environment"
git clone -b cancel git@github.com:You2Xi2/etcd.git cancel
cd cancel/
make
cd ..
git clone git@github.com:You2Xi2/etcd.git client
cd client/
go install -v ./tools/benchmark
benchmark_PATH=$(go list -f "{{.Target}}" ./tools/benchmark)
mv $benchmark_PATH ../cancel/bin
go get go.etcd.io/etcd/client/v3
cd ..

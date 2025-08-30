#!/bin/bash

set -e

export AUTOCANCEL_HOME=$(git rev-parse --show-toplevel)

if [ ! -d "$AUTOCANCEL_HOME/scripts/data/elasticsearch" ]; then
    mkdir $AUTOCANCEL_HOME/scripts/data/elasticsearch
    pushd $AUTOCANCEL_HOME/scripts/data/elasticsearch
    wget https://github.com/easonycliu/AutoCancelProject/releases/download/v1.0-es-data/xaa
    wget https://github.com/easonycliu/AutoCancelProject/releases/download/v1.0-es-data/xab
    wget https://github.com/easonycliu/AutoCancelProject/releases/download/v1.0-es-data/xac
    wget https://github.com/easonycliu/AutoCancelProject/releases/download/v1.0-es-data/xad

    cat xa* > elasticsearch_doc.tar.gz
    tar -xzf elasticsearch_doc.tar.gz
    mv single_node/* .
    popd
fi

if [ ! -d "$AUTOCANCEL_HOME/scripts/data/solr" ]; then
    mkdir $AUTOCANCEL_HOME/scripts/data/solr
    pushd $AUTOCANCEL_HOME/scripts/data/solr
    wget https://github.com/easonycliu/AutoCancelProject/releases/download/v1.0-solr-data/solr_exp.tar.gz

    tar -xzf solr_exp.tar.gz
    mv single_node_v9.0/* .
    popd
fi

if [ ! -d "$AUTOCANCEL_HOME/scripts/data/solr_bench_home" ]; then
    mkdir $AUTOCANCEL_HOME/scripts/data/solr_bench_home
    pushd $AUTOCANCEL_HOME/scripts/data/solr_bench_home
    wget https://github.com/easonycliu/AutoCancelProject/releases/download/v1.0-solr-bench-data/xaa
    wget https://github.com/easonycliu/AutoCancelProject/releases/download/v1.0-solr-bench-data/xab
    wget https://github.com/easonycliu/AutoCancelProject/releases/download/v1.0-solr-bench-data/xac
    wget https://github.com/easonycliu/AutoCancelProject/releases/download/v1.0-solr-bench-data/xad
    wget https://github.com/easonycliu/AutoCancelProject/releases/download/v1.0-solr-bench-data/xae
    wget https://github.com/easonycliu/AutoCancelProject/releases/download/v1.0-solr-bench-data/xaf

    cat xa* > solr_bench.tar.gz
    tar -xzf solr_bench.tar.gz
    mv suites/* .
	mv stress-facets-local-autocancel.json stress-facets-local-autocancel-base.json
    popd
fi

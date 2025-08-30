#!/bin/bash
set -m
set -e

export AUTOCANCEL_HOME=$(git rev-parse --show-toplevel)

if [ ! -f "$AUTOCANCEL_HOME/autocancel_exp/elasticsearch_exp/query/boolean_search_1.json" ]; then
    docker run --rm --net=host -v $AUTOCANCEL_HOME/autocancel_exp/elasticsearch_exp:/root -w /root easonliu12138/es_py_env:v1.1 /root/performance_issues/complex_boolean_operations.py 1500 boolean_search_1.json
fi

if [ ! -f "$AUTOCANCEL_HOME/autocancel_exp/elasticsearch_exp/query/boolean_search_2.json" ]; then
    docker run --rm --net=host -v $AUTOCANCEL_HOME/autocancel_exp/elasticsearch_exp:/root -w /root easonliu12138/es_py_env:v1.1 /root/performance_issues/complex_boolean_operations.py 4500 boolean_search_2.json
fi

if [ ! -f "$AUTOCANCEL_HOME/autocancel_exp/elasticsearch_exp/query/bulk_large_document.json" ]; then
    docker run --rm --net=host -v $AUTOCANCEL_HOME/autocancel_exp/elasticsearch_exp:/root -w /root easonliu12138/es_py_env:v1.1 /root/performance_issues/large_document_write.py 800000000 bulk_large_document.json
fi

if [ ! -f "$AUTOCANCEL_HOME/autocancel_exp/solr_exp/query/boolean_search_1.json" ]; then
    docker run --rm --net=host -v $AUTOCANCEL_HOME/autocancel_exp/solr_exp:/root -w /root easonliu12138/es_py_env:v1.1 /root/performance_issues/complex_boolean_operations.py 80000 boolean_search_1.json
fi

if [ ! -f "$AUTOCANCEL_HOME/autocancel_exp/solr_exp/query/boolean_search_2.json" ]; then
    docker run --rm --net=host -v $AUTOCANCEL_HOME/autocancel_exp/solr_exp:/root -w /root easonliu12138/es_py_env:v1.1 /root/performance_issues/complex_boolean_operations.py 80000 boolean_search_2.json
fi

if [ ! -f "$AUTOCANCEL_HOME/autocancel_exp/solr_exp/query/boolean_search_interfere.json" ]; then
    docker run --rm --net=host -v $AUTOCANCEL_HOME/autocancel_exp/solr_exp:/root -w /root easonliu12138/es_py_env:v1.1 /root/performance_issues/complex_boolean_operations.py 100000 boolean_search_interfere.json
fi


#!/bin/bash
set -m
set -e

export AUTOCANCEL_HOME=$(git rev-parse --show-toplevel)

if [ $(docker images | grep "solr_exp" | wc -l) -eq 0 ]; then
    docker build --build-arg SOLR_ID=$(id -u) -t solr_exp:v1.0-9.0.0 $AUTOCANCEL_HOME/scripts/images/solr_exp
fi

if [ $(docker images | grep "rally_exp" | wc -l) -eq 0 ]; then
    docker build --build-arg UID=$(id -u) -t rally_exp:v1.0 $AUTOCANCEL_HOME/scripts/images/rally_exp
fi


#!/bin/bash

set -e

export AUTOCANCEL_HOME=$(git rev-parse --show-toplevel)

if [ -d "$AUTOCANCEL_HOME/elasticsearch/build/distribution/local/elasticsearch-8.9.0-SNAPSHOT" ]; then
    sudo chown -R $(id -u):$(id -u) $AUTOCANCEL_HOME/elasticsearch/build/distribution/local/elasticsearch-8.9.0-SNAPSHOT
fi

pushd $AUTOCANCEL_HOME/autocancel_java_code
./gradlew build
popd

pushd $AUTOCANCEL_HOME/scripts/baseline/psandbox/toy_sandbox
./gradlew build
popd

cp $AUTOCANCEL_HOME/autocancel_java_code/lib/build/libs/autocancel-0.0.1.jar $AUTOCANCEL_HOME/elasticsearch/localRepo
cp $AUTOCANCEL_HOME/scripts/baseline/psandbox/toy_sandbox/lib/build/libs/toysandbox-0.0.1.jar $AUTOCANCEL_HOME/elasticsearch/localRepo

pushd $AUTOCANCEL_HOME/elasticsearch
./gradlew --write-verification-metadata sha256 localDistro
popd

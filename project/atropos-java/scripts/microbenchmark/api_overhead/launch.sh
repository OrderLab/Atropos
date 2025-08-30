#!/bin/bash

set -e
set -m

export AUTOCANCEL_HOME=$(git rev-parse --show-toplevel)

javac -cp ''$AUTOCANCEL_HOME'/autocancel_java_code/lib/build/libs/autocancel-0.0.1.jar:.' ApiOverhead.java
java -Dautocancel.app=solr -Dcancel.enable=false -cp ''$AUTOCANCEL_HOME'/autocancel_java_code/lib/build/libs/autocancel-0.0.1.jar:.' ApiOverhead

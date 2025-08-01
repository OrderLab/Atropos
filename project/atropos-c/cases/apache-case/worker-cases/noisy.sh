#!/bin/bash

ab -s 60 -t 110 -c 1 -n 1 -k http://127.0.0.1:8080/index.html\?name\=a
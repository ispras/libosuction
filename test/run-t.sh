#!/bin/sh -

cd $(dirname $(realpath $0))

./run-scripts-1.sh "t-*.sh" "$@"

#!/bin/sh -
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2018 ISP RAS (http://ispras.ru/en)

cd $(dirname $(realpath $0))

./run-scripts-1.sh "t-*.sh" "$@"

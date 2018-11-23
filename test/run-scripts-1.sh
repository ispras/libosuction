#!/bin/sh -
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2018 ISP RAS (http://ispras.ru/en)

cd $(dirname $(realpath $0))

globstr="$1"
test "$2" = "-k" && keep_going=true

die() {
  echo "$s test/check script failed"
  if ! test "$keep_going" = "true"; then
    exit 1
  fi
}

for s in $globstr
do
  . ./$s
done

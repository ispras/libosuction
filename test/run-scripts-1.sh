#!/bin/sh -

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

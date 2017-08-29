#!/bin/bash -

usage () {
cat <<EOF
Usage: $0 [-ld <path-to-ld>] [-passes <passes>] <build-script>

This script takes an actual build script (or command) as its argument.  It runs
the command three times in the course of carrying out the three build passes.

This script uses values from config.mak generated by configure, so you have to
run that first.

If -ld option is supplied path-to-ld will be used as a linker. Otherwise the ld
from the plugdir ($plugdir/ld/ld) is used; if it cannot be found the system ld
is used (it must be new enough to support linker plugins).

If -passes option (-p for short) is supplied only the specified build passes
are executed. E.g. use '-p 12' to skip the 0-th pass (the default is 012 i.e.
all passes).
EOF
exit 0
}


cleanup() {
  test -n "$dpid" && kill $dpid

  if [ -z "$dont_rm_real" -a -n "$gcc_real" -a -e "$gcc_real" ]; then
    mv $gcc_real $gcc
    mv $gxx_real $gxx
  fi
}

trap "cleanup" EXIT

die() {
  [ -n "$1" ] && echo "$1" || echo "Fail after build pass $pass."
  echo "Exiting."
  exit 1
}

setwrappers() {
  cp $util/gcc-wrapper-$pass $gcc
  cp $util/gcc-wrapper-$pass $gxx

  if [ "$pass" = 0 ]; then
    cp $real_ld $lddir/ld
  else
    cp $util/wrapper-$pass $lddir/ld
  fi
}

build() {
  cd - > /dev/null
  stdcxx_path=$(dirname $($gxx -print-file-name=libstdc++.so))

  CC=$gcc CXX=$gxx \
  PATH="$lddir:$PATH" \
  LD_LIBRARY_PATH="$stdcxx_path:$LD_LIBRARY_PATH" \
    $bldcmd || die "Build pass $pass failed."
  cd - > /dev/null
}

buildpass() {
  pass=$1
  setwrappers
  build
}

shallrun() {
  echo "$passes" | grep "$1" >/dev/null
  return $?
}

passes="012"
while test $# -gt 0
do
  case "$1" in
    --help|-help|-h) usage ;;
    --passes|-passes|-p) passes=$2 ; shift ;;
    --ld|-ld) real_ld=$2 ; shift ;;
    *) bldcmd="$1" ;;
  esac
  shift
done

test -n "$bldcmd" ||
  die "Please pass a valid build command. Run $0 -h for help."
file "$bldcmd" &>/dev/null && test -x "$bldcmd" && bldcmd=$(realpath $bldcmd)

test -z "$real_ld" && real_ld=$plugdir/ld/ld
test -e "$real_ld" || real_ld=$(which ld)

srcroot="$(dirname $(realpath $0))/.."
cfg=$srcroot/config.mak
util=$srcroot/util

test -e "$cfg" || die "Please run configure first."
gcc=$(grep 'CC =' $cfg | awk '{ print $3 }')
gxx=$(grep 'CXX =' $cfg | awk '{ print $3 }')
plugdir=$(grep 'plugdir =' $cfg | awk '{ print $3 }')
suffix=$(grep ORIG_CMD_SUFFIX $util/gcc-wrapper.c |
              head -1 | awk '{ print $3 }' | tr -d '"')
gcc_real=$gcc$suffix
gxx_real=$gxx$suffix

if [ ! -e "$gcc_real" ]; then
  cp $gcc $gcc_real
  cp $gxx $gxx_real
else
  dont_rm_real=1
  die "$gcc_real already exists."
fi


tmpdir=$(mktemp -d "$plugdir/temps.XXX")
lddir="$tmpdir"

cd $tmpdir
{
  $util/daemon &
} || die "Cannot start the daemon."
dpid=$!


if shallrun 0; then
  buildpass 0
  cat jfunc-* > jfunc-all
  $util/jf2sign jfunc-all $plugdir/dlsym-signs.txt > $plugdir/sign-all || die
fi
if shallrun 1; then
  buildpass 1
  merged=$plugdir/merged.vis
  $util/merge deps-* > $merged || die "Merge failed."
  amended=$($util/amend-merge-output.sh $merged)
  mv $amended $plugdir/merged.vis.gcc
fi
if shallrun 2; then
  buildpass 2
fi

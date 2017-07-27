#!/bin/bash
PLUGIN_ARG="-fplugin-arg-libplug-sign-dlsym=1 -fplugin-arg-libplug-in=signs.txt"

function compile_test() {
  echo "COMPILING $TFILE"
  $TCOMPILER $TFILE $TFLAGS
}

function dump_contains() {
  echo $(grep -cPR "$2" $1)
}

function dump_check_test() {
  local name="$1"
  local str="$2"
  local expected="$3"
  printf "TESTING $TFILE ($name):\t"
  local actual=$(dump_contains $TFILE.*i.$TPASS "$str")
  if [ $expected -eq $actual ]
  then
    echo "PASS"
  else
    echo "FAIL: Expected $expected but actual is $actual"
  fi
}

function cleanup_test() {
  rm $TFILE.* *.out
}

# jfunc--pass.c
TPASS="jfunc"
echo "----------------"
echo "jfunc-pass tests"
echo "----------------"

TFLAGS="-fplugin=$PLUGIN $PLUGIN_ARG -fdump-ipa-all -ldl"

## C tests
TCOMPILER=$CC

### asm_name.c
TFILE="asm_name.c"

compile_test
dump_check_test "jf1" "asm_func_caller,1->dlsym,1" 1
dump_check_test "jf2" "asm_func_caller,0->dlopen,0" 1
cleanup_test

### cycle_cgraph.c
TFILE="cycle_cgraph.c"

compile_test
dump_check_test "jf1" "caller2,1->dlsym,1" 1
dump_check_test "jf2" "caller2,0->dlopen,0" 1
dump_check_test "jf3" "caller2,0->caller1,0" 1
dump_check_test "jf4" "caller2,1->caller1,1" 1
dump_check_test "jf5" "caller1,0->caller2,0" 1
dump_check_test "jf6" "caller1,1->caller2,1" 1
cleanup_test

## C++ tests
TCOMPILER=$CXX

### overriden.cpp
TFILE="overridden.cpp"

compile_test
dump_check_test "jf1" "_ZN12interseption22GetRealFunctionAddressEPKcd,0->_ZN12interseption22GetRealFunctionAddressEPKcS1_,0" 1
dump_check_test "jf2" "_ZN12interseption22GetRealFunctionAddressEPKcS1_,0->_ZN12interseption22GetRealFunctionAddressEPKc,0" 1
dump_check_test "jf3" "_ZN12interseption22GetRealFunctionAddressEPKc,1->dlsym,0" 1
cleanup_test

exit 0

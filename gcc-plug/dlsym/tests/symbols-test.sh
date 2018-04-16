#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2018 ISP RAS (http://ispras.ru/en)
PLUGIN_ARG="-fplugin-arg-libplug-sign-dlsym=1 -fplugin-arg-libplug-in=signs.txt"

COUNTER_PASSED=0
COUNTER_TOTAL=0

function compile_test() {
  echo "COMPILING $TFILE"
  $TCOMPILER $TFILE $TFLAGS &> $TFILE.log
}

function dump_contains() {
  echo $(grep -cPR "$2" $1)
}

function dump_check_test() {
  local name="$1"
  local str="$2"
  local expected="$3"
  COUNTER_TOTAL=$((COUNTER_TOTAL + 1))
  printf "TESTING $TFILE ($name):"
  local actual=$(dump_contains $TFILE.*i.$TPASS "$str")
  if [ $expected -eq $actual ]
  then
    COUNTER_PASSED=$((COUNTER_PASSED + 1))
    echo "PASS"
  else
    echo "FAIL: Expected $expected but actual is $actual"
  fi
}

function log_check_test() {
  local name="$1"
  local str="$2"
  local expected="$3"
  COUNTER_TOTAL=$((COUNTER_TOTAL + 1))
  printf "TESTING $TFILE ($name):"
  local actual=$(dump_contains $TFILE.log "$str")
  if [ $expected -eq $actual ]
  then
    COUNTER_PASSED=$((COUNTER_PASSED + 1))
    echo "PASS"
  else
    echo "FAIL: Expected $expected but actual is $actual"
  fi
}

function cleanup_test() {
  rm $TFILE.* *.o
}

# symbols-pass.c
TPASS="symbols"
echo "------------------"
echo "symbols-pass tests"
echo "------------------"

TFLAGS="-c -O2 -fplugin=$PLUGIN $PLUGIN_ARG -fdump-ipa-all -ldl"

## C tests
TCOMPILER=$CC

### array.c
TFILE="array.c"

compile_test
dump_check_test "sign" "dlsym matched to the signature" 1
dump_check_test "const" "dlsym set state:CONSTANT" 1
dump_check_test "symbol" "main->dlsym->\[bar\]" 1
cleanup_test

### array2d.c
TFILE="array2d.c"

compile_test
dump_check_test "sign" "dlsym matched to the signature" 1
dump_check_test "const" "dlsym set state:CONSTANT" 1
dump_check_test "symbol" "main->dlsym->\[bar1\]" 1
cleanup_test

### array_assign.c
TFILE="array_assign.c"

compile_test
dump_check_test "sign" "dlsym matched to the signature" 1
dump_check_test "const" "dlsym set state:CONSTANT" 1
dump_check_test "symbol" "main->dlsym->\[init\]" 1
cleanup_test

### array_assign_part.c
TFILE="array_assign_part.c"

compile_test
dump_check_test "sign" "dlsym matched to the signature" 1
dump_check_test "part-const" "dlsym set state:PARTIALLY_CONSTANT" 1
dump_check_test "symbol" "main->dlsym->\[_init\]" 1
cleanup_test

### array_const_idx_sens.c
TFILE="array_const_idx_sens.c"

compile_test
dump_check_test "sign" "sqlite3_load_extension matched to the signature" 1
dump_check_test "dyn" "sqlite3_load_extension set state:DYNAMIC" 1
cleanup_test

### array_var_idx_sens.c
TFILE="array_var_idx_sens.c"

compile_test
dump_check_test "sign" "sqlite3_load_extension matched to the signature" 1
dump_check_test "dyn" "sqlite3_load_extension set state:DYNAMIC" 1
cleanup_test

### asm_name.c
TFILE="asm_name.c"

compile_test
dump_check_test "sign" "dlsym matched to the signature" 1
dump_check_test "const" "dlsym set state:CONSTANT" 1
dump_check_test "wrong symbol" "\*asm_func_caller->dlsym->\[bar\]" 0
dump_check_test "symbol" "asm_func_caller->dlsym->\[bar\]" 1
cleanup_test

### const_array.c
TFILE="const_array.c"

compile_test
dump_check_test "sign" "dlsym matched to the signature" 1
dump_check_test "const" "dlsym set state:CONSTANT" 1
dump_check_test "symbol" "main->dlsym->\[bar\]" 1
cleanup_test

### const_array2.c
TFILE="const_array2.c"

compile_test
dump_check_test "sign" "dlsym matched to the signature" 1
dump_check_test "const" "dlsym set state:CONSTANT" 1
dump_check_test "symbol" "main->dlsym->\[(foo,zoo)|(zoo,foo)\]" 1
cleanup_test

### const_array3.c
TFILE="const_array3.c"

compile_test
dump_check_test "sign" "dlsym matched to the signature" 1
dump_check_test "const" "dlsym set state:CONSTANT" 1
dump_check_test "symbol" "main->dlsym->\[(foo,bar)|(bar,foo)\]" 1
cleanup_test

### const_array4.c
TFILE="const_array4.c"

compile_test
dump_check_test "sign" "dlsym matched to the signature" 1
dump_check_test "const" "dlsym set state:CONSTANT" 1
dump_check_test "symbol" "main->dlsym->\[(foo,bar)|(bar,foo)\]" 1
cleanup_test

### const_array5.c
TFILE="const_array5.c"

compile_test
dump_check_test "sign" "dlsym matched to the signature" 1
dump_check_test "part-const" "dlsym set state:PARTIALLY_CONSTANT" 1
dump_check_test "symbol" "main->dlsym->\[(foo,bar)|(bar,foo)\]" 1
cleanup_test

### const_array6.c
TFILE="const_array6.c"

compile_test
dump_check_test "sign" "dlsym matched to the signature" 1
dump_check_test "const" "dlsym set state:CONSTANT" 1
dump_check_test "symbol" "main->dlsym->\[(bar1,bar)|(bar,bar1)\]" 1
cleanup_test

### const_array7.c
TFILE="const_array7.c"

compile_test
dump_check_test "sign" "dlsym matched to the signature" 1
dump_check_test "dyn" "dlsym set state:DYNAMIC" 1
cleanup_test

### cycle_cgraph.c
TFILE="cycle_cgraph.c"

compile_test
dump_check_test "sign" "dlsym matched to the signature" 1
dump_check_test "const" "dlsym set state:CONSTANT" 1
dump_check_test "symbol" "caller2->dlsym->\[bar\]" 1
cleanup_test

### cycle_cgraph2.c
TFILE="cycle_cgraph2.c"

compile_test
dump_check_test "sign" "dlsym matched to the signature" 1
dump_check_test "const" "dlsym set state:CONSTANT" 1
dump_check_test "symbol" "function_caller->dlsym->\[bar\]" 1
cleanup_test

### const_struct.c
TFILE="const_struct.c"

compile_test
dump_check_test "sign" "dlsym matched to the signature" 1
dump_check_test "const" "dlsym set state:CONSTANT" 1
dump_check_test "sym" "main->dlsym->\[(baz,bar)|(bar,baz)\]" 1
cleanup_test

### conditional.c
TFILE="conditional.c"

compile_test
dump_check_test "sign" "dlsym matched to the signature" 1
dump_check_test "const" "dlsym set state:CONSTANT" 1
dump_check_test "sym" "main->dlsym->\[(baz,bar)|(bar,baz)\]" 1
cleanup_test

### dlsym_fake.c
TFILE="dlsym_fake.c"

compile_test
log_check_test "warning" "9:32: warning: an inconsistent call of ‘dlsym’ is found, a symbol is expected at the argument position ‘1’" 1
cleanup_test

### dlsym_ref.c
TFILE="dlsym_ref.c"

compile_test
log_check_test "warning" "12:17: warning: the address of ‘dlsym’ is used" 1
log_check_test "warning" "13:18: warning: the address of ‘dlsym’ is used" 1
cleanup_test

### dlvsym.c
TFILE="dlvsym.c"

compile_test
dump_check_test "sign" "dlvsym matched to the signature" 1
dump_check_test "const" "dlvsym set state:CONSTANT" 1
dump_check_test "symbol" "main->dlvsym->\[bar\]" 1
cleanup_test

### func_call.c
TFILE="func_call.c"

compile_test
dump_check_test "sign" "dlsym matched to the signature" 1
dump_check_test "dyn" "dlsym set state:DYNAMIC" 1
cleanup_test

### func_macro.c
TFILE="func_macro.c"

compile_test
dump_check_test "sign" "dlsym matched to the signature" 1
dump_check_test "const" "dlsym set state:CONSTANT" 1
dump_check_test "symbol" "foo->dlsym->\[foo\]" 1
cleanup_test

### global_var.c
TFILE="global_var.c"

compile_test
dump_check_test "sign" "dlsym matched to the signature" 1
dump_check_test "const" "dlsym set state:CONSTANT" 1
dump_check_test "symbol" "main->dlsym->\[foo\]" 1
cleanup_test

### global_var2.c
TFILE="global_var2.c"

compile_test
dump_check_test "sign" "dlsym matched to the signature" 1
dump_check_test "part-const" "dlsym set state:PARTIALLY_CONSTANT" 1
dump_check_test "symbol" "main->dlsym->\[(foo,goo)|(goo,foo)\]" 1
cleanup_test

### ifcallopt.c
TFILE="ifcallopt.c"

compile_test
dump_check_test "sign" "dlsym matched to the signature" 2
dump_check_test "const" "dlsym set state:CONSTANT" 1
dump_check_test "undef" "dlsym set state:UNDEFINED" 1
dump_check_test "symbol" "function_caller.constprop.0->dlsym->\[func_name\]" 1
cleanup_test

### local_array.c
TFILE="local_array.c"

compile_test
dump_check_test "sign" "dlsym matched to the signature" 1
dump_check_test "const" "dlsym set state:CONSTANT" 1
dump_check_test "symbol" "main->dlsym->\[bar\]" 1
cleanup_test

### macro_passing.c
TFILE="macro_passing.c"

compile_test
dump_check_test "sign" "dlsym matched to the signature" 2
dump_check_test "const" "dlsym set state:CONSTANT" 2
dump_check_test "symbol" "main->dlsym->\[foo\]" 1
dump_check_test "symbol" "main->dlsym->\[bar\]" 1
cleanup_test

### mux_wrapper.c
TFILE="mux_wrapper.c"

compile_test
dump_check_test "sign" "mux_dlsym matched to the signature" 2
dump_check_test "const" "mux_dlsym set state:CONSTANT" 2
dump_check_test "symbol" "main->mux_dlsym->\[foo\]" 1
dump_check_test "symbol" "main->mux_dlsym->\[bar\]" 1
cleanup_test

### static_proxy.c
TFILE="static_proxy.c"

compile_test
dump_check_test "sign" "dlsym matched to the signature" 1
dump_check_test "const" "dlsym set state:CONSTANT" 1
dump_check_test "symbol" "function_caller.constprop.0->dlsym->\[bar\]" 1
cleanup_test

### static_proxy1.c
TFILE="static_proxy1.c"

compile_test
dump_check_test "sign" "dlsym matched to the signature" 1
dump_check_test "const" "dlsym set state:CONSTANT" 1
dump_check_test "symbol" "function_caller.constprop.0->dlsym->\[(bar,baz)|(baz,bar)\]" 1
cleanup_test

### static_proxy2.c
TFILE="static_proxy2.c"

compile_test
dump_check_test "sign" "dlsym matched to the signature" 1
dump_check_test "const" "dlsym set state:CONSTANT" 1
dump_check_test "symbol" "function_caller.constprop.1->dlsym->\[baz\]" 1
cleanup_test

### switch.c
TFILE="switch.c"

compile_test
dump_check_test "sign" "dlsym matched to the signature" 1
dump_check_test "const" "dlsym set state:CONSTANT" 1
dump_check_test "sym" "main->dlsym->\[(baz,foo)|(foo,baz)\]" 1
cleanup_test

### switch2.c
TFILE="switch2.c"

compile_test
dump_check_test "sign" "dlsym matched to the signature" 1
dump_check_test "const" "dlsym set state:CONSTANT" 1
dump_check_test "symbol" "function_caller.constprop.0->dlsym->\[(foo,baz)|(baz,foo)\]" 1
cleanup_test

### undef.c
TFILE="undef.c"

compile_test
dump_check_test "sign" "dlsym matched to the signature" 1
dump_check_test "undef" "dlsym set state:UNDEFINED" 1
cleanup_test

### var.c
TFILE="var.c"

compile_test
dump_check_test "sign" "dlsym matched to the signature" 1
dump_check_test "const" "dlsym set state:CONSTANT" 1
dump_check_test "symbol" "main->dlsym->\[bar\]" 1
cleanup_test

### wrapper.c
TFILE="wrapper.c"

compile_test
dump_check_test "sign" "dlsym matched to the signature" 1
dump_check_test "const" "dlsym set state:CONSTANT" 1
dump_check_test "symbol" "function_caller->dlsym->\[(bar,baz)|(baz,bar)\]" 1
cleanup_test

### wrapper2.c
TFILE="wrapper2.c"

compile_test
dump_check_test "sign" "dlsym matched to the signature" 1
dump_check_test "const" "dlsym set state:CONSTANT" 1
dump_check_test "symbol" "function_caller_1->dlsym->\[(bar,baz)|(baz,bar)\]" 1
cleanup_test

## C++ tests
TCOMPILER=$CXX

### const.cpp
TFILE="const.cpp"

compile_test
dump_check_test "sign" "dlsym matched to the signature" 1
dump_check_test "const" "dlsym set state:CONSTANT" 1
dump_check_test "symbol" "main->dlsym->\[Create\]" 1
cleanup_test

### overridden.cpp
TFILE="overridden.cpp"

compile_test
dump_check_test "sign" "dlsym matched to the signature" 1
dump_check_test "const" "dlsym set state:CONSTANT" 1
dump_check_test "symbol" "_ZN12interseption22GetRealFunctionAddressEPKc->dlsym->\[foo\]" 1
cleanup_test

### reinter_cast.cpp
TFILE="reinter_cast.cpp"

compile_test
dump_check_test "sign" "dlsym matched to the signature" 1
dump_check_test "const" "dlsym set state:CONSTANT" 1
dump_check_test "symbol" "main->dlsym->\[bar\]" 1
cleanup_test

# Summary
echo "------------------"
echo "Passed:	$COUNTER_PASSED"
echo "Total:	$COUNTER_TOTAL"
echo "------------------"

# TODO c_str_call.cpp
# TODO strcat.c
#echo $(simple_test wrapper_complex.c "dlsym matched to the signature" 1 "Signature"  )
#echo $(simple_test wrapper_complex.c "dlsym set state:DYNAMIC" 1 "DYNAMIC" )
#echo $(simple_test wrapper_complex.c "function_caller->dlsym->\[bar\]" 1 "Symbol Set" )

exit 0

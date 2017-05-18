#!/bin/bash
GCCDIR="/home/eugene/Workspace/install/gcc-trunk/bin"                  
CXX="$GCCDIR/x86_64-pc-linux-gnu-g++"
CC="$GCCDIR/x86_64-pc-linux-gnu-gcc"
CXXFLAGS+="-O0 -ggdb3 -fno-exceptions -fno-rtti -std=c++11 -fpic -Wall"
PLUGIN=../libplug.so 


function dump_contains() {
  echo $(grep -cPR "$2" $1)
}

function simple_test() {
  local file="$1"
  local str="$2"
  local expected="$3"
  echo "TESTING $file ($4)"
  $CC -O2 -fplugin=$PLUGIN $file -ldl -fdump-ipa-all
  local actual=$(dump_contains $file.312i.dlsym "$str")
  if [ $expected -eq $actual ]
  then
    echo "PASS"
  else
    echo "FAIL: Expected $expected but actual is $actual"
  fi
  rm $file.* *.out
}

function simple_test_cpp() {
  local file="$1"
  local str="$2"
  local expected="$3"
  echo "TESTING $file ($4)"
  $CXX -O2 -fplugin=$PLUGIN $file -ldl -fdump-ipa-all
  local actual=$(dump_contains $file.312i.dlsym "$str")
  if [ $expected -eq $actual ]
  then
    echo "PASS"
  else
    echo "FAIL: Expected $expected but actual is $actual"
  fi
  rm $file.* *.out
}
# echo $(simple_test array.c       "dlsym matched to the signature" 1 "Signature"  )
# echo $(simple_test array.c       "dlsym set is not limited"	   1 "Not limited")
# TODO array_assign.c
echo $(simple_test_cpp const.cpp "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test_cpp const.cpp "dlsym set is not limited"	   0 "Limited")
echo $(simple_test_cpp const.cpp "int main\(int, char\*\*\)->dlsym->\[Create\]" 1 "Symbol Set" )
echo $(simple_test const_array.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test const_array.c "dlsym set is not limited"	   0 "Limited")
echo $(simple_test const_array.c "main->dlsym->\[bar\]"           1 "Symbol Set" )
# TODO const_array2.c
echo $(simple_test conditional.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test conditional.c "dlsym set is not limited"        0 "Limited" )
echo $(simple_test conditional.c "main->dlsym->\[(baz,bar)|(bar,baz)\]" 1 "Symbol Set" )
# TODO const.cpp
# TODO c_str_call.cpp
echo $(simple_test cycle_cgraph.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test cycle_cgraph.c "dlsym set is not limited"        0 "Limited" )
echo $(simple_test cycle_cgraph.c "caller2->dlsym->\[bar\]"        1 "Symbol Set" )
echo $(simple_test cycle_cgraph2.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test cycle_cgraph2.c "dlsym set is not limited"        0 "Limited" )
echo $(simple_test cycle_cgraph2.c "function_caller->dlsym->\[bar\]" 1 "Symbol Set" )
# TODO func_macro
echo $(simple_test global_const.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test global_const.c "dlsym set is not limited"        0 "Limited" )
echo $(simple_test global_const.c "function_caller->dlsym->\[foo\]" 1 "Symbol Set" )
# TODO Skipped global_handler.c
echo $(simple_test local_array.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test local_array.c "dlsym set is not limited"        0 "Limited" )
echo $(simple_test local_array.c "main->dlsym->\[bar\]" 1 "Symbol Set" )
# TODO local.c (extern variable) + local_d.c + local.h
# TODO phi_rec.c
echo $(simple_test proxy.c "dlsym matched to the signature" 2 "Signature"  )
echo $(simple_test proxy.c "dlsym set is not limited"        1 "Not limited" )
echo $(simple_test proxy.c "main->dlsym->\[bar\]" 1 "Symbol Set" )
echo $(simple_test static_proxy.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test static_proxy.c "dlsym set is not limited"        0 "Limited" )
echo $(simple_test static_proxy.c "function_caller.constprop->dlsym->\[bar\]" 1 "Symbol Set" )
echo $(simple_test static_proxy1.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test static_proxy1.c "dlsym set is not limited"        0 "Limited" )
echo $(simple_test static_proxy1.c "function_caller.constprop->dlsym->\[(baz,bar)|(bar,baz)\]" 1 "Symbol Set" )
echo $(simple_test static_proxy2.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test static_proxy2.c "dlsym set is not limited"        0 "Limited" )
echo $(simple_test static_proxy2.c "function_caller.constprop->dlsym->\[baz\]" 1 "Symbol Set" )
# TODO strcat.c
echo $(simple_test switch.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test switch.c "dlsym set is not limited"        0 "Limited" )
echo $(simple_test switch.c "main->dlsym->\[(baz,foo)|(foo,baz)\]" 1 "Symbol Set" )
echo $(simple_test switch2.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test switch2.c "dlsym set is not limited"        0 "Limited" )
echo $(simple_test switch2.c "function_caller.constprop->dlsym->\[(baz,foo)|(foo,baz)\]" 1 "Symbol Set" )
echo $(simple_test undef.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test undef.c "dlsym set is not limited"        1 "Not limited" )
echo $(simple_test var.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test var.c "dlsym set is not limited"        0 "Limited" )
echo $(simple_test var.c "main->dlsym->\[bar\]" 1 "Symbol Set" )
echo $(simple_test wrapper.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test wrapper.c "dlsym set is not limited"        0 "Limited" )
echo $(simple_test wrapper.c "function_caller->dlsym->\[(bar,baz)|(baz,bar)\]" 1 "Symbol Set" )
echo $(simple_test wrapper2.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test wrapper2.c "dlsym set is not limited"        0 "Limited" )
echo $(simple_test wrapper2.c "function_caller_1->dlsym->\[(bar,baz)|(baz,bar)\]" 1 "Symbol Set" )
#echo $(simple_test wrapper_complex.c "dlsym matched to the signature" 1 "Signature"  )
#echo $(simple_test wrapper_complex.c "dlsym set is not limited"        0 "Limited" )
#echo $(simple_test wrapper_complex.c "function_caller->dlsym->\[bar\]" 1 "Symbol Set" )

exit 0

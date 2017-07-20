#!/bin/bash
PLUGIN_ARG="-fplugin-arg-libplug-sign-dlsym=1"

function dump_contains() {
  echo $(grep -cPR "$2" $1)
}

function simple_test() {
  local file="$1"
  local str="$2"
  local expected="$3"
  echo "TESTING $file ($4)"
  $CC -O2 -fplugin=$PLUGIN $PLUGIN_ARG $file -ldl -fdump-ipa-all
  local actual=$(dump_contains $file.*i.dlsym "$str")
  if [ $expected -eq $actual ]
  then
    echo "PASS"
  else
    echo "FAIL: Expected $expected but actual is $actual"
  fi
  rm $file.* *.out
  rm *.dlsym
}

function simple_test_cpp() {
  local file="$1"
  local str="$2"
  local expected="$3"
  echo "TESTING $file ($4)"
  $CXX -O2 -fplugin=$PLUGIN $PLUGIN_ARG $file -ldl -fdump-ipa-all
  local actual=$(dump_contains $file.*i.dlsym "$str")
  if [ $expected -eq $actual ]
  then
    echo "PASS"
  else
    echo "FAIL: Expected $expected but actual is $actual"
  fi
  rm $file.* *.out
  rm *.dlsym
}
# TODO handle constant index
# echo $(simple_test array.c       "dlsym matched to the signature" 1 "Signature"  )
# echo $(simple_test array.c       "dlsym set state:DYNAMIC"	   1 "DYNAMIC")
echo $(simple_test array_assign.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test array_assign.c "dlsym set state:CONSTANT" 1 "CONSTANT" )
echo $(simple_test array_assign.c "main->dlsym->\[init\]" 1 "Symbol Set" )
echo $(simple_test array_assign_part.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test array_assign_part.c "dlsym set state:PARTIALLY_CONSTANT" 1 "PARTIALLY_CONSTANT" )
echo $(simple_test array_assign_part.c "main->dlsym->\[_init\]" 1 "Symbol Set" )
echo $(simple_test asm_name.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test asm_name.c "dlsym set state:CONSTANT" 1 "CONSTANT" )
echo $(simple_test asm_name.c "\*asm_func_caller->dlsym->\[bar\]" 0 "Wrong Symbol Set" )
echo $(simple_test asm_name.c "asm_func_caller->dlsym->\[bar\]" 1 "Symbol Set" )
echo $(simple_test_cpp const.cpp "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test_cpp const.cpp "dlsym set state:CONSTANT" 1 "CONSTANT")
echo $(simple_test_cpp const.cpp "main->dlsym->\[Create\]" 1 "Symbol Set" )
echo $(simple_test_cpp overridden.cpp "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test_cpp overridden.cpp "dlsym set state:CONSTANT" 1 "CONSTANT")
echo $(simple_test_cpp overridden.cpp "_ZN12interseption22GetRealFunctionAddressEPKc->dlsym->\[foo\]" 1 "Symbol Set" )
echo $(simple_test const_array.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test const_array.c "dlsym set state:CONSTANT" 1 "CONSTANT")
echo $(simple_test const_array.c "main->dlsym->\[bar\]" 1 "Symbol Set" )
echo $(simple_test const_array2.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test const_array2.c "dlsym set state:CONSTANT" 1 "CONSTANT")
echo $(simple_test const_array2.c "main->dlsym->\[(foo,zoo)|(zoo,foo)\]" 1 "Symbol Set" )
echo $(simple_test const_array3.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test const_array3.c "dlsym set state:CONSTANT" 1 "CONSTANT")
echo $(simple_test const_array3.c "main->dlsym->\[(foo,bar)|(bar,foo)\]" 1 "Symbol Set" )
echo $(simple_test const_array4.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test const_array4.c "dlsym set state:CONSTANT" 1 "CONSTANT")
echo $(simple_test const_array4.c "main->dlsym->\[(foo,bar)|(bar,foo)\]" 1 "Symbol Set" )
echo $(simple_test const_array5.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test const_array5.c "dlsym set state:PARTIALLY_CONSTANT" 1 "PARTIALLY_CONSTANT")
echo $(simple_test const_array5.c "main->dlsym->\[(foo,bar)|(bar,foo)\]" 1 "Symbol Set" )
echo $(simple_test const_array6.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test const_array6.c "dlsym set state:CONSTANT" 1 "CONSTANT")
echo $(simple_test const_array6.c "main->dlsym->\[(bar1,bar)|(bar,bar1)\]" 1 "Symbol Set" )
echo $(simple_test const_array7.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test const_array7.c "dlsym set state:DYNAMIC" 1 "DYNAMIC")
echo $(simple_test const_struct.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test const_struct.c "dlsym set state:CONSTANT" 1 "CONSTANT")
echo $(simple_test const_struct.c "main->dlsym->\[(baz,bar)|(bar,baz)\]" 1 "Symbol Set" )
echo $(simple_test conditional.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test conditional.c "dlsym set state:CONSTANT" 1 "CONSTANT" )
echo $(simple_test conditional.c "main->dlsym->\[(baz,bar)|(bar,baz)\]" 1 "Symbol Set" )
# TODO c_str_call.cpp
echo $(simple_test cycle_cgraph.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test cycle_cgraph.c "dlsym set state:CONSTANT" 1 "CONSTANT" )
echo $(simple_test cycle_cgraph.c "caller2->dlsym->\[bar\]" 1 "Symbol Set" )
echo $(simple_test cycle_cgraph2.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test cycle_cgraph2.c "dlsym set state:CONSTANT" 1 "CONSTANT" )
echo $(simple_test cycle_cgraph2.c "function_caller->dlsym->\[bar\]" 1 "Symbol Set" )
echo $(simple_test func_call.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test func_call.c "dlsym set state:DYNAMIC" 1 "DYNAMIC")
echo $(simple_test func_macro.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test func_macro.c "dlsym set state:CONSTANT" 1 "CONSTANT")
echo $(simple_test func_macro.c "foo->dlsym->\[foo\]" 1 "Symbol Set" )
echo $(simple_test global_var.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test global_var.c "dlsym set state:CONSTANT" 1 "CONSTANT")
echo $(simple_test global_var.c "main->dlsym->\[foo\]" 1 "Symbol Set" )
echo $(simple_test global_var2.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test global_var2.c "dlsym set state:PARTIALLY_CONSTANT" 1 "PARTIALLY_CONSTANT")
echo $(simple_test global_var2.c "main->dlsym->\[(foo,goo)|(goo,foo)\]" 1 "Symbol Set" )
echo $(simple_test local_array.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test local_array.c "dlsym set state:CONSTANT" 1 "CONSTANT" )
echo $(simple_test local_array.c "main->dlsym->\[bar\]" 1 "Symbol Set" )
echo $(simple_test macro_passing.c "dlsym matched to the signature" 2 "Signature"  )
echo $(simple_test macro_passing.c "dlsym set state:CONSTANT" 2 "CONSTANT" )
echo $(simple_test macro_passing.c "main->dlsym->\[foo\]" 1 "Symbol Set" )
echo $(simple_test macro_passing.c "main->dlsym->\[bar\]" 1 "Symbol Set" )
echo $(simple_test_cpp reinter_cast.cpp "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test_cpp reinter_cast.cpp "dlsym set state:CONSTANT" 1 "CONSTANT" )
echo $(simple_test_cpp reinter_cast.cpp "main->dlsym->\[bar\]" 1 "Symbol Set" )
echo $(simple_test static_proxy.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test static_proxy.c "dlsym set state:CONSTANT" 1 "CONSTANT" )
echo $(simple_test static_proxy.c "function_caller.constprop.0->dlsym->\[bar\]" 1 "Symbol Set" )
echo $(simple_test static_proxy1.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test static_proxy1.c "dlsym set state:CONSTANT" 1 "CONSTANT" )
echo $(simple_test static_proxy1.c "function_caller.constprop.0->dlsym->\[(baz,bar)|(bar,baz)\]" 1 "Symbol Set" )
echo $(simple_test static_proxy2.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test static_proxy2.c "dlsym set state:CONSTANT" 1 "CONSTANT" )
echo $(simple_test static_proxy2.c "function_caller.constprop.1->dlsym->\[baz\]" 1 "Symbol Set" )
# TODO strcat.c
echo $(simple_test switch.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test switch.c "dlsym set state:CONSTANT" 1 "CONSTANT" )
echo $(simple_test switch.c "main->dlsym->\[(baz,foo)|(foo,baz)\]" 1 "Symbol Set" )
echo $(simple_test switch2.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test switch2.c "dlsym set state:CONSTANT" 1 "CONSTANT" )
echo $(simple_test switch2.c "function_caller.constprop->dlsym->\[(baz,foo)|(foo,baz)\]" 1 "Symbol Set" )
echo $(simple_test undef.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test undef.c "dlsym set state:UNDEFINED" 1 "UNDEFINED" )
echo $(simple_test var.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test var.c "dlsym set state:CONSTANT" 1 "CONSTANT" )
echo $(simple_test var.c "main->dlsym->\[bar\]" 1 "Symbol Set" )
echo $(simple_test wrapper.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test wrapper.c "dlsym set state:CONSTANT" 1 "CONSTANT" )
echo $(simple_test wrapper.c "function_caller->dlsym->\[(bar,baz)|(baz,bar)\]" 1 "Symbol Set" )
echo $(simple_test wrapper2.c "dlsym matched to the signature" 1 "Signature"  )
echo $(simple_test wrapper2.c "dlsym set state:CONSTANT" 1 "CONSTANT" )
echo $(simple_test wrapper2.c "function_caller_1->dlsym->\[(bar,baz)|(baz,bar)\]" 1 "Symbol Set" )
#echo $(simple_test wrapper_complex.c "dlsym matched to the signature" 1 "Signature"  )
#echo $(simple_test wrapper_complex.c "dlsym set state:DYNAMIC" 1 "DYNAMIC" )
#echo $(simple_test wrapper_complex.c "function_caller->dlsym->\[bar\]" 1 "Symbol Set" )

exit 0

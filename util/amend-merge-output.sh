#!/bin/bash -

linecnt()
{
  wc -l $1 | awk '{ print $1 }'
}

merged=$1

felim=$(mktemp --tmpdir elim-XXX)
floc=$(mktemp --tmpdir loc-XXX)
fhid=$(mktemp --tmpdir hid-XXX)

awk -F: -v elim="$felim" -v loc="$floc" -v hid="$fhid" -- '
BEGIN {
  ELIM = 0; LOC = 1; HID = 2
  state = ELIM
  SUBSEP = ":"
}

/[0-9]+ [0-9]+ [0-9]+ [0-9a-f]{32,32}/ {
  state = ELIM
  next
}

/^$/ {
  state++
  next
}

#       1 (obj)          2 (md5)           3 (srcidcnt)  4 (sym)
# libQt5UiTools.a:2aaa1fa352bffe51e178b14dbf40e55f:6:_ZTIN13QFormI

{
  arr[$2, $5, state]++
  all[$2, $5] = $3
  obj[$2, $5] = $1  # mkpriv gcc plugin does not care about this field
}

END {
  for (i in all)
    if (all[i] == arr[i SUBSEP ELIM] + arr[i SUBSEP LOC] + arr[i SUBSEP HID])
      if (arr[i SUBSEP HID]  > 0)
        print obj[i]":"i >> hid
      else if (arr[i SUBSEP LOC] > 0)
        print obj[i]":"i >> loc
      else
        print obj[i]":"i >> elim
} ' $merged

result=$merged-$$

echo "$(linecnt $felim) $(linecnt $floc) $(linecnt $fhid)"> $result
cat $felim >> $result
echo >> $result
cat $floc >> $result
echo >> $result
cat $fhid >> $result

rm $felim $floc $fhid

echo $result

#!/bin/bash -

linecnt()
{
  wc -l $1 | awk '{ print $1 }'
}

merged=$1

fstatics=$(mktemp --tmpdir statics-XXX)
flibprivs=$(mktemp --tmpdir libprivs-XXX)

awk -F: -v statics="$fstatics" -v libprivs="$flibprivs" -- '
BEGIN {
  state = 0    # 0 -- in statics, 1 -- in libprivates
  SUBSEP = ":"
}

/[0-9]+ [0-9]+ [0-9a-f]{32,32}/ {
  state = 0
  next
}

/^$/ {
  state = 1
  next
}

#       1 (obj)          2 (md5)           3 (srcidcnt)  4 (sym)
# libQt5UiTools.a:2aaa1fa352bffe51e178b14dbf40e55f:6:_ZTIN13QFormI

{
  arr[$2, $4, state]++
  all[$2, $4] = $3
  obj[$2, $4] = $1  # mkpriv gcc plugin does not care about this field
}

END {
  for (i in all)
    if (all[i] == arr[i SUBSEP 0] + arr[i SUBSEP 1])
      if (arr[i SUBSEP 1] > 0)
        print obj[i]":"i >> libprivs
      else
        print obj[i]":"i >> statics
} ' $merged

result=$merged-$$

echo "$(linecnt $fstatics) $(linecnt $flibprivs)" > $result
cat $fstatics >> $result
echo >> $result
cat $flibprivs >> $result

rm $fstatics $flibprivs

echo $result

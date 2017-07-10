#!/bin/bash -

# Take the hashes from .o files and paste into the .vis files (.vis files are
# created from .vis.def files via substituting proper hashes for the
# placeholders).

# This script takes a list of .o files with the .o suffix stripped.

vnames_seen=""

while [ $# -gt 0 ]
do
  bname="$1"
  oname="$bname.o"
  sname=$(echo "$bname" | sed -e 's/[0-9]\+$//')
  vname="$sname.vis"

  hash=$(objdump -h "$oname" | sed -n "s/.*[.]comment[.]privplugid[.]\([^ ]\+\).*/\1/p")
  if [ ! -n "$hash" ]; then
    echo "Error: $oname doesn't have a hash section"
    exit 1
  fi

  case " $vnames_seen " in
    *" $vname "*)
      # Second time around we should sed the result of the first step and so on.
      sed -i -e "s/:$bname:/:$hash:/" "$vname"
      ;;
    *)
      sed -e "s/:$bname:/:$hash:/" "$vname.def" > "$vname"
      vnames_seen="$vnames_seen $vname "
      ;;
  esac

  shift
done

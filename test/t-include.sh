# -IDIR, -LDIR options should not influence srcid and, consequently, linkid.

tmpdir=$(mktemp -d)
trap 'rmdir $tmpdir' EXIT
echo 'int main() { return 42; }' | $CC -xc - -I$tmpdir -o /dev/null || exit 1
echo 'int main() { return 42; }' | $CC -xc - -L$tmpdir -o /dev/null || exit 1

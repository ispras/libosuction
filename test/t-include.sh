# -IDIR, -LDIR options should not influence srcid and, consequently, linkid.

tmpdir=$(mktemp -d)
trap 'rmdir $tmpdir' EXIT
<<<'int main() { return 42; }' $CC -xc - -I$tmpdir -o /dev/null || exit 1
<<<'int main() { return 42; }' $CC -xc - -L$tmpdir -o /dev/null || exit 1

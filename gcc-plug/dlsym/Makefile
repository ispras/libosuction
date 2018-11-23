# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2018 ISP RAS (http://ispras.ru/en)

PLUGIN = libplug.so

.PHONY: clean test

test: $(PLUGIN)
	$(CC) -O2 -shared -fPIC tests/testlib.c -o tests/testlib.so; \
	cd "tests" || exit 1; \
	CC=$(CC) CXX=$(CXX) PLUGIN="../$(PLUGIN)" ./symbols-test.sh || exit 1; \
	CC=$(CC) CXX=$(CXX) PLUGIN="../$(PLUGIN)" ./jfunc-test.sh || exit 1;

clean:
	-rm -f $(PLUGIN) *.o a.out
	-rm -f tests/*.out tests/*.so

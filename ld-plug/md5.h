// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018 ISP RAS (http://ispras.ru/en)

// Trivial wrappers/helpers for md5 computation routines defined in md5-impl.h

#ifndef MD5_H
#define MD5_H

#include "md5-impl.h"

static void md5_buffer(const void *buf, size_t size, unsigned char *md5sum)
{
	struct md5 ctx;
	md5_init(&ctx);
	md5_update(&ctx, buf, size);
	md5_sum(&ctx, md5sum);
}

static void printmd5(char *restrict p, const unsigned char md5[restrict])
{
  for (int i = 0; i < 16; i++)
    {
      *p++ = "0123456789abcdef"[md5[i] >> 4];
      *p++ = "0123456789abcdef"[md5[i] & 15];
    }
}

#endif

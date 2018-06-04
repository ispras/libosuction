// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018 ISP RAS (http://ispras.ru/en)

#ifndef WRAPPER_COMMON_H
#define WRAPPER_COMMON_H

#include "confdef.h"

#define MERGED_PRIVDATA AUXDIR "merged.vis"
#define GCC_SOCKFD "GCC_SOCKFD"

extern void die(const char *fmt, ...);
extern int daemon_connect(int argc, char *argv[], char tool);

#endif  /* wrapper-common.h */

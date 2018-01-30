#ifndef WRAPPER_COMMON_H
#define WRAPPER_COMMON_H

#include "confdef.h"

#define MERGED_PRIVDATA AUXDIR "merged.vis"

extern void die(const char *fmt, ...);
extern int daemon_connect(int argc, char *argv[], char tool);

#endif  /* wrapper-common.h */

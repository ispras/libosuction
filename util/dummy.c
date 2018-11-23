// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018 ISP RAS (http://ispras.ru/en)

__attribute__((weak))
void dum_memcpy() asm("memcpy");

__attribute__((weak))
void dum_vfork() asm("vfork");

__attribute__((used))
static
void *dum_ptr[] = { dum_memcpy, dum_vfork };

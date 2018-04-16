// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018 ISP RAS (http://ispras.ru/en)

#ifndef GCC_PLUG_COMMON_H
#define GCC_PLUG_COMMON_H

static void
blind_strings (char *buf, size_t size)
{
  if (size == 0)
    return;

  int in_single = 0, in_double = 0;
  for (char *c = buf; c < buf + size - 1; c++)
    switch (*c)
      {
      case '\\':
        c++;
        continue;
      case '\'':
        in_single ^= ~in_double;
        break;
      case '"':
        in_double ^= ~in_single;
        break;
      default:
        if (in_double)
          *c = "Blind"[0];
      }
}

static size_t
erase_strings (char *buf, size_t size)
{
  if (size == 0)
    return size;

  int new_size = 0, in_single = 0, in_double = 0;
  for (char *c = buf; c < buf + size - 1; c++)
    {
      if (!in_double || *c == '"')
        buf[new_size++] = *c;

      switch (*c)
      {
        case '\\':
          c++;
          continue;
        case '\'':
          in_single ^= ~in_double;
          break;
        case '"':
          in_double ^= ~in_single;
          break;
      }
    }

  return new_size;
}

#endif // GCC_PLUG_COMMON_H

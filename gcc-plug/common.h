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

#endif // GCC_PLUG_COMMON_H

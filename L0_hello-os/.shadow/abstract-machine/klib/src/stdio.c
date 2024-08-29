#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <stdarg.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

static int vsnprintf_helper(char *out, size_t n, const char *fmt, va_list ap) {
  int written = 0;
  const char *p = fmt;
  
  while (*p && written < n - 1) {
    if (*p == '%') {
      p++;
      switch (*p) {
        case 'c': {
          int c = va_arg(ap, int);
          out[written++] = (char)c;
          break;
        }
        case 's': {
          const char *s = va_arg(ap, const char *);
          while (*s && written < n - 1) {
            out[written++] = *s++;
          }
          break;
        }
        case 'd': {
          int num = va_arg(ap, int);
          written += snprintf(out + written, n - written, "%d", num);
          break;
        }
        case 'x': {
          int num = va_arg(ap, int);
          written += snprintf(out + written, n - written, "%x", num);
          break;
        }
        default:
          out[written++] = *p;
          break;
      }
    } else {
      out[written++] = *p;
    }
    p++;
  }

  if (written < n) {
    out[written] = '\0';
  }

  return written;
}

int vsnprintf(char *out, size_t n, const char *fmt, va_list ap) {
  return vsnprintf_helper(out, n, fmt, ap);
}

int snprintf(char *out, size_t n, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int ret = vsnprintf(out, n, fmt, ap);
  va_end(ap);
  return ret;
}

int vsprintf(char *out, const char *fmt, va_list ap) {
  return vsnprintf(out, (size_t)-1, fmt, ap);
}

int sprintf(char *out, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int ret = vsnprintf(out, (size_t)-1, fmt, ap);
  va_end(ap);
  return ret;
}

int printf(const char *fmt, ...) {
  char buffer[1024];
  va_list ap;
  va_start(ap, fmt);
  int ret = vsnprintf(buffer, sizeof(buffer), fmt, ap);
  va_end(ap);
  putstr(buffer);
  return ret;
}

#endif


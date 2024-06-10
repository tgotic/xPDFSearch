//========================================================================
//
// GString.cc
//
// Simple variable-length string type.
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>
#include "gmem.h"
#include "gmempp.h"
#include "GString.h"

//------------------------------------------------------------------------

union GStringFormatArg {
  int i;
  Guint ui;
  long l;
  Gulong ul;
#ifdef LLONG_MAX
  long long ll;
#endif
#ifdef ULLONG_MAX
  unsigned long long ull;
#endif
  double f;
  char c;
  char *s;
  GString *gs;
};

enum GStringFormatType {
  fmtIntDecimal,
  fmtIntHex,
  fmtIntOctal,
  fmtIntBinary,
  fmtUIntDecimal,
  fmtUIntHex,
  fmtUIntOctal,
  fmtUIntBinary,
  fmtLongDecimal,
  fmtLongHex,
  fmtLongOctal,
  fmtLongBinary,
  fmtULongDecimal,
  fmtULongHex,
  fmtULongOctal,
  fmtULongBinary,
#ifdef LLONG_MAX
  fmtLongLongDecimal,
  fmtLongLongHex,
  fmtLongLongOctal,
  fmtLongLongBinary,
#endif
#ifdef ULLONG_MAX
  fmtULongLongDecimal,
  fmtULongLongHex,
  fmtULongLongOctal,
  fmtULongLongBinary,
#endif
  fmtDouble,
  fmtDoubleTrim,
  fmtChar,
  fmtString,
  fmtGString,
  fmtSpace
};

static const char *formatStrings[] = {
  "d", "x", "o", "b", "ud", "ux", "uo", "ub",
  "ld", "lx", "lo", "lb", "uld", "ulx", "ulo", "ulb",
#ifdef LLONG_MAX
  "lld", "llx", "llo", "llb",
#endif
#ifdef ULLONG_MAX
  "ulld", "ullx", "ullo", "ullb",
#endif
  "f", "g",
  "c",
  "s",
  "t",
  "w",
  NULL
};

//------------------------------------------------------------------------

static inline  int size( int len) {
   int delta;
  for (delta = 8; delta < len && delta < 0x100000; delta <<= 1) ;
  if (len > INT_MAX - delta) {
    gMemError("Integer overflow in GString::size()");
  }
  // this is ((len + 1) + (delta - 1)) & ~(delta - 1)
  return (len + delta) & ~(delta - 1);
}

inline void GString::resize(int length1) {
  if (length1 < 0) {
    gMemError("GString::resize() with negative length");
  }
  const  int l1 = size(length1);
  if (!s) {
    s = new char[l1];
  } else if (l1 != size(length)) {
    char *s1 = new char[l1];
    if (s1) {
      if (length1 < length) {
	memcpy(s1, s, length1);
	s1[length1] = '\0'; 
      } else {
	memcpy(s1, s, length + 1);
      }
    }
    delete[] s;
    s = s1;
  }
}

GString::GString(const char *sA)
{
  if (sA) {
    int  n = (int)strlen(sA);
    resize(n);
    if (s) {
        length = n;
        memcpy(s, sA, n + 1);
    }
  }
}

GString::GString(const char *sA, int lengthA)
{
  if (sA && lengthA) {
    resize(lengthA);
    if (s) {
      length = lengthA;
      memcpy(s, sA, length);
      s[length] = '\0';
    }
  }
}

GString::GString(GString *str, int idx, int lengthA)
{
  if (str && str->getCString() && (idx >= 0) && (idx < str->getLength())) {
    const int right{ str->getLength() - idx };
    if (right > lengthA) {
      lengthA = right;
    }
    if (lengthA) {
      resize(lengthA);
      if (s) {
	length = lengthA;
	memcpy(s, str->getCString() + idx, length);
	s[length] = '\0';
      }
    }
  }
}

GString::GString(GString *str)
{
  if (str && str->getCString()) {
    const int len = str->getLength();
    resize(len);
    if (s) {
      length = len;
      memcpy(s, str->getCString(), length + 1);
    }
  }
}

GString::GString(GString *str1, GString *str2)
{
  int n1{ 0 };
  int n2{ 0 };
  if (str1 && str1->getCString()) {
    n1 = str1->getLength();
  }
  if (str2 && str2->getCString()) {
    n2 = str2->getLength();
  }
  if (n1 > INT_MAX - n2) {
    gMemError("Integer overflow in GString::GString()");
  }
  auto n{ n1 + n2 };
  if (n) {
    resize(n);
    if (s) {
      length = n;
      if (n1) {
        memcpy(s, str1->getCString(), n1);
      }
      if (n2) {
        memcpy(s + n1, str2->getCString(), n2);
      }
      s[length] = '\0';
    }
  }
}

GString *GString::fromInt(int x) {
  char buf[24]; // enough space for 64-bit ints plus a little extra
  const char *p;
  int len;

  formatInt(x, buf, sizeof(buf), gFalse, 0, 10, &p, &len);
  return new GString(p, len);
}

GString *GString::format(const char *fmt, ...) {
  GString *s = new GString();
  if (s) {
    va_list argList;
    va_start(argList, fmt);
    s->appendfv(fmt, argList);
    va_end(argList);
  }
  return s;
}

GString *GString::formatv(const char *fmt, va_list argList) {
  GString *s = new GString();
  if (s)
    s->appendfv(fmt, argList);
  return s;
}

GString::~GString() {
  delete[] s;
}

GString *GString::clear() {
  length = 0;
  delete[] s;
  s = nullptr;
  return this;
}

GString *GString::append(char c) {
  return append(&c, 1);
}

GString *GString::append(GString *str) {
  if (str) {
    append(str->getCString(), str->getLength());
  }
  return this;
}

GString *GString::append(const char *str) {
  if (str) {
    append(str, (int)strlen(str));
  }
  return this;
}

GString *GString::append(const char *str, int lengthA) {
  if (lengthA < 0 || length > INT_MAX - lengthA) {
    gMemError("Integer overflow in GString::append()");
  }
  if (str && lengthA) {
    resize(length + lengthA);
    if (s) {
      memcpy(s + length, str, lengthA);
      length += lengthA;
      s[length] = '\0';
    }
  }
  return this;
}

GString *GString::appendf(const char *fmt, ...) {
  if (fmt) {
    va_list argList;

    va_start(argList, fmt);
    appendfv(fmt, argList);
    va_end(argList);
  }
  return this;
}

GString *GString::appendfv(const char *fmt, va_list argList) {
  int idx, width, prec;
  int len = 0;
  int argsLen = 0;
  int argsSize = 8;
  GStringFormatType ft;
  const char *str = NULL;
  const char *p0 = fmt;
  char buf[65];

  GStringFormatArg arg;
  GStringFormatArg *args = (GStringFormatArg *)gmallocn(argsSize, sizeof(GStringFormatArg));

  GBool reverseAlign, zeroFill;
  while (*p0) {
    if (*p0 == '{') {
      ++p0;
      if (*p0 == '{') {
	++p0;
	append('{');
      } else {

	// parse the format string
	if (!(*p0 >= '0' && *p0 <= '9')) {
	  break;
	}
	idx = *p0 - '0';
	for (++p0; *p0 >= '0' && *p0 <= '9'; ++p0) {
	  idx = 10 * idx + (*p0 - '0');
	}
	if (*p0 != ':') {
	  break;
	}
	++p0;
	if (*p0 == '-') {
	  reverseAlign = gTrue;
	  ++p0;
	} else {
	  reverseAlign = gFalse;
	}
	width = 0;
	zeroFill = *p0 == '0';
	for (; *p0 >= '0' && *p0 <= '9'; ++p0) {
	  width = 10 * width + (*p0 - '0');
	}
	if (width < 0) {
	  width = 0;
	}
	if (*p0 == '.') {
	  ++p0;
	  prec = 0;
	  for (; *p0 >= '0' && *p0 <= '9'; ++p0) {
	    prec = 10 * prec + (*p0 - '0');
	  }
	} else {
	  prec = 0;
	}
	for (ft = (GStringFormatType)0;
	     formatStrings[ft];
	     ft = (GStringFormatType)(ft + 1)) {
	  if (!strncmp(p0, formatStrings[ft], strlen(formatStrings[ft]))) {
	    break;
	  }
	}
	if (!formatStrings[ft]) {
	  break;
	}
	p0 += strlen(formatStrings[ft]);
	if (*p0 != '}') {
	  break;
	}
	++p0;

	// fetch the argument
	if (idx > argsLen) {
	  break;
	}
	if (idx == argsLen) {
	  if (argsLen == argsSize) {
	    argsSize *= 2;
	    args = (GStringFormatArg *)greallocn(args, argsSize,
						 sizeof(GStringFormatArg));
	  }
	  switch (ft) {
	  case fmtIntDecimal:
	  case fmtIntHex:
	  case fmtIntOctal:
	  case fmtIntBinary:
	  case fmtSpace:
	    args[argsLen].i = va_arg(argList, int);
	    break;
	  case fmtUIntDecimal:
	  case fmtUIntHex:
	  case fmtUIntOctal:
	  case fmtUIntBinary:
	    args[argsLen].ui = va_arg(argList, Guint);
	    break;
	  case fmtLongDecimal:
	  case fmtLongHex:
	  case fmtLongOctal:
	  case fmtLongBinary:
	    args[argsLen].l = va_arg(argList, long);
	    break;
	  case fmtULongDecimal:
	  case fmtULongHex:
	  case fmtULongOctal:
	  case fmtULongBinary:
	    args[argsLen].ul = va_arg(argList, Gulong);
	    break;
#ifdef LLONG_MAX
	  case fmtLongLongDecimal:
	  case fmtLongLongHex:
	  case fmtLongLongOctal:
	  case fmtLongLongBinary:
	    args[argsLen].ll = va_arg(argList, long long);
	    break;
#endif
#ifdef ULLONG_MAX
	  case fmtULongLongDecimal:
	  case fmtULongLongHex:
	  case fmtULongLongOctal:
	  case fmtULongLongBinary:
	    args[argsLen].ull = va_arg(argList, unsigned long long);
	    break;
#endif
	  case fmtDouble:
	  case fmtDoubleTrim:
	    args[argsLen].f = va_arg(argList, double);
	    break;
	  case fmtChar:
	    args[argsLen].c = (char)va_arg(argList, int);
	    break;
	  case fmtString:
	    args[argsLen].s = va_arg(argList, char *);
	    break;
	  case fmtGString:
	    args[argsLen].gs = va_arg(argList, GString *);
	    break;
	  }
	  ++argsLen;
	}

	// format the argument
	arg = args[idx];
	str = NULL;
	len = 0;
	switch (ft) {
	case fmtIntDecimal:
	  formatInt(arg.i, buf, sizeof(buf), zeroFill, width, 10, &str, &len);
	  break;
	case fmtIntHex:
	  formatInt(arg.i, buf, sizeof(buf), zeroFill, width, 16, &str, &len);
	  break;
	case fmtIntOctal:
	  formatInt(arg.i, buf, sizeof(buf), zeroFill, width, 8, &str, &len);
	  break;
	case fmtIntBinary:
	  formatInt(arg.i, buf, sizeof(buf), zeroFill, width, 2, &str, &len);
	  break;
	case fmtUIntDecimal:
	  formatUInt(arg.ui, buf, sizeof(buf), zeroFill, width, 10,
		     &str, &len);
	  break;
	case fmtUIntHex:
	  formatUInt(arg.ui, buf, sizeof(buf), zeroFill, width, 16,
		     &str, &len);
	  break;
	case fmtUIntOctal:
	  formatUInt(arg.ui, buf, sizeof(buf), zeroFill, width, 8, &str, &len);
	  break;
	case fmtUIntBinary:
	  formatUInt(arg.ui, buf, sizeof(buf), zeroFill, width, 2, &str, &len);
	  break;
	case fmtLongDecimal:
	  formatInt(arg.l, buf, sizeof(buf), zeroFill, width, 10, &str, &len);
	  break;
	case fmtLongHex:
	  formatInt(arg.l, buf, sizeof(buf), zeroFill, width, 16, &str, &len);
	  break;
	case fmtLongOctal:
	  formatInt(arg.l, buf, sizeof(buf), zeroFill, width, 8, &str, &len);
	  break;
	case fmtLongBinary:
	  formatInt(arg.l, buf, sizeof(buf), zeroFill, width, 2, &str, &len);
	  break;
	case fmtULongDecimal:
	  formatUInt(arg.ul, buf, sizeof(buf), zeroFill, width, 10,
		     &str, &len);
	  break;
	case fmtULongHex:
	  formatUInt(arg.ul, buf, sizeof(buf), zeroFill, width, 16,
		     &str, &len);
	  break;
	case fmtULongOctal:
	  formatUInt(arg.ul, buf, sizeof(buf), zeroFill, width, 8, &str, &len);
	  break;
	case fmtULongBinary:
	  formatUInt(arg.ul, buf, sizeof(buf), zeroFill, width, 2, &str, &len);
	  break;
#ifdef LLONG_MAX
	case fmtLongLongDecimal:
	  formatInt(arg.ll, buf, sizeof(buf), zeroFill, width, 10, &str, &len);
	  break;
	case fmtLongLongHex:
	  formatInt(arg.ll, buf, sizeof(buf), zeroFill, width, 16, &str, &len);
	  break;
	case fmtLongLongOctal:
	  formatInt(arg.ll, buf, sizeof(buf), zeroFill, width, 8, &str, &len);
	  break;
	case fmtLongLongBinary:
	  formatInt(arg.ll, buf, sizeof(buf), zeroFill, width, 2, &str, &len);
	  break;
#endif
#ifdef ULLONG_MAX
	case fmtULongLongDecimal:
	  formatUInt(arg.ull, buf, sizeof(buf), zeroFill, width, 10,
		     &str, &len);
	  break;
	case fmtULongLongHex:
	  formatUInt(arg.ull, buf, sizeof(buf), zeroFill, width, 16,
		     &str, &len);
	  break;
	case fmtULongLongOctal:
	  formatUInt(arg.ull, buf, sizeof(buf), zeroFill, width, 8,
		     &str, &len);
	  break;
	case fmtULongLongBinary:
	  formatUInt(arg.ull, buf, sizeof(buf), zeroFill, width, 2,
		     &str, &len);
	  break;
#endif
	case fmtDouble:
	  formatDouble(arg.f, buf, sizeof(buf), prec, gFalse, &str, &len);
	  break;
	case fmtDoubleTrim:
	  formatDouble(arg.f, buf, sizeof(buf), prec, gTrue, &str, &len);
	  break;
	case fmtChar:
	  buf[0] = arg.c;
	  str = buf;
	  len = 1;
	  reverseAlign = !reverseAlign;
	  break;
	case fmtString:
	  if (arg.s) {
	    str = arg.s;
	    len = (int)strlen(str);
	  } else {
	    str = "(null)";
	    len = 6;
	  }
	  reverseAlign = !reverseAlign;
	  break;
	case fmtGString:
	  if (arg.gs) {
	    str = arg.gs->getCString();
	    len = arg.gs->getLength();
	  } else {
	    str = "(null)";
	    len = 6;
	  }
	  reverseAlign = !reverseAlign;
	  break;
	case fmtSpace:
	  str = buf;
	  len = 0;
	  width = arg.i;
	  break;
	}

	// append the formatted arg, handling width and alignment
	if (!reverseAlign && len < width) {
	  for (int i = len; i < width; ++i) {
	    append(' ');
	  }
	}
	if (str)
	  append(str, len);
	if (reverseAlign && len < width) {
	  for (int i = len; i < width; ++i) {
	    append(' ');
	  }
	}
      }

    } else if (*p0 == '}') {
      ++p0;
      if (*p0 == '}') {
	++p0;
      }
      append('}');
      
    } else {
      const char* p1 = p0 + 1;
      for (; *p1 && *p1 != '{' && *p1 != '}'; ++p1) ;
      append(p0, (int)(p1 - p0));
      p0 = p1;
    }
  }

  gfree(args);
  return this;
}

#ifdef LLONG_MAX
void GString::formatInt(long long x, char *buf, int bufSize,
			GBool zeroFill, int width, int base,
			const char **p, int*len) {
#else
void GString::formatInt(long x, char *buf, int bufSize,
			GBool zeroFill, int width, int base,
			const char **p, int*len) {
#endif
  static const char* vals = "0123456789abcdef";
  GBool neg;
  int start, i, j;

  i = bufSize;
  neg = x < 0;
  if (neg) {
    x = -x;
  }
  start = neg ? 1 : 0;
  if (x == 0) {
    buf[--i] = '0';
  } else if (base) {
    while (i > start && x) {
      buf[--i] = vals[x % base];
      x /= base;
    }
  }
  if (zeroFill) {
    for (j = bufSize - i; i > start && j < width - start; ++j) {
      buf[--i] = '0';
    }
  }
  if (neg) {
    buf[--i] = '-';
  }
  *p = buf + i;
  *len = bufSize - i;
}

#ifdef ULLONG_MAX
void GString::formatUInt(unsigned long long x, char *buf, int bufSize,
			 GBool zeroFill, int width, int base,
			 const char **p, int *len) {
#else
void GString::formatUInt(Gulong x, char *buf, int bufSize,
			 GBool zeroFill, int width, int base,
			 const char **p, int*len) {
#endif
  static const char* vals = "0123456789abcdef";
  if (!buf || bufSize == 0) {
    return;
  }
  int i = bufSize;
  if (x == 0) {
    buf[--i] = '0';
  } else if (base) {
    while (i > 0 && x) {
      buf[--i] = vals[x % base];
      x /= base;
    }
  }
  if (zeroFill) {
    for (int j = bufSize - i; i > 0 && j < width; ++j) {
      buf[--i] = '0';
    }
  }
  *p = buf + i;
  *len = bufSize - i;
}

void GString::formatDouble(double x, char *buf, int bufSize, int prec,
			   GBool trim, const char **p, int *len) {
  GBool neg, started;
  double x2;
  int d, i, j;
  neg = x < 0;
  x = fabs(x);
  x = floor(x * pow(10.0, prec) + 0.5);
  i = bufSize;
  started = !trim;
  for (j = 0; j < prec && i > 1; ++j) {
    x2 = floor(0.1 * (x + 0.5));
    d = (int)floor(x - 10 * x2 + 0.5);
    if (started || d != 0) {
      buf[--i] = (char)('0' + d);
      started = gTrue;
    }
    x = x2;
  }
  if (i > 1 && started) {
    buf[--i] = '.';
  }
  if (i > 1) {
    do {
      x2 = floor(0.1 * (x + 0.5));
      d = (int)floor(x - 10 * x2 + 0.5);
      buf[--i] = (char)('0' + d);
      x = x2;
    } while (i > 1 && x);
  }
  if (neg) {
    buf[--i] = '-';
  }
  *p = buf + i;
  *len = bufSize - i;
}

GString *GString::insert(int i, char c) {
  return insert(i, &c, 1);
}

GString *GString::insert(int i, GString *str) {
  if (str) {
    insert(i, str->getCString(), str->getLength());
  }
  return this;
}

GString *GString::insert(int i, const char *str) {
  if (str) {
    insert(i, str, (int)strlen(str));
  }
  return this;
}

GString *GString::insert(int i, const char *str, int lengthA) {
  if (lengthA < 0 || length > INT_MAX - lengthA) {
    gMemError("Integer overflow in GString::insert()");
  }
  if ((i < 0) || (i > length)) {
    gMemError("GString::insert() index > length");
  }
  if (str) {
    resize(length + lengthA);
    if (s) {
      memmove(s + i + lengthA, s + i, length - i + 1);
      memcpy(s + i, str, lengthA);
      length += lengthA;
    }
  }
  return this;
}

GString *GString::del(int i, int n) {
  if (getCString() && (i >= 0 && n > 0 && (i <= INT_MAX - n))) {
    if (i > length)
      i = length;
    if (i + n > length) {
      n = length - i;
    }
    // for (j = i; j <= length - n; ++j) {
    //   s[j] = s[j + n];
    // }
    memmove(s + i, s + i + n, length - (i + n) + 1);
    resize(length -= n);
  }
  return this;
}

GString *GString::upperCase() {
  for (int i = 0; i < length; ++i) {
    s[i] = (char)toupper(s[i] & 0xff);
  }
  return this;
}

GString *GString::lowerCase() {
  for (int i = 0; i < length; ++i) {
    s[i] = (char)tolower(s[i] & 0xff);
  }
  return this;
}

int GString::cmp(GString *str) const {
  const auto n1{ getLength() }, n2{ str ? str->getLength() : 0 };
  auto p1{ getCString() };
  auto p2{ str ? str->getCString() : nullptr };

  if (p1 && p2) {
    for (int i = 0; i < n1 && i < n2; ++i, ++p1, ++p2) {
      auto x = (*p1 & 0xff) - (*p2 & 0xff);
      if (x != 0) {
        return x;
      }
    }
  }
  return n1 - n2;
}

int GString::cmpN(GString *str, int n) const {
  const auto n1{ getLength() }, n2{ str ? str->getLength() : 0 };
  auto p1{ getCString() };
  auto p2{ str ? str->getCString() : nullptr };

  if (p1 && p2) {
    int i{ 0 };
    for (; i < n1 && i < n2 && i < n; ++i, ++p1, ++p2) {
      auto x = (*p1 & 0xff) - (*p2 & 0xff);
      if (x != 0) {
        return x;
      }
    }
    if (i == n) {
      return 0;
    }
  }
  return n1 - n2;
}

int GString::cmp(const char *sA) const {
  auto n1{ getLength() };
  auto p1{ getCString() };
  auto p2{ sA };
  if (p1 && p2) {
    int i{ 0 };
    for (; i < n1 && *p2; ++i, ++p1, ++p2) {
      auto x = (*p1 & 0xff) - (*p2 & 0xff);
      if (x != 0) {
        return x;
      }
    }
    if (i < n1) {
      return 1;
    }
    if (*p2) {
      return -1;
    }
    return 0;
  }
  return 1;
}

int GString::cmpN(const char *sA, int n) const {
  auto n1{ getLength() };
  auto p1{ getCString() };
  auto p2{ sA };

  if (p1 && p2) {
    int i{ 0 };
    for (; i < n1 && i < n && *p2 ; ++i, ++p1, ++p2) {
      auto x = (*p1 & 0xff) - (*p2 & 0xff);
        if (x != 0) {
          return x;
       }
    }
    if (i == n) {
      return 0;
    }
    if (i < n1) {
      return 1;
    }
    if (*p2) {
      return -1;
    }
    return 0;
  }
  return 1;
}

char GString::getChar(int i) const {
  if (s && (i >= 0) && (i < length)) {
    return s[i]; 
  }
  return 0; 
}

void GString::setChar(int i, char c) {
  if (s && (i >= 0) && (i < length)) {
    s[i] = c; 
  } 
}

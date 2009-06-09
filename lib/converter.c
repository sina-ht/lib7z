/*
 * converter.c -- Character code converter
 * (C)Copyright 2001-2004 by Hiroshi Takekawa
 * This file is part of lib7z.
 *
 * Last Modified: Sun Sep  5 14:44:09 2004.
 * $Id$
 *
 * lib7z is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * lib7z is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "common.h"

#include "converter.h"

#ifdef HAVE_ICONV

#define ICONV_OUTPUT_SIZE 65536
#include <iconv.h>

int
converter_convert(char *s, char **d_r, size_t insize, char *from, char *to)
{
  iconv_t cd;
  char d[ICONV_OUTPUT_SIZE];
  char *inptr, *outptr;
  size_t avail, nconv;

  if (strcasecmp(from, "noconv") == 0) {
    *d_r = strdup(s);
    return strlen(s);
  }
  if (!s) {
    *d_r = NULL;
    return 0;
  }
  if (insize == 0) {
    *d_r = strdup("");
    return 0;
  }

  if ((cd = iconv_open(to, from)) == (iconv_t)-1) {
    if (errno == EINVAL) {
      err_message("%s: conversion from %s to %s is not supported by iconv().\n", __FUNCTION__, from, to);
      *d_r = strdup(s);
      return strlen(s);
    }
    perror("converter_convert");
    return -errno;
  }

  inptr = s;
  outptr = d;
  avail = ICONV_OUTPUT_SIZE - 1;

  if ((nconv = iconv(cd, (ICONV_CONST char **)&inptr, &insize, &outptr, &avail)) != (size_t)-1) {
    *outptr = '\0';
    *d_r = strdup(d);
  } else {
    debug_message_fnc("iconv error occurred: nconv = %d.\n", nconv);
    switch (errno) {
    case E2BIG:
      debug_message("Increase ICONV_OUTPUT_SIZE and recompile.\n");
      break;
    case EILSEQ:
      debug_message_fnc("Invalid sequence passed: %s\n", inptr);
      break;
    case EINVAL:
      debug_message("Incomplete multi-byte sequence passed.\n");
      break;
    default:
      perror("converter_convert");
      break;
    }
    *outptr = '\0';
    *d_r = NULL;
    iconv_close(cd);
    return -errno;
  }

  iconv_close(cd);

  return strlen(*d_r);
}
#else
int
converter_convert(char *s, char **d_r, size_t insize, char *from, char *to)
{
  *d_r = strdup(s);
  return strlen(*d_r);
}
#endif

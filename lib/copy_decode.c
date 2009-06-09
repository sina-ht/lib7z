/*
 * copy_decode.c
 * (C)Copyright 2004-2006 by Hiroshi Takekawa
 * This file is part of lib7z.
 *
 * Last Modified: Fri Apr 14 00:12:51 2006.
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

#include <stdlib.h>

#include "7z.h"
#include "common.h"
#include "copy_decode.h"

typedef struct _copy_decoder {
  UINT64 offset;
  I7z_stream *st;
} COPY_Decoder;

void *
copy_decoder_create(I7z_stream *st, unsigned char *prop, unsigned int propsize)
{
  COPY_Decoder *dec;

  if ((dec = calloc(1, sizeof(*dec))) == NULL) {
    err_message_fnc("No enough memory.\n");
    return NULL;
  }
  dec->st = st;
  dec->offset = 0;

  return dec;
}

void
copy_decoder_destroy(void *_dec)
{
  COPY_Decoder *dec = _dec;

  if (dec)
    free(dec);
}

int
copy_decode(void *_dec, unsigned char *output, UINT64 *outsize)
{
  COPY_Decoder *dec = _dec;

  *outsize = i7z_stream_read(dec->st, output, *outsize);
  dec->offset += *outsize;

  return 0;
}

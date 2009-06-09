/*
 * memorystream.c
 * (C)Copyright 2004 by Hiroshi Takekawa
 * This file is part of lib7z.
 *
 * Last Modified: Sun Jul 24 00:20:38 2005.
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

#include "7z.h"
#include "memorystream.h"

static int
memorystream_read(void *obj, unsigned char *buf, unsigned int size)
{
  I7z_memorystream *mst = obj;
  unsigned int read_size = (mst->offset + size <= mst->size) ? size : mst->size - mst->offset;

  memcpy(buf, mst->buf + mst->offset, read_size);
  mst->offset += read_size;

  return read_size;
}

static int
memorystream_seek(void *obj, int offset, int whence)
{
  I7z_memorystream *mst = obj;

  switch (whence) {
  case _I7z_END:
    offset += mst->size;
    break;
  case _I7z_CUR:
    offset += mst->offset;
    break;
  case _I7z_SET:
    break;
  default:
    return -3;
  }

  if (offset < 0)
    return -1;
  if (offset > mst->size)
    return -2;
  mst->offset = offset;

  return 0;
}

static unsigned int
memorystream_tell(void *obj)
{
  I7z_memorystream *mst = obj;
  return mst->offset;
}

static void
memorystream_close(void *obj)
{
  I7z_memorystream *mst = obj;

  if (mst->buf) {
    free(mst->buf);
    mst->buf = NULL;
  }
}

static void
memorystream_destroy(void *obj)
{
  memorystream_close(obj);
  free(obj);
}

int
i7z_stream_make_memorystream(I7z_stream *st, unsigned char *buf, unsigned int size)
{
  I7z_memorystream *mst = calloc(1, sizeof(*mst));

  mst->buf = buf;
  mst->size = size;
  mst->offset = 0;

  st->obj = mst;
  st->read_func = memorystream_read;
  st->seek_func = memorystream_seek;
  st->tell_func = memorystream_tell;
  st->close_func = memorystream_close;
  st->destroy_func = memorystream_destroy;

  return 1;
}

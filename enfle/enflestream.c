/*
 * enflestream.c
 * (C)Copyright 2004 by Hiroshi Takekawa
 * This file is part of Enfle.
 *
 * Last Modified: Fri Oct  7 08:13:10 2005.
 * $Id$
 *
 * Enfle is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Enfle is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include <stdio.h>

#define REQUIRE_STRING_H
#include "compat.h"

#include "stream.h"
#include "7z.h"
#include "enflestream.h"
#include "enflestream_private.h"

static int
enflestream_read(void *obj, unsigned char *buf, unsigned int size)
{
  I7z_enflestream *mst = obj;
  return stream_read(mst->st, buf, size);
}

static int
enflestream_seek(void *obj, int offset, int whence)
{
  I7z_enflestream *mst = obj;
  return stream_seek(mst->st, offset, whence);
}

static unsigned int
enflestream_tell(void *obj)
{
  I7z_enflestream *mst = obj;
  return stream_tell(mst->st);
}

static void
enflestream_close(void *obj)
{
  I7z_enflestream *mst = obj;
  stream_close(mst->st);
}

static void
enflestream_destroy(void *obj)
{
  free(obj);
}

void
i7z_stream_set_enflestream(I7z_stream *st, Stream *enfle_st)
{
  I7z_enflestream *mst = st->obj;
  mst->st = enfle_st;
}

int
i7z_stream_make_enflestream(I7z_stream *st, Stream *enfle_st)
{
  I7z_enflestream *mst = calloc(1, sizeof(*mst));

  mst->st = enfle_st;

  st->obj = mst;
  st->read_func = enflestream_read;
  st->seek_func = enflestream_seek;
  st->tell_func = enflestream_tell;
  st->close_func = enflestream_close;
  st->destroy_func = enflestream_destroy;

  return 1;
}

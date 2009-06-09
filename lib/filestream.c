/*
 * filestream.c
 * (C)Copyright 2004 by Hiroshi Takekawa
 * This file is part of lib7z.
 *
 * Last Modified: Sun Sep  5 14:44:40 2004.
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
#include "filestream.h"

static int
filestream_read(void *obj, unsigned char *buf, unsigned int size)
{
  I7z_filestream *mst = obj;
  return fread(buf, 1, size, mst->fp);
}

static int
filestream_seek(void *obj, int offset, int whence)
{
  I7z_filestream *mst = obj;
  return fseek(mst->fp, offset, whence);
}

static unsigned int
filestream_tell(void *obj)
{
  I7z_filestream *mst = obj;
  return ftell(mst->fp);
}

static void
filestream_close(void *obj)
{
  I7z_filestream *mst = obj;
  fclose(mst->fp);
}

int
i7z_stream_make_filestream(I7z_stream *st, char *path)
{
  I7z_filestream *mst = calloc(1, sizeof(*mst));

  if ((mst->fp = fopen(path, "rb")) == NULL)
    return 0;

  st->obj = mst;
  st->read_func = filestream_read;
  st->seek_func = filestream_seek;
  st->tell_func = filestream_tell;
  st->close_func = filestream_close;

  return 1;
}

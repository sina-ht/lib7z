/*
 * 7z.c
 * (C)Copyright 2004 by Hiroshi Takekawa
 * This file is part of lib7z.
 *
 * Last Modified: Sun Jul 24 00:25:18 2005.
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

#define PACKAGE "7z"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "7z.h"
#include "common.h"

static void
extract_files(I7z *i7z, I7z_stream *st)
{
  unsigned char *outbuf;
  UINT64 outsize;
  int res;
  I7z_pack *pack = &i7z->main_pack;
  I7z_coder *coder = NULL;
  void *dec = NULL;
  unsigned int i;

  for (i = 0; i < i7z->nfiles; i++) {
    int nfolder = i7z->file_to_folder[i];
    I7z_folder *folder = &pack->folders[nfolder];
    unsigned int idx = i7z->file_to_substream[i];
    unsigned int idx_f = idx - folder->idx_base;

    if (idx == (unsigned int)-1)
      continue;
    if (idx_f == 0) {
      debug_message_fnc("Start of Folder %u\n", nfolder);
      i7z_stream_seek(st, pack->folders[nfolder].stream_base, _I7z_SET);
      if (dec)
	coder->destroy(dec);
      coder = &pack->folders[nfolder].coders[0];
      if ((dec = coder->create(st, coder->properties, coder->propertiessize)) == NULL)
	err_message_fnc("create() failed\n");
    }

    outsize = folder->unsubstreamsizes[idx_f];
    if ((outbuf = malloc(outsize)) == NULL) {
      err_message_fnc("Cannot allocate %d bytes\n", outsize);
      return;
    }
    debug_message_fnc("File #%u: Substream #%d: outsize = %llu (Folder #%u)\n", i, idx, outsize, nfolder);
    if ((res = coder->decode(dec, outbuf, &outsize)) == 0) {
      FILE *fp;
      char fname[9];

      debug_message_fnc("  Decoded: outsize = %lld\n", outsize);
      snprintf(fname, 9, "F%02llu%05d", nfolder, idx);
      if (idx_f == 0) {
	fp = fopen(fname, "wb");
	fwrite(outbuf, 1, outsize, fp);
	fflush(fp);
	fclose(fp);
      }
      free(outbuf);
    } else {
      debug_message_fnc("decode() failed: error code = %d.\n", res);
    }
  }
  if (coder && dec)
    coder->destroy(dec);
}

int
main(int argc, char **argv)
{
  I7z_stream *st;
  I7z *i7z;
  int res;

  if (argc == 1) {
    printf("Usage: %s filename\n", argv[0]);
    return 0;
  }

  if ((st = i7z_stream_create()) == NULL)
    exit(1);
  if (!i7z_stream_make_filestream(st, argv[1]))
    exit(1);

  if ((i7z = i7z_create()) == NULL)
    goto out;

  if (i7z_identify(i7z, st) != 0)
    goto out_free_i7z;

  if ((res = i7z_parse_header(&i7z, st)) < 0)
    goto out_free_i7z;

  extract_files(i7z, st);

 out_free_i7z:
  i7z_free(i7z);
 out:
  i7z_stream_destroy(st);

  return 0;
}

/*
 * 7z.c -- 7z archiver plugin
 * (C)Copyright 2000-2004 by Hiroshi Takekawa
 * This file is part of Enfle.
 *
 * Last Modified: Thu Sep 23 04:24:59 2004.
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
#include <stdlib.h>
#include <ctype.h>

#define REQUIRE_STRING_H
#include "compat.h"
#include "common.h"

#include "enfle/archiver-plugin.h"

#include "7z.h"

typedef struct _7z_data {
  I7z *i7z;
  I7z_stream *i7z_st;
  I7z_coder *coder;
  void *dec;
  int current_index;
  int current_folder;
} I7z_data;

typedef struct _7z_info {
  I7z_data *i7z_d;
  int idx;
} I7z_info;

DECLARE_ARCHIVER_PLUGIN_METHODS;

static ArchiverPlugin plugin = {
  .type = ENFLE_PLUGIN_ARCHIVER,
  .name = "7z",
  .description = "7z Archiver plugin version 0.1",
  .author = "Hiroshi Takekawa",
  .archiver_private = NULL,

  .identify = identify,
  .open = open
};

ENFLE_PLUGIN_ENTRY(archiver_7z)
{
  ArchiverPlugin *arp;

  if ((arp = (ArchiverPlugin *)calloc(1, sizeof(ArchiverPlugin))) == NULL)
    return NULL;
  memcpy(arp, &plugin, sizeof(ArchiverPlugin));

  return (void *)arp;
}

ENFLE_PLUGIN_EXIT(archiver_7z, p)
{
  free(p);
}

/* override methods */

static int /* overrides archive::open */
I7z_open(Archive *arc, Stream *st, char *path)
{
  I7z_info *i7z_i = (I7z_info *)archive_get(arc, path);
  I7z_data *i7z_d;
  I7z *i7z;
  I7z_stream *i7z_st;
  I7z_pack *pack;
  I7z_folder *folder;
  unsigned char *outbuf;
  int i, res, skip, nfolder;
  UINT64 outsize;

  if (i7z_i == NULL) {
    err_message_fnc("archive_get(%s) failed.\n", path);
    return OPEN_ERROR;
  }
  debug_message_fnc("path: %s, idx: %d\n", path, i7z_i->idx);
  i7z_d = i7z_i->i7z_d;
  i7z = i7z_d->i7z;
  i7z_st = i7z_d->i7z_st;
  pack = &i7z->main_pack;
  debug_message_fnc("file_to_folder: %p\n", i7z->file_to_folder);
  nfolder = i7z->file_to_folder[i7z_i->idx];
  folder = &pack->folders[nfolder];

  skip = 0;
  if (nfolder != i7z_d->current_folder)
    i7z_d->current_index = -1;
  if (i7z_i->idx != i7z_d->current_index) {
    if (i7z_d->current_index == -1 || i7z_i->idx < i7z_d->current_index) {
      debug_message_fnc("Decompress from start of folder %d...\n", nfolder);
      stream_seek(arc->st, folder->stream_base, _SET);
      if (i7z_d->dec)
	i7z_d->coder->destroy(i7z_d->dec);
      i7z_d->coder = &pack->folders[nfolder].coders[0];
      if ((i7z_d->dec = i7z_d->coder->create(i7z_st, i7z_d->coder->properties, i7z_d->coder->propertiessize)) == NULL)
	err_message_fnc("create() failed\n");
      i7z_d->current_index = folder->idx_base;
      i7z_d->current_folder = nfolder;
    }
    skip = i7z_i->idx - i7z_d->current_index;
  }

  debug_message_fnc("skip = %d\n", skip);
  for (i = 0; i < skip + 1; i++) {
    if (i7z->file_to_substream[i7z_i->idx - skip + i] == -1)
      continue;
    debug_message_fnc("current_index = %d, idx_base = %d\n", i7z_d->current_index, folder->idx_base);
    outsize = folder->unsubstreamsizes[i7z_d->current_index - folder->idx_base];
    i7z_d->current_index++;
    outbuf = malloc(outsize);
    if ((res = i7z_d->coder->decode(i7z_d->dec, outbuf, &outsize)) != 0) {
      debug_message_fnc("decode() failed: error code = %d.\n", res);
      free(outbuf);
      return OPEN_ERROR;
    }
    if (i == skip)
      break;
    debug_message_fnc(" Skipping %d / %d\n", i + 1, skip);
    free(outbuf);
  }

  debug_message_fnc("Substream #%d(Folder #%d): size = %lld\n", i7z_i->idx, nfolder, outsize);

  st->path = strdup(path);
  return stream_make_memorystream(st, outbuf, outsize);
}

static void /* overrides archive::destroy */
I7z_destroy(Archive *arc)
{
  I7z_data *i7z_d = arc->data;
  I7z *i7z = i7z_d->i7z;
  I7z_coder *coder = &i7z->main_pack.folders[0].coders[0];

  if (i7z_d->dec)
    i7z_d->coder->destroy(i7z_d->dec);
  i7z_free(i7z);
  i7z_stream_close(i7z_d->i7z_st);
  i7z_stream_destroy(i7z_d->i7z_st);
  free(i7z_d);
  stream_destroy(arc->st);
  hash_destroy(arc->filehash);
  if (arc->path)
    free(arc->path);
  free(arc);
}

/* methods */

DEFINE_ARCHIVER_PLUGIN_IDENTIFY(a, st, priv)
{
  I7z *i7z = NULL;
  I7z_stream *i7z_st = NULL;
  int res;

  if ((i7z = i7z_create()) == NULL)
    return OPEN_ERROR;
  if ((i7z_st = i7z_stream_create()) == NULL) {
    res = OPEN_ERROR;
    goto out;
  }
  i7z_stream_make_enflestream(i7z_st, st);
  if (i7z_identify(i7z, i7z_st) != 0) {
    res = OPEN_NOT;
    goto out;
  }

  res = OPEN_OK;
 out:
  if (i7z_st)
    i7z_stream_destroy(i7z_st);
  if (i7z)
    i7z_free(i7z);

  return res;
}

DEFINE_ARCHIVER_PLUGIN_OPEN(a, st, priv)
{
  I7z *i7z = NULL;
  I7z_stream *i7z_st = NULL;
  Dlist *dl;
  Dlist_data *dd;
  I7z_data *i7z_d;
  I7z_info *i7z_i;
  Hash *hash;
  unsigned int i;
  int res;

#ifdef IDENTIFY_BEFORE_OPEN
  if (identify(a, st, priv) == OPEN_NOT)
    return OPEN_NOT;
  stream_rewind(st);
#endif

  if ((i7z = i7z_create()) == NULL)
    return OPEN_ERROR;
  if ((i7z_st = i7z_stream_create()) == NULL) {
    res = OPEN_ERROR;
    goto out_free_i7z;
  }
  i7z_stream_make_enflestream(i7z_st, st);
  if (i7z_identify(i7z, i7z_st) != 0) {
    res = OPEN_NOT;
    goto out_free_i7z;
  }

  i7z_stream_tell(i7z_st);
  if ((res = i7z_parse_header(&i7z, i7z_st)) < 0) {
    res = OPEN_ERROR;
    goto out_free_i7z;
  }

  i7z_stream_tell(i7z_st);
  if ((i7z_d = calloc(1, sizeof(*i7z_d))) == NULL) {
    res = OPEN_ERROR;
    goto out_free_i7z;
  }

  i7z_stream_tell(i7z_st);
  i7z_d->current_index = -1;
  i7z_d->current_folder = -1;
  i7z_d->i7z = i7z;
  i7z_d->i7z_st = i7z_st;
  a->data = i7z_d;

  if (i7z->nfiles == 0) {
    res = OPEN_OK;
    goto out_free_i7z;
  }

  dl = dlist_create();
  hash = hash_create(ARCHIVE_FILEHASH_SIZE);
  for (i = 0; i < i7z->nfiles; i++) {
    char *path;
    if (i7z->file_to_substream[i] == (UINT64)-1)
      continue;
    path = calloc(1, strlen(i7z->filenames[i]) + 2);
    path[0] = '#';
    strcat(path, i7z->filenames[i]);
    dlist_add(dl, path);
    if ((i7z_i = calloc(1, sizeof(*i7z_i))) == NULL)
      return OPEN_ERROR;
    i7z_i->i7z_d = i7z_d;
    i7z_i->idx = i7z->file_to_substream[i];
    if (hash_define_str_value(hash, path, i7z_i) < 0) {
      warning("%s: %s: %s already in hash.\n", __FILE__, __FUNCTION__, path);
    }
  }

  /* Don't sort */
  //dlist_set_compfunc(dl, archive_key_compare);
  //dlist_sort(dl);

  dlist_iter (dl, dd) {
    void *remainder = hash_lookup_str(hash, dlist_data(dd));

    if (!remainder) {
      warning("%s: %s: %s not in hash.\n", __FILE__, __FUNCTION__, (char *)dlist_data(dd));
    } else {
      I7z_info *i7z_i = remainder;
      debug_message("7z: %s: add %s as index %d\n", __FUNCTION__, (char *)dlist_data(dd), i7z_i->idx);
      archive_add(a, dlist_data(dd), (void *)i7z_i);
    }
  }
  hash_destroy(hash);
  dlist_destroy(dl);

  a->path = strdup(st->path);
  a->st = stream_transfer(st);
  i7z_stream_set_enflestream(i7z_st, a->st);
  a->open = I7z_open;
  a->destroy = I7z_destroy;

  return OPEN_OK;

 out_free_i7z:
  if (i7z_st)
    i7z_stream_destroy(i7z_st);
  if (i7z)
    i7z_free(i7z);

  return res;
}

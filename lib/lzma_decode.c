/*
 * lzma_decode.c
 * (C)Copyright 2004 by Hiroshi Takekawa
 * This file is part of lib7z.
 *
 * Last Modified: Sat Mar 14 01:21:01 2009.
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
#include "wrapper/7zip/Compress/LZMA_C/LzmaDecode.h"
#include "lzma_decode.h"

#if !defined(_LZMA_IN_CB)
#error "_LZMA_IN_CB should be defined in LzmaDecode.h"
#endif
#if !defined(_LZMA_OUT_READ)
#error "_LZMA_OUT_READ should be defined in LzmaDecode.h"
#endif

#define READ_BUFFER_SIZE 4096
typedef struct _read_stream {
  ILzmaInCallback InCallback;
  I7z_stream *st;
  unsigned char buf[READ_BUFFER_SIZE];
} ReadStream;

typedef struct _lzma_decoder {
  CLzmaDecoderState state;
  UINT64 offset;
  ReadStream *rs;
} LZMA_Decoder;

static int
read_stream(void *obj, const unsigned char **buf, SizeT *size)
{
  ReadStream *rs = obj;

  *size = i7z_stream_read(rs->st, rs->buf, READ_BUFFER_SIZE);
  *buf = rs->buf;
  return LZMA_RESULT_OK;
}

void *
lzma_decoder_create(I7z_stream *st, unsigned char *prop, unsigned int propsize)
{
  ReadStream *rs;
  LZMA_Decoder *dec;

  if ((dec = calloc(1, sizeof(*dec))) == NULL) {
    err_message_fnc("No enough memory.\n");
    return NULL;
  }

  /* Decode LZMA properties and allocate memory */
  if (LzmaDecodeProperties(&dec->state.Properties, prop, LZMA_PROPERTIES_SIZE) != LZMA_RESULT_OK) {
    err_message_fnc("Incorrect stream properties.\n");
    return NULL;
  }
  if ((dec->state.Probs = (CProb *)malloc(LzmaGetNumProbs(&dec->state.Properties) * sizeof(CProb))) == NULL) {
    free(dec);
    err_message_fnc("No enough memory.\n");
    return NULL;
  }
  if ((dec->state.Dictionary = (unsigned char *)malloc(dec->state.Properties.DictionarySize)) == NULL) {
    free(dec->state.Probs);
    free(dec);
    err_message_fnc("No enough memory.\n");
    return NULL;
  }

  if ((rs = calloc(1, sizeof(*rs))) == NULL) {
    err_message_fnc("No enough memory.\n");
    return NULL;
  }
  rs->InCallback.Read = read_stream;
  rs->st = st;

  LzmaDecoderInit(&dec->state);

  dec->offset = 0;
  dec->rs = rs;

  return dec;
}

void
lzma_decoder_destroy(void *_dec)
{
  LZMA_Decoder *dec = _dec;

  if (dec) {
    if (dec->rs)
      free(dec->rs);
    if (dec->state.Probs)
      free(dec->state.Probs);
    if (dec->state.Dictionary)
      free(dec->state.Dictionary);
    free(dec);
  }
}

int
lzma_decode(void *_dec, unsigned char *output, UINT64 *outsize)
{
  LZMA_Decoder *dec = _dec;
  UINT64 decoded;
  UINT32 kBlockSize = 0x100000;
  int result;

  decoded = 0;
  while (decoded < *outsize) {
    SizeT blocksize = *outsize - decoded;
    SizeT processed;

    if (blocksize > kBlockSize)
      blocksize = kBlockSize;
    processed = 0;
    result = LzmaDecode(&dec->state, &dec->rs->InCallback,
			output + decoded, blocksize,
			&processed);
    switch (result) {
    case 0:
      break;
    case LZMA_RESULT_DATA_ERROR:
      err_message_fnc("Data error.\n");
      return LZMA_DATA_ERROR;
    default:
      err_message_fnc("LzmaDecode returned %d.\n", result);
      return UNKNOWN_ERROR;
    }
    if (processed == 0) {
      *outsize = decoded;
      break;
    }
    debug_message_fnc("LzmaDecode() returned %d, processed = %d.\n", result, processed);
    decoded += processed;
    dec->offset += processed;
  }

  return 0;
}

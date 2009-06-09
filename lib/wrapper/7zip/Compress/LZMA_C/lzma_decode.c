/*
 * lzma_decode.c
 * (C)Copyright 2004 by Hiroshi Takekawa
 * This file is part of lib7z.
 *
 * Last Modified: Sun Sep  5 14:41:42 2004.
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
#include "LzmaDecode.h"
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
  UINT64 offset;
  ReadStream *rs;
  unsigned char *internal_data;
  unsigned char *dictionary;
} LZMA_Decoder;

static int
read_stream(void *obj, unsigned char **buf, unsigned int *size)
{
  ReadStream *rs = obj;

  *size = i7z_stream_read(rs->st, rs->buf, READ_BUFFER_SIZE);
  *buf = rs->buf;
  return LZMA_RESULT_OK;
}

void *
lzma_decoder_create(I7z_stream *st, unsigned char *prop, unsigned int propsize)
{
  int i, lc, lp, pb;
  unsigned int lzmaInternalSize;
  unsigned char *lzmaInternalData;
  unsigned int dictionarySize;
  unsigned char *dictionary;
  ReadStream *rs;
  unsigned char prop0 = prop[0];
  LZMA_Decoder *dec;

  /*
   * 1) Read first byte of properties for LZMA compressed stream, 
   * check that it has correct value and calculate three 
   * LZMA property variables:
   */

  if (prop0 >= (9 * 5 * 5)) {
    err_message_fnc("Property error\n");
    return NULL;
  }
  for (pb = 0; prop0 >= (9 * 5); pb++, prop0 -= (9 * 5)) ;
  for (lp = 0; prop0 >= 9; lp++, prop0 -= 9) ;
  lc = prop0;

  /*
   * 2) Calculate required amount for LZMA lzmaInternalSize:
   */

  lzmaInternalSize = (LZMA_BASE_SIZE + (LZMA_LIT_SIZE << (lc + lp))) * sizeof(CProb);
  lzmaInternalSize += 100;

  /*
   * 3) Allocate that memory with malloc or some other function:
   */

  if ((lzmaInternalData = malloc(lzmaInternalSize)) == NULL) {
    err_message_fnc("No enough memory.\n");
    return NULL;
  }

  /*
   * 4b) Decompress data with buffering:
   */

  if ((rs = calloc(1, sizeof(*rs))) == NULL) {
    err_message_fnc("No enough memory.\n");
    return NULL;
  }
  rs->InCallback.Read = read_stream;
  rs->st = st;

  dictionarySize = 0;
  for (i = 0; i < 4; i++)
    dictionarySize += (unsigned int)prop[i + 1] << (i * 8);

  if ((dictionary = malloc(dictionarySize)) == NULL) {
    free(lzmaInternalData);
    err_message_fnc("No enough memory.\n");
    return NULL;
  }

  debug_message_fnc("LzmaDecoderInit: internalsize = %d, dictionarysize = %d\n", lzmaInternalSize, dictionarySize);

  LzmaDecoderInit(lzmaInternalData, lzmaInternalSize,
		  lc, lp, pb,
		  dictionary, dictionarySize,
		  &rs->InCallback);

  if ((dec = calloc(1, sizeof(*dec))) == NULL) {
    free(lzmaInternalData);
    err_message_fnc("No enough memory.\n");
    return NULL;
  }
  dec->offset = 0;
  dec->rs = rs;
  dec->internal_data = lzmaInternalData;
  dec->dictionary = dictionary;

  return dec;
}

void
lzma_decoder_destroy(void *_dec)
{
  LZMA_Decoder *dec = _dec;

  if (dec) {
    if (dec->dictionary)
      free(dec->dictionary);
    if (dec->internal_data)
      free(dec->internal_data);
    if (dec->rs)
      free(dec->rs);
    free(dec);
  }
}

int
lzma_decode(void *_dec, unsigned char *output, UINT64 *outsize)
{
  LZMA_Decoder *dec = _dec;
  unsigned int processed;
  UINT64 decoded;
  UINT32 kBlockSize = 0x10000;
  int result;

  decoded = 0;
  while (decoded < *outsize) {
    UINT32 blocksize = *outsize - decoded;

    if (blocksize > kBlockSize)
      blocksize = kBlockSize;
    processed = 0;
    result = LzmaDecode(dec->internal_data,
			output + decoded, blocksize,
			&processed);
    switch (result) {
    case 0:
      break;
    case LZMA_RESULT_DATA_ERROR:
      err_message_fnc("Data error.\n");
      return LZMA_DATA_ERROR;
    case LZMA_RESULT_NOT_ENOUGH_MEM:
      err_message_fnc("No enough buffer.\n");
      return LZMA_NO_ENOUGH_BUFFER;
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

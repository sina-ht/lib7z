/*
 * ppmd_decode.c
 * (C)Copyright 2004 by Hiroshi Takekawa
 * This file is part of lib7z.
 *
 * Last Modified: Sun Jul 24 01:27:35 2005.
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


#include <stdlib.h>
#include <string.h>

#if defined(WIN32)
#include <initguid.h>
#else
#define INITGUID
#endif

#include "../../../Common/MyCom.h"
#include "PPMDDecoder.h"
#include "ppmd_decode.h"

struct PPMD_Decoder {
  I7z_stream *st;
  unsigned char *prop;
  unsigned int propsize;
  NCompress::NPPMD::CDecoder *decoder;
  UInt64 offset;
};

class CStreamInStream: public ISequentialInStream, public CMyUnknownImp
{
  I7z_stream *st;
  unsigned char *prop;
  unsigned int propsize;
  unsigned int prop_ptr;
public:
  MY_UNKNOWN_IMP

  unsigned int offset;
  CStreamInStream(I7z_stream *_st, unsigned char *_prop, unsigned int _propsize):
    st(_st), prop(_prop), propsize(_propsize), prop_ptr(0), offset(0) {}
  virtual ~CStreamInStream() {}
  HRESULT Read(void *_data, UINT32 _size, UINT32 *processedSize) {
    int read_size = 0;

    if (_size > 0)
      read_size += i7z_stream_read(st, ((unsigned char *)_data) + read_size, _size);
    offset += read_size;
    if (processedSize)
      *processedSize = read_size;
    return 0;
  }
  HRESULT ReadPart(void *data, UINT32 size, UINT32 *processedSize) {
    return Read(data, size, processedSize);
  }
};

class CMemoryOutStream: public ISequentialOutStream, public CMyUnknownImp
{
  unsigned char *data;
  unsigned int size;
public:
  MY_UNKNOWN_IMP

  unsigned int offset;
  CMemoryOutStream(unsigned char *_data, unsigned int _size): data(_data), size(_size), offset(0) {}
  virtual ~CMemoryOutStream() {}
  HRESULT Write(const void *_data, UINT32 _size, UINT32 *processedSize) {
    int write_size;

    write_size = (offset + _size <= size) ? _size : size - offset;
    memcpy(data, _data, write_size);
    offset += write_size;
    if (processedSize)
      *processedSize = write_size;
    return 0;
  }
  HRESULT WritePart(const void *data, UINT32 size, UINT32 *processedSize) {
    return Write(data, size, processedSize);
  }
};

class ProgressInfo: public ICompressProgressInfo, public CMyUnknownImp
{
  MY_UNKNOWN_IMP

  HRESULT SetRatioInfo(const UInt64 *inSize, const UInt64 *outSize) {
    return 0;
  }
};

void *
ppmd_decoder_create(I7z_stream *st, unsigned char *prop, unsigned int propsize)
{
  PPMD_Decoder *dec = new PPMD_Decoder;
  HRESULT res;

  if (!dec)
    return NULL;

  dec->st = st;
  dec->prop = prop;
  dec->propsize = propsize;
  dec->decoder = new NCompress::NPPMD::CDecoder;
  dec->offset = 0;
  printf("prop(size %d): %02X%02X%02X%02X%02X\n", propsize,
	 prop[0],
	 prop[1],
	 prop[2],
	 prop[3],
	 prop[4]
	 );
  res = dec->decoder->SetDecoderProperties2((const Byte *)prop, propsize);

  return dec;
}

void
ppmd_decoder_destroy(void *_dec)
{
  PPMD_Decoder *dec = (PPMD_Decoder *)_dec;

  if (dec) {
    if (dec->decoder)
      delete dec->decoder;
    delete dec;
  }
}

int
ppmd_decode(void *_dec, unsigned char *output, UInt64 *outsize)
{
  PPMD_Decoder *dec = (PPMD_Decoder *)_dec;
  ISequentialInStream *ifs;
  ISequentialOutStream *ofs;
  ICompressProgressInfo *pi;
  UInt64 insize;
  HRESULT res;

  ifs = new CStreamInStream(dec->st, dec->prop, dec->propsize);
  ofs = new CMemoryOutStream(output, *outsize);
  pi = new ProgressInfo;
  res = dec->decoder->Code(ifs, ofs, &insize, outsize, pi);
  dec->offset += *outsize;
  delete pi;

  return res;
}

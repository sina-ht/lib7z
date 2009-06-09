/*
 * ppmd_decode.c
 * (C)Copyright 2004 by Hiroshi Takekawa
 * This file is part of lib7z.
 *
 * Last Modified: Sun Sep  5 14:42:12 2004.
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
  ISequentialInStream *ifs;
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

    if (prop_ptr < propsize) {
      read_size = prop_ptr + _size <= propsize ? _size : propsize - prop_ptr;
      memcpy(_data, prop + prop_ptr, read_size);
      prop_ptr += read_size;
      _size -= read_size;
    }

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

  dec->ifs = new CStreamInStream(st, prop, propsize);
  dec->decoder = new NCompress::NPPMD::CDecoder;
  dec->offset = 0;
  res = dec->decoder->Create(1 << 20);
  res = dec->decoder->SetDecoderProperties(dec->ifs);

  return dec;
}

void
ppmd_decoder_destroy(void *_dec)
{
  PPMD_Decoder *dec = (PPMD_Decoder *)_dec;

  if (dec) {
    if (dec->ifs)
      delete dec->ifs;
    if (dec->decoder)
      delete dec->decoder;
    delete dec;
  }
}

int
ppmd_decode(void *_dec, unsigned char *output, UInt64 *outsize)
{
  PPMD_Decoder *dec = (PPMD_Decoder *)_dec;
  ISequentialOutStream *ofs;
  ICompressProgressInfo *pi;
  UInt64 insize;
  HRESULT res;

  ofs = new CMemoryOutStream(output, *outsize);
  pi = new ProgressInfo;
  res = dec->decoder->Code(dec->ifs, ofs, &insize, outsize, pi);
  dec->offset += *outsize;
  dec->decoder->Flush();
  delete pi;
  delete ofs;

  return res;
}

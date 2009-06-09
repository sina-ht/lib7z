// PPMDDecoder.cpp

#include "StdAfx.h"

//#include "Common/Defs.h"
//#include "Windows/Defs.h"

#include "PPMDDecoder.h"

namespace NCompress {
namespace NPPMD {

STDMETHODIMP CDecoder::SetDecoderProperties(ISequentialInStream *inStream)
{
  UINT32 processedSize;
  RINOK(inStream->Read(&_order, 
      sizeof(_order), &processedSize));
  if (processedSize != sizeof(_order))
    return E_FAIL;
  RINOK(inStream->Read(&_usedMemorySize, 
      sizeof(_usedMemorySize), &processedSize));
  if (processedSize != sizeof(_usedMemorySize))
    return E_FAIL;
  return S_OK;
}

class CDecoderFlusher
{
  CDecoder *_coder;
public:
  CDecoderFlusher(CDecoder *coder): _coder(coder) {}
  ~CDecoderFlusher()
  {
    _coder->Flush();
    // _coder->ReleaseStreams();
  }
};

UINT32 GetMatchLen(const BYTE *pointer1, const BYTE *pointer2, 
    UINT32 limit)
{  
  UINT32 i;
  for(i = 0; i < limit && *pointer1 == *pointer2; 
      pointer1++, pointer2++, i++);
  return i;
}

HRESULT CDecoder::Create(UInt32 size) {
    _outStream.Create(size);
    _rangeDecoder.Create(size);
    return S_OK;
}

STDMETHODIMP CDecoder::CodeReal(ISequentialInStream *inStream,
      ISequentialOutStream *outStream, const UInt64 *inSize, const UInt64 *outSize,
      ICompressProgressInfo *progress)
{
  /*
  if (outSize == NULL)
    return E_INVALIDARG;
  */

  CDecoderFlusher flusher(this);

  _outStream.Init();
  _outStream.SetStream(outStream);

  if (!inited) {
    _rangeDecoder.Init();
    _rangeDecoder.SetStream(inStream);
    progressPosValuePrev = 0;
    pos = 0;

    try {
      if (!_info.SubAllocator.StartSubAllocator(_usedMemorySize)) 
	return E_OUTOFMEMORY;
    } catch(...) {
      return E_OUTOFMEMORY;
    }

    // _info.Init();
    // _info.MaxOrder = _order; 
    _info.MaxOrder = 0;
    _info.StartModelRare(_order);
    inited = true;
  }

  UInt64 size = (outSize == NULL) ? (UInt64)(-1) : *outSize;
  UInt64 decoded = 0;

  while(decoded < size)
  {
    pos++;
    decoded++;
    int symbol = _info.DecodeSymbol(&_rangeDecoder);
    if (symbol < 0)
      return S_OK;
    _outStream.WriteByte(symbol);
    if (pos - progressPosValuePrev >= (1 << 18) && progress != NULL)
    {
      UInt64 inSize = _rangeDecoder.GetProcessedSize();
      RINOK(progress->SetRatioInfo(&inSize, &pos));
      progressPosValuePrev = pos;
    }
  }
  return S_OK;
}

STDMETHODIMP CDecoder::Code(ISequentialInStream *inStream,
    ISequentialOutStream *outStream, const UInt64 *inSize, const UInt64 *outSize,
    ICompressProgressInfo *progress)
{
  try { return CodeReal(inStream, outStream, inSize, outSize, progress); }
  catch(const COutBufferException &e) { return e.ErrorCode; }
  catch(const CInBufferException &e) { return e.ErrorCode; }
  catch(...) { return E_FAIL; }
}


}}

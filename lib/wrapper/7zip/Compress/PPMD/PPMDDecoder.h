// Compress/PPM/PPMDDecoder.h

#ifndef __COMPRESS_PPM_PPMD_DECODER_H
#define __COMPRESS_PPM_PPMD_DECODER_H

#include "Common/MyCom.h"

#include "../../ICoder.h"
#include "../../Common/OutBuffer.h"
#include "../RangeCoder/RangeCoder.h"

#include "PPMDDecode.h"

namespace NCompress {
namespace NPPMD {

class CDecoder : 
  public ICompressCoder,
  public ICompressSetDecoderProperties,
  public CMyUnknownImp
{
  NRangeCoder::CDecoder _rangeDecoder;

  COutBuffer _outStream;

  CDecodeInfo _info;

  BYTE _order;
  UINT32 _usedMemorySize;
  UInt64 progressPosValuePrev, pos;
  bool inited;

public:
  CDecoder(): inited(false) {};
  virtual ~CDecoder() {}
  MY_UNKNOWN_IMP1(ICompressSetDecoderProperties)

  /*
  void ReleaseStreams()
  {
    _rangeDecoder.ReleaseStream();
    _outStream.ReleaseStream();
  }
  */

  // STDMETHOD(Code)(UINT32 aNumBytes, UINT32 &aProcessedBytes);
  HRESULT Flush()
    { return _outStream.Flush(); }

  HRESULT Create(UInt32 size);
  STDMETHOD(CodeReal)(ISequentialInStream *inStream,
      ISequentialOutStream *outStream, 
      const UInt64 *inSize, const UInt64 *outSize,
      ICompressProgressInfo *progress);

  STDMETHOD(Code)(ISequentialInStream *inStream,
      ISequentialOutStream *outStream, const UInt64 *inSize, const UInt64 *outSize,
      ICompressProgressInfo *progress);


  // ICompressSetDecoderProperties
  STDMETHOD(SetDecoderProperties)(ISequentialInStream *inStream);

};

}}

#endif

// PPMDTest.cpp

#include <iostream>
#include <fstream>

#if defined(WIN32)
#include <initguid.h>
#else
#define INITGUID
#endif

#include "../../../Common/MyCom.h"
#include "PPMDEncoder.h"
#include "PPMDDecoder.h"

class CFileInStream: public ISequentialInStream, public CMyUnknownImp
{
  std::ifstream *st;
public:
  MY_UNKNOWN_IMP

  CFileInStream(char *path) {
	st = new std::ifstream(path);
  }
  virtual ~CFileInStream() {
	st->close();
  }
  HRESULT Read(void *data, UINT32 size, UINT32 *processedSize) {
	st->read((char *)data, size);
	if (processedSize)
	  *processedSize = st->gcount();
	return 0;
  }
  HRESULT ReadPart(void *data, UINT32 size, UINT32 *processedSize) {
	return Read(data, size, processedSize);
  }
};

class CFileOutStream: public ISequentialOutStream, public CMyUnknownImp
{
  std::ofstream *st;
public:
  MY_UNKNOWN_IMP

  CFileOutStream(char *path) {
	st = new std::ofstream(path);
  }
  virtual ~CFileOutStream() {
	st->close();
  }
  HRESULT Write(const void *data, UINT32 size, UINT32 *processedSize) {
	st->write((char *)data, size);
	st->flush();
	if (processedSize)
	  *processedSize = size;
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

int
main(int argc, char **argv)
{
  ISequentialInStream *ifs;
  ISequentialOutStream *ofs;
  ICompressProgressInfo *pi;
  NCompress::NPPMD::CEncoder *enc;
  NCompress::NPPMD::CDecoder *dec;
  HRESULT res;

  if (argc == 4) {
    if (argv[1][0] == 'e') {
      ifs = new CFileInStream(argv[2]);
      ofs = new CFileOutStream(argv[3]);
      pi = new ProgressInfo;
      enc = new NCompress::NPPMD::CEncoder;
      res = enc->Create(1 << 20);
      res = enc->WriteCoderProperties(ofs);
      res = enc->Code(ifs, ofs, 0, 0, pi);
      delete enc;
      delete pi;
      delete ofs;
      delete ifs;
    } else if (argv[1][0] == 'd') {
      ifs = new CFileInStream(argv[2]);
      ofs = new CFileOutStream(argv[3]);
      pi = new ProgressInfo;
      dec = new NCompress::NPPMD::CDecoder;
      res = dec->Create(1 << 20);
      res = dec->SetDecoderProperties(ifs);
      UInt64 outsize = 5;
      res = dec->Code(ifs, ofs, 0, &outsize, pi);
      res = dec->Code(ifs, ofs, 0, 0, pi);
      delete dec;
      delete pi;
      delete ofs;
      delete ifs;
    } else {
      std::cerr << "(e|d) infile outfile\n";
    }
  }

  return 0;
}

noinst_LTLIBRARIES = lib7z.la
INCLUDES = -I$(top_srcdir)/include -I$(top_srcdir)/lib/LZMA_SDK -I$(top_srcdir)/lib/LZMA_SDK/7zip/Compress/RangeCoder -I$(top_srcdir)/lib/LZMA_SDK/7zip/Compress/LZMA_C -I$(top_srcdir)/lib/LZMA_SDK/7zip/Compress/LZMA -I$(top_srcdir)/lib/LZMA_SDK/7zip/Compress/PPMD

noinst_HEADERS = \
	converter.h \
	filestream.h \
	memorystream.h \
	LZMA_SDK/Common/Alloc.h \
	LZMA_SDK/Common/CRC.h \
	LZMA_SDK/Common/MyCom.h \
	LZMA_SDK/Common/MyWindows.h \
	LZMA_SDK/Common/MyUnknown.h \
	LZMA_SDK/Common/StdAfx.h \
	LZMA_SDK/Common/Types.h \
	LZMA_SDK/7zip/ICoder.h \
	LZMA_SDK/7zip/IStream.h \
	LZMA_SDK/7zip/Common/InBuffer.h \
	LZMA_SDK/7zip/Common/OutBuffer.h \
	LZMA_SDK/7zip/Common/StdAfx.h \
	LZMA_SDK/7zip/Compress/RangeCoder/RangeCoder.h \
	LZMA_SDK/7zip/Compress/RangeCoder/RangeCoderBit.h \
	LZMA_SDK/7zip/Compress/RangeCoder/StdAfx.h \
	wrapper/7zip/Compress/PPMD/ppmd_decode.h \
	wrapper/7zip/Compress/PPMD/PPMDDecode.h \
	wrapper/7zip/Compress/PPMD/PPMDDecoder.h \
	wrapper/7zip/Compress/PPMD/PPMDContext.h \
	wrapper/7zip/Compress/PPMD/PPMDSubAlloc.h \
	wrapper/7zip/Compress/PPMD/PPMDType.h \
	wrapper/7zip/Compress/PPMD/StdAfx.h \
	wrapper/7zip/Compress/LZMA_C/lzma_decode.h \
	wrapper/7zip/Compress/LZMA_C/LzmaDecode.h

#	wrapper/7zip/Compress/PPMD/PPMDEncoder.cpp
#	wrapper/7zip/Compress/LZMA/LZMADecoder.cpp
#	LZMA_SDK/7zip/Compress/LZMA/LZMAEncoder.cpp
#	LZMA_SDK/7zip/Compress/LZ/LZInWindow.cpp
#	LZMA_SDK/7zip/Compress/LZ/LZOutWindow.cpp
lib7z_la_SOURCES = \
	7z.c \
	converter.c \
	filestream.c \
	memorystream.c \
	wrapper/7zip/Compress/LZMA_C/lzma_decode.c \
	wrapper/7zip/Compress/LZMA_C/LzmaDecode.c \
	wrapper/7zip/Compress/PPMD/StdAfx.cpp \
	wrapper/7zip/Compress/PPMD/ppmd_decode.cpp \
	wrapper/7zip/Compress/PPMD/PPMDDecoder.cpp \
	LZMA_SDK/Common/Alloc.cpp \
	LZMA_SDK/Common/CRC.cpp \
	LZMA_SDK/7zip/Common/InBuffer.cpp \
	LZMA_SDK/7zip/Common/OutBuffer.cpp \
	LZMA_SDK/7zip/Compress/RangeCoder/RangeCoderBit.cpp

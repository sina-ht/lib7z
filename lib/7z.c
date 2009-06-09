/*
 * 7z.c
 * (C)Copyright 2004 by Hiroshi Takekawa
 * This file is part of lib7z.
 *
 * Last Modified: Thu Sep 23 04:33:16 2004.
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
#include "converter.h"

#include "LZMA_SDK/Common/Types.h"
#include "wrapper/7zip/Compress/PPMD/ppmd_decode.h"
#include "wrapper/7zip/Compress/LZMA_C/lzma_decode.h"

typedef enum _i7z_id {
  kEnd = 0x00,
  kHeader = 0x01,
  kArchiveProperties = 0x02,
    
  kAdditionalStreamsInfo = 0x03,
  kMainStreamsInfo = 0x04,
  kFilesInfo = 0x05,

  kPackInfo = 0x06,
  kUnPackInfo = 0x07,
  kSubStreamsInfo = 0x08,

  kSize = 0x09,
  kCRC = 0x0a,

  kFolder = 0x0b,

  kCodersUnPackSize = 0x0c,
  kNumUnPackStream = 0x0d,

  kEmptyStream = 0x0e,
  kEmptyFile = 0x0f,
  kAnti = 0x10,

  kName = 0x11,
  kCreationTime = 0x12,
  kLastAccessTime = 0x13,
  kLastWriteTime = 0x14,
  kAttributes = 0x15,
  kComment = 0x16,

  kEncodedHeader = 0x17,
} I7z_ID;

#define stream_read_ch(__st_p) ({			\
      I7z_stream *__st = (__st_p);			\
      unsigned char __c;				\
      if (i7z_stream_read(__st, &__c, 1) != 1) {	\
	err_message_fnc("Unexpected EOF.\n");		\
	return UNEXPECTED_EOF;				\
      }							\
      __c;						\
    })

static UINT64
get_uint64(I7z_stream *st)
{
  unsigned char c;
  int i, mask, extra_bytes;
  UINT64 value;

  if (i7z_stream_read(st, &c, 1) != 1)
    fatal("Unexpected EOF.\n");

  for (i = 0, mask = 0x80; mask > 0; i++, mask >>= 1) {
    if ((c & mask) == 0)
      break;
  }
  value = (i <= 6) ? (c & (mask - 1)) << (8 * i) : 0;
  extra_bytes = i;

  for (i = 0; i < extra_bytes; i++) {
    unsigned int ci;

    if (i7z_stream_read(st, &c, 1) != 1)
      fatal("Unexpected EOF.\n");
    ci = c;
    value += ci << (8 * i);
  }

  return value;
}

static void
free_coder(I7z_coder *coder)
{
  if (!coder)
    return;

  if (coder->ids)
    free(coder->ids);
  if (coder->properties)
    free(coder->properties);
}

static void
free_folder(I7z_folder *f)
{
  unsigned int i;

  if (!f)
    return;
  if (f->unpacksizes)
    free(f->unpacksizes);
  if (f->coders) {
    for (i = 0; i < f->ncoders; i++)
      free_coder(&f->coders[i]);
    free(f->coders);
  }
  if (f->crcs)
    free(f->crcs);
  if (f->unsubstreamsizes)
    free(f->unsubstreamsizes);
}

static void
i7z_pack_free(I7z_pack *pack)
{
  unsigned int i;

  if (!pack)
    return;

  if (pack->packsizes)
    free(pack->packsizes);
  if (pack->folders) {
    for (i = 0; i < pack->nfolders; i++)
      free_folder(&pack->folders[i]);
    free(pack->folders);
  }
  if (pack->subcrcs)
    free(pack->subcrcs);
}

void
i7z_free(I7z *i7z)
{
  if (!i7z)
    return;

  i7z_pack_free(&i7z->additional_pack);
  i7z_pack_free(&i7z->main_pack);
  
  if (i7z->wc_filenames)
    free(i7z->wc_filenames);
  if (i7z->filenames)
    free(i7z->filenames);
  if (i7z->additional_streams) {
    unsigned int i;
    for (i = 0; i < i7z->additional_pack.npackstreams; i++)
      free(i7z->additional_streams[i]);
    free(i7z->additional_streams);
  }
  free(i7z);
}

/*
  Header sample:
  17: EncodedHeader
  06: PackInfo
  C0: UINT64(BYTE y[2]): PackPos = 0x9e4e = 40526
  4E 9E: 0x9e4e = 40526
  01: UINT64(BYTE y[0]): NumPackStreams = 1
  09: Size
  84: UINT64(BYTE y[1]): PackSizes[0] = 0x421 = 1057
  21
  00: End (of PackInfo)
  07: UnPackInfo (Coders Info)
  0B: Folder
  01: UINT64(BYTE y[0]): NumFolders = 1
  00: External = 0
  Folder[0]:
  01: NumCoders = 1
  Coders[0]:
  23: 0b00100011:
  0:3(3): DecompressionMethod.IDSize = 3
  4(0): IsSimple
  5(1): There Are Attributes
  7(0): Last Method in Alternative_Method_List
  03: ID[0] = 3
  01: ID[1] = 1
  01: ID[2] = 1
  05: UINT64(BYTE y[0]): PropertiesSize = 5
  5D 00 00 10 00: Properties
  0C: CodersUnPackSize
  8A: UINT64(BYTE y[1]): UnPackSize[0] = 0xad6 = 2774
  D6
  0A: CRC
  01: Not All Are Defined
  72 C3 7A C1: CRC value
  00: End
  00
*/

static int
parse_archive_properties(I7z *i7z, I7z_stream *st)
{
  debug_message_fn("()\n");
  debug_message_fn("() end\n");
  return 0;
}

/*
  PackInfo
  ~~~~~~~~~~~~
  BYTE NID::kPackInfo  (0x06)
  UINT64 PackPos
  UINT64 NumPackStreams

  []
  BYTE NID::kSize    (0x09)
  UINT64 PackSizes[NumPackStreams]
  []

  []
  BYTE NID::kCRC      (0x0A)
  PackStreamDigests[NumPackStreams]
  []

  BYTE NID::kEnd
*/
static int
parse_packinfo(I7z_pack *pack, I7z_stream *st)
{
  unsigned char c;
  unsigned int i;

  debug_message_fn("()\n");

  pack->packpos = get_uint64(st);
  pack->npackstreams = get_uint64(st);
  if ((pack->packsizes = calloc(pack->npackstreams, sizeof(*pack->packsizes))) == NULL) {
    err_message_fnc("No enough memory.\n");
    return NO_ENOUGH_MEMORY;
  }
#if 0
  if ((pack->packstreamdigests = calloc(pack->npackstreams, sizeof(*pack->packstreamdigests))) == NULL) {
    err_message_fnc("No enough memory.\n");
    return NO_ENOUGH_MEMORY;
  }
#endif

  debug_message_fnc("pos = %lld, npackst = %lld\n", pack->packpos, pack->npackstreams);

  for (;;) {
    c = stream_read_ch(st);
    if (c == kEnd)
      break;
    switch (c) {
    case kSize:
      debug_message("PackInfo Sizes:\n");
      for (i = 0; i < pack->npackstreams; i++) {
	pack->packsizes[i] = get_uint64(st);
	debug_message(" Stream %d: %lld bytes\n", i, pack->packsizes[i]);
      }
      break;
    case kCRC:
      break;
    default:
      err_message_fnc("Unexpected property id %02X\n", c);
      return UNEXPECTED_PROPERTYID;
    }
  }

  debug_message_fn("() end\n");

  return 0;
}

static void *
null_create(I7z_stream *st, unsigned char *properties, unsigned int propertiessize)
{
  err_message_fnc("Decoder not found...\n");
  return NULL;
}

static void
coder_id_to_decoder(I7z_coder *coder, int idsize)
{
  coder->create = null_create;

  debug_message("Coder: ");
  switch(coder->ids[0]) {
  case 0:
    debug_message("Copy"); break;
  case 1:
    debug_message("Reserved"); break;
  case 2:
    debug_message("Common "); break;
  case 3:
    debug_message("7z ");
    switch (coder->ids[1]) {
    case 1:
      debug_message("LZMA version %d", coder->ids[2]);
      coder->create = lzma_decoder_create;
      coder->decode = lzma_decode;
      coder->destroy = lzma_decoder_destroy;
      break;
    case 3:
      debug_message("Branch ");
      switch (coder->ids[2]) {
      case 1:
	debug_message("x86 ");
	switch (coder->ids[3]) {
	case 0x03:
	  debug_message("BCJ"); break;
	case 0x1B:
	  debug_message("BCJ2"); break;
	default:
	  debug_message("UNKNOWN"); break;
	}
	break;
      case 2:
	debug_message("PPC ");
	switch (coder->ids[3]) {
	case 0x05:
	  debug_message("BC_PPC_B (Big Endian)"); break;
	default:
	  debug_message("UNKNOWN"); break;
	}
	break;
      case 3:
	debug_message("Alpha ");
	switch (coder->ids[3]) {
	case 0x01:
	  debug_message("BC_Alpha"); break;
	default:
	  debug_message("UNKNOWN"); break;
	}
	break;
      case 4:
	debug_message("IA64");
	switch (coder->ids[3]) {
	case 0x01:
	  debug_message("BC_IA64"); break;
	default:
	  debug_message("UNKNOWN"); break;
	}
	break;
      case 5:
	debug_message("ARM");
	switch (coder->ids[3]) {
	case 0x01:
	  debug_message("BC_ARM"); break;
	default:
	  debug_message("UNKNOWN"); break;
	}
	break;
      case 6:
	debug_message("M68");
	switch (coder->ids[3]) {
	case 0x05:
	  debug_message("BC_M68_B (Big Endian)"); break;
	default:
	  debug_message("UNKNOWN"); break;
	}
	break;
      case 7:
	debug_message("ARM Thumb");
	switch (coder->ids[3]) {
	case 0x01:
	  debug_message("BC_ARMThumb"); break;
	default:
	  debug_message("UNKNOWN"); break;
	}
	break;
      case 8:
	debug_message("reserved for SPARC"); break;
      default:
	debug_message("UNKNOWN"); break;
      }
      break;
    case 4:
      debug_message("PPMD version %d", coder->ids[2]);
      coder->create = ppmd_decoder_create;
      coder->decode = ppmd_decode;
      coder->destroy = ppmd_decoder_destroy;
      break;
    default:
      debug_message("Unknown"); break;
    }
    break;
  case 4:
    debug_message("Misc "); break;
  case 6:
    debug_message("Crypto "); break;
  case 7:
    debug_message("Hash "); break;
  default:
    debug_message("Unknown"); break;
  }
  debug_message("\n");
}

/*
  Folder
  ~~~~~~
  UINT64 NumCoders;
  for (NumCoders)
  {
  BYTE 
  {
  0:3 DecompressionMethod.IDSize
  4:
  0 - IsSimple
  1 - Is not simple
  5:
  0 - No Attributes
  1 - There Are Attributes
  7:
  0 - Last Method in Alternative_Method_List
  1 - There are more alternative methods
  } 
  BYTE DecompressionMethod.ID[DecompressionMethod.IDSize]
  if (!IsSimple)
  {
  UINT64 NumInStreams;
  UINT64 NumOutStreams;
  }
  if (DecompressionMethod[0] != 0)
  {
  UINT64 PropertiesSize
  BYTE Properties[PropertiesSize]
  }
  }
    
  NumBindPairs = NumOutStreamsTotal - 1;

  for (NumBindPairs)
  {
  UINT64 InIndex;
  UINT64 OutIndex;
  }

  NumPackedStreams = NumInStreamsTotal - NumBindPairs;
  if (NumPackedStreams > 1)
  for(NumPackedStreams)
  {
  UINT64 Index;
  };
*/
static int
parse_folder(I7z_pack *pack, I7z_stream *st, I7z_folder *f)
{
  unsigned int i;
  int j, nin = 0, nout = 0;
  int nbindpairs, npackedstreams;

  debug_message_fn("()\n");

  f->ncoders = get_uint64(st);

  debug_message_fnc("coders = %lld\n", f->ncoders);

  if ((f->coders = calloc(f->ncoders, sizeof(*f->coders))) == NULL) {
    err_message_fnc("No enough memory.\n");
    return NO_ENOUGH_MEMORY;
  }

  for (i = 0; i < f->ncoders; i++) {
    I7z_coder *coder = &f->coders[i];
    unsigned char c;

    c = stream_read_ch(st);
    coder->idsize = c & 0xf;
    debug_message_fnc("ID(%d bytes): ", coder->idsize);
    if ((coder->ids = calloc(coder->idsize, sizeof(*coder->ids))) == NULL) {
      err_message_fnc("No enough memory.\n");
      return NO_ENOUGH_MEMORY;
    }
    for (j = 0; j < coder->idsize; j++) {
      coder->ids[j] = stream_read_ch(st);
      debug_message("%02X ", coder->ids[j]);
    }
    debug_message("\n");

    coder_id_to_decoder(coder, coder->idsize);

    coder->ninstreams = 1;
    coder->noutstreams = 1;
    if (c & 0x10) {
      /* !IsSimple */
      coder->ninstreams = get_uint64(st);
      coder->noutstreams = get_uint64(st);
    }
    nin += coder->ninstreams;
    nout += coder->noutstreams;

    /* Properties? */
    if (c & 0x20) {
      coder->propertiessize = get_uint64(st);
      if ((coder->properties = calloc(coder->propertiessize, sizeof(*coder->properties))) == NULL) {
	err_message_fnc("No enough memory.\n");
	return NO_ENOUGH_MEMORY;
      }
      debug_message_fnc("Properties (%lld bytes): ", coder->propertiessize);
      for (j = 0; j < (int)coder->propertiessize; j++) {
	coder->properties[j] = stream_read_ch(st);
	debug_message("%02X ", coder->properties[j]);
      }
      debug_message("\n");
    } else {
      debug_message_fnc("No Properties\n");
    }
  }

  f->ninstreams = nin;
  f->noutstreams = nout;

  nbindpairs = nout - 1;
  npackedstreams = nin - nbindpairs;

  if (nbindpairs) {
    if ((f->inindexes = calloc(nbindpairs, sizeof(*f->inindexes))) == NULL) {
      err_message_fnc("No enough memory.\n");
      return NO_ENOUGH_MEMORY;
    }
    if ((f->outindexes = calloc(nbindpairs, sizeof(*f->outindexes))) == NULL) {
      err_message_fnc("No enough memory.\n");
      return NO_ENOUGH_MEMORY;
    }
    for (i = 0; i < nbindpairs; i++) {
      f->inindexes[i] = get_uint64(st);
      f->outindexes[i] = get_uint64(st);
      debug_message_fnc("pair#%d: in %d -> %d out\n", i, f->inindexes[i], f->outindexes[i]);
    }
  }
    
  if (npackedstreams > 1) {
    warning_fnc("nout %d, nin %d, nbindpairs %d, npackedstreams %d\n", nout, nin, nbindpairs, npackedstreams);
    fatal("Unimplemented (folder).\n");
  }

  debug_message_fn("() end\n");

  return 0;
}

static int
parse_crc(I7z_stream *st, int noutstreams, UINT32 *crcs)
{
  unsigned char c, buf[4];
  int i;

  c = stream_read_ch(st);
  if (c == 0) {
    c = stream_read_ch(st);
    fatal("Unimplemented (CRC: Not All Defined), next = %02X\n", c);
  }

  for (i = 0; i < noutstreams; i++) {
    if (i7z_stream_read(st, buf, 4) != 4) {
      err_message_fnc("Unexpected EOF.\n");
      return UNEXPECTED_EOF;
    }
    crcs[i] = *((UINT32 *)buf);
  }

  return 0;
}

/*
  Coders Info
  ~~~~~~~~~~~

  BYTE NID::kUnPackInfo  (0x07)


  BYTE NID::kFolder  (0x0B)
  UINT64 NumFolders
  BYTE External
  switch(External)
  {
  case 0:
  Folders[NumFolders]
  case 1:
  UINT64 DataStreamIndex
  }


  BYTE ID::kCodersUnPackSize  (0x0C)
  for(Folders)
  for(Folder.NumOutStreams)
  UINT64 UnPackSize;


  []
  BYTE NID::kCRC   (0x0A)
  UnPackDigests[NumFolders]
  []

  

  BYTE NID::kEnd
*/
static int
parse_folders(I7z_pack *pack, I7z_stream *st)
{
  unsigned int i;
  int res;

  if ((pack->folders = calloc(pack->nfolders, sizeof(*pack->folders))) == NULL) {
    err_message_fnc("No enough memory.\n");
    return NO_ENOUGH_MEMORY;
  }

  for (i = 0; i < pack->nfolders; i++)
    if ((res = parse_folder(pack, st, &pack->folders[i])) < 0)
      return res;

  return 0;
}

static int
parse_unpackinfo(I7z *i7z, I7z_pack *pack, I7z_stream *st)
{
  unsigned char c;
  unsigned int i, j;
  int res;
  UINT64 stream_base;

  debug_message_fn("()\n");

  c = stream_read_ch(st);
  if (c != kFolder) {
    err_message_fnc("Unexpected Property ID: %02X.\n", c);
    return UNEXPECTED_PROPERTYID;
  }

  pack->nfolders = get_uint64(st);
  debug_message_fnc("%lld folder(s)\n", pack->nfolders);

  c = stream_read_ch(st);
  if (c) {
    I7z_stream *external_st = i7z_stream_create();
    UINT64 external_index = get_uint64(st);

    debug_message_fnc("Use External: index = %llu\n", external_index);
    i7z_stream_make_memorystream(external_st,
				 i7z->additional_streams[external_index],
				 i7z->additional_pack.folders[external_index].unpacksizes[0]);
    if ((res = parse_folders(pack, external_st)) < 0)
      return res;
    i7z_stream_close(external_st);
    i7z_stream_destroy(external_st);
  } else {
    if ((res = parse_folders(pack, st)) < 0)
      return res;
  }

  c = stream_read_ch(st);
  if (c != kCodersUnPackSize) {
    err_message_fnc("Unexpected Property ID: %02X.\n", c);
    return UNEXPECTED_PROPERTYID;
  }

  stream_base = 32 + pack->packpos;
  for (i = 0; i < pack->nfolders; i++) {
    I7z_folder *f = &pack->folders[i];

    debug_message_fnc("Folder %d:\n", i);
    if ((f->unpacksizes = calloc(f->noutstreams, sizeof(*f->unpacksizes))) == NULL) {
      err_message_fnc("No enough memory.\n");
      return NO_ENOUGH_MEMORY;
    }
    for (j = 0; j < f->noutstreams; j++) {
      f->unpacksizes[j] = get_uint64(st);
      debug_message_fnc(" Stream %d: %llu bytes\n", j, f->unpacksizes[j]);
    }
    f->stream_base = stream_base;
    stream_base += pack->packsizes[i];
  }

  for (;;) {
    c = stream_read_ch(st);
    if (c == kEnd)
      break;
    switch (c) {
    case kCRC:
      for (i = 0; i < pack->nfolders; i++) {
	I7z_folder *f = &pack->folders[i];

	if ((f->crcs = calloc(f->noutstreams, sizeof(*f->crcs))) == NULL) {
	  err_message_fnc("No enough memory.\n");
	  return NO_ENOUGH_MEMORY;
	}
	if ((res = parse_crc(st, f->noutstreams, f->crcs)) < 0)
	  return res;
	debug_message_fnc("Got %d CRC\n", f->noutstreams);
      }
      break;
    case kMainStreamsInfo:
      /* Buggy ? */
      return c;
    default:
      err_message_fnc("Unexpected property id %02X\n", c);
      return UNEXPECTED_PROPERTYID;
    }
  }

  debug_message_fn("() end\n");

  return 0;
}

/*
  SubStreams Info
  ~~~~~~~~~~~~~~
  BYTE NID::kSubStreamsInfo; (0x08)

  []
  BYTE NID::kNumUnPackStream; (0x0D)
  UINT64 NumUnPackStreamsInFolders[NumFolders];
  []


  []
  BYTE NID::kSize  (0x09)
  UINT64 UnPackSizes[]
  []


  []
  BYTE NID::kCRC  (0x0A)
  Digests[Number of streams with unknown CRC]
  []

  
  BYTE NID::kEnd
*/
static int
parse_substreaminfo(I7z_pack *pack, I7z_stream *st)
{
  unsigned char c;
  unsigned int i, j;
  int res;
  UINT64 idx_base;

  debug_message_fn("()\n");

  /* Set default for substream */
  for (i = 0; i < pack->nfolders; i++)
    pack->folders[i].nunsubstreams = 1;

  for (;;) {
    c = stream_read_ch(st);
    if (c == kEnd)
      break;
    debug_message_fn("(%02X)\n", c);
    switch (c) {
    case kNumUnPackStream:
      debug_message_fnc("SubStreamInfo NumUnPackStream:\n");
      idx_base = 0;
      for (i = 0; i < pack->nfolders; i++) {
	I7z_folder *f = &pack->folders[i];

	f->nunsubstreams = get_uint64(st);
	debug_message_fnc("Folder %d: %lld unsubstream(s)\n", i, f->nunsubstreams);
	if ((f->unsubstreamsizes = calloc(f->nunsubstreams, sizeof(*f->unsubstreamsizes))) == NULL) {
	  err_message_fnc("No enough memory.\n");
	  return NO_ENOUGH_MEMORY;
	}
	f->idx_base = idx_base;
	idx_base += f->nunsubstreams;
      }
      break;
    case kSize:
      debug_message("SubStreamInfo Sizes:\n");
      for (i = 0; i < pack->nfolders; i++) {
	I7z_folder *f = &pack->folders[i];
	UINT64 total = 0;

	debug_message(" Folder %d:\n", i);
	/* The last unsubpacksize is obvious */
	for (j = 0; j < f->nunsubstreams - 1; j++) {
	  f->unsubstreamsizes[j] = get_uint64(st);
	  total += f->unsubstreamsizes[j];
	  debug_message("  SubStream %d: %llu bytes\n", j, f->unsubstreamsizes[j]);
	}
	f->unsubstreamsizes[j] = f->unpacksizes[0] - total;
	debug_message("  SubStream %d: %llu bytes (%llu, %llu)\n", j, f->unsubstreamsizes[j], f->unpacksizes[0], total);
      }
      break;
    case kCRC:
      debug_message("SubStreamInfo CRC\n");
      {
	UINT64 nunsubstreams = 0;
	for (i = 0; i < pack->nfolders; i++) {
	  I7z_folder *f = &pack->folders[i];

	  nunsubstreams += f->nunsubstreams;
	}
	if ((pack->subcrcs = calloc(nunsubstreams, sizeof(*pack->subcrcs))) == NULL) {
	  err_message_fnc("No enough memory.\n");
	  return NO_ENOUGH_MEMORY;
	}
	if ((res = parse_crc(st, nunsubstreams, pack->subcrcs)) < 0)
	  return res;
	debug_message_fnc("Got %lld subcrcs\n", nunsubstreams);
      }	
      break;
    default:
      err_message_fnc("Unexpected property id %02X\n", c);
      return UNEXPECTED_PROPERTYID;
    }
  }

  /* Set default for sizes */
  if (pack->folders[0].unsubstreamsizes == NULL) {
    idx_base = 0;
    for (i = 0; i < pack->nfolders; i++) {
      I7z_folder *f = &pack->folders[i];

      debug_message_fnc("Folder %d: %lld unsubstream(s)\n", i, f->nunsubstreams);
      if ((f->unsubstreamsizes = calloc(f->nunsubstreams, sizeof(*f->unsubstreamsizes))) == NULL) {
	err_message_fnc("No enough memory.\n");
	return NO_ENOUGH_MEMORY;
      }
      f->idx_base = idx_base;
      idx_base += f->nunsubstreams;
      f->unsubstreamsizes[0] = f->unpacksizes[0];
      debug_message(" SubStream 0: %llu bytes\n", f->unsubstreamsizes[0]);
    }
  }

  debug_message_fn("() end\n");

  return 0;
}

/*
Streams Info
~~~~~~~~~~~~

  []
  PackInfo
  []


  []
  CodersInfo
  []


  []
  SubStreamsInfo
  []

  BYTE NID::kEnd
*/
static int
parse_streams_info(I7z *i7z, I7z_pack *pack, I7z_stream *st)
{
  unsigned char c;
  int res;

  debug_message_fn("()\n");

  for (;;) {
    c = stream_read_ch(st);
    if (c == kEnd)
      break;

    switch (c) {
    case kPackInfo:
      if ((res = parse_packinfo(pack, st)) < 0)
	return res;
      break;
    case kUnPackInfo:
      if ((res = parse_unpackinfo(i7z, pack, st)) < 0)
	return res;
      if (res == kMainStreamsInfo)
	return res;
      break;
    case kSubStreamsInfo:
      if ((res = parse_substreaminfo(pack, st)) < 0)
	return res;
      break;
    case kFilesInfo:
      /* buggy archive? */
      return kFilesInfo;
    case kMainStreamsInfo:
      /* buggy archive? */
      return kMainStreamsInfo;
    default:
      err_message_fnc("Unexpected property id %02X\n", c);
      return UNEXPECTED_PROPERTYID;
    }
  }

  debug_message_fn("() end\n");

  return 0;
}

static int
parse_files_info_name(I7z *i7z, I7z_stream *st)
{
  unsigned char c;
  unsigned int i;
  int res;

  if ((i7z->wc_filenames = calloc(i7z->nfiles, sizeof(*i7z->wc_filenames))) == NULL) {
    err_message_fnc("No enough memory.\n");
    res = NO_ENOUGH_MEMORY;
    goto out;
  }
  if ((i7z->filenames = calloc(i7z->nfiles, sizeof(*i7z->filenames))) == NULL) {
    err_message_fnc("No enough memory.\n");
    res = NO_ENOUGH_MEMORY;
    goto out;
  }
  for (i = 0; i < i7z->nfiles; i++) {
    unsigned short int wc;
    unsigned short int *wcs;
    int j, size = 64;

    if ((wcs = calloc(size, sizeof(*wcs))) == NULL) {
      err_message_fnc("No enough memory.\n");
      res = NO_ENOUGH_MEMORY;
      goto out;
    }
    j = 0;
    for (;;) {
      if (i7z_stream_read(st, (char *)&wc, sizeof(wc)) != sizeof(wc)) {
	free(wcs);
	err_message_fnc("Unexpected EOF.\n");
	res = UNEXPECTED_EOF;
	goto out;
      }
      wcs[j] = wc;
      if (wc == 0) {
	i7z->wc_filenames[i] = wcs;
	if ((res = converter_convert((char *)wcs, &i7z->filenames[i], j * sizeof(*wcs), "UNICODELITTLE", "EUC-JP")) == 0) {
	  err_message_fnc(" converter_convert returned %d\n", res);
	}
	debug_message(" File #%d: %s\n", i, i7z->filenames[i]);
	break;
      }
      j++;
      if (j >= size) {
	unsigned short int *tmp;

	size += 64;
	if ((tmp = realloc(wcs, size * sizeof(*wcs))) == NULL) {
	  free(wcs);
	  err_message_fnc("No enough memory.\n");
	  res = UNEXPECTED_EOF;
	  goto out;
	}
	wcs = tmp;
      }
    }
  }

  return 0;

 out:
  if (i7z->wc_filenames) {
    for (i = 0; i < i7z->nfiles; i++) {
      if (i7z->wc_filenames[i])
	free(i7z->wc_filenames[i]);
    }
    free(i7z->wc_filenames);
    i7z->wc_filenames = NULL;
  }
  if (i7z->filenames) {
    for (i = 0; i < i7z->nfiles; i++) {
      if (i7z->filenames[i])
	free(i7z->filenames[i]);
    }
    free(i7z->filenames);
    i7z->filenames = NULL;
  }

  return res;
}

/*
  FilesInfo
  ~~~~~~~~~
  BYTE NID::kFilesInfo;  (0x05)
  UINT64 NumFiles

  while(true)
  {
  BYTE PropertyType;
  if (aType == 0)
  break;

  UINT64 Size;

  switch(PropertyType)
  {
  kEmptyStream:   (0x0E)
  for(NumFiles)
  BIT IsEmptyStream

  kEmptyFile:     (0x0F)
  for(EmptyStreams)
  BIT IsEmptyFile

  kAnti:          (0x10)
  for(EmptyStreams)
  BIT IsAntiFile
      
  case kCreationTime:   (0x12)
  case kLastAccessTime: (0x13)
  case kLastWriteTime:  (0x14)
  BYTE AllAreDefined
  if (AllAreDefined == 0)
  {
  for(NumFiles)
  BIT TimeDefined
  }
  BYTE External;
  if(External != 0)
  UINT64 DataIndex
  []
  for(Definded Items)
  UINT32 Time
  []
      
  kName     (0x11)
  BYTE External;
  if(External != 0)
  UINT64 DataIndex
  []
  for(Files)
  {
  wchar_t Names[NameSize];
  wchar_t 0;
  }
  []

  kAttributes:  (0x15)
  BYTE AllAreDefined
  if (AllAreDefined == 0)
  {
  for(NumFiles)
  BIT AttributesAreDefined
  }
  BYTE External;
  if(External != 0)
  UINT64 DataIndex
  []
  for(Definded Attributes)
  UINT32 Attributes
  []
  }
  }
*/
static int
parse_files_info(I7z *i7z, I7z_stream *st)
{
  I7z_pack *pack = &i7z->main_pack;
  unsigned char c;
  UINT64 external_index, size;
  unsigned int i, j, k;
  int l, t, res;

  debug_message_fn("()\n");

  i7z->nfiles = get_uint64(st);
  debug_message_fnc("%lld file(s)\n", i7z->nfiles);

  if ((i7z->file_to_folder = calloc(i7z->nfiles, sizeof(*i7z->file_to_folder))) == NULL) {
    err_message_fnc("No enough memory.\n");
    return NO_ENOUGH_MEMORY;
  }

  k = 0;
  for (i = 0; i < pack->nfolders; i++) {
    for (j = 0; j < pack->folders[i].nunsubstreams; j++)
      i7z->file_to_folder[k++] = i;
  }

  if ((i7z->file_to_substream = calloc(i7z->nfiles, sizeof(*i7z->file_to_substream))) == NULL) {
    err_message_fnc("No enough memory.\n");
    return NO_ENOUGH_MEMORY;
  }
  for (i = 0; i < i7z->nfiles; i++)
    i7z->file_to_substream[i] = i;

  for (;;) {
    c = stream_read_ch(st);
    if (c == 0)
      break;
    size = get_uint64(st);
    switch (c) {
    case kEmptyStream:
      debug_message_fnc("Got kEmptyStream(0x%02X), Size = %lld\n", c, size);
      j = k = l = t = 0;
      pack->folders[0].idx_base = -1;
      for (i = 0; i < i7z->nfiles; i++) {
	if (j == 0) {
	  j = 8;
	  c = stream_read_ch(st);
	}
	if (c & 0x80) {
	  i7z->file_to_substream[i] = (UINT64)-1;
	  k++;
	  t++;
	  debug_message_fnc(" Substream #%d is EMPTY.\n", i);
	} else {
	  i7z->file_to_substream[i] = k;
	  i7z->file_to_folder[i] = l;
	  if (pack->folders[l].idx_base == -1) {
	    debug_message_fnc(" Folder#%d starts from Substream #%d.\n", l, k);
	    pack->folders[l].idx_base = k;
	  }
	  k++;
	  if (k >= t + pack->folders[l].nunsubstreams) {
	    t += pack->folders[l].nunsubstreams;
	    l++;
	    if (l < pack->nfolders)
	      pack->folders[l].idx_base = -1;
	  }
	}
	c <<= 1;
	j--;
      }
      break;
    case kEmptyFile:
      err_message_fnc("Got kEmptyFile(0x%02X), Size = %lld, Not implemented\n", c, size);
      // for(EmptyStreams)
      //   BIT IsEmptyFile
      break;
    case kAnti:
      err_message_fnc("Got kAnti(0x%02X), Size = %lld, Not implemented\n", c, size);
      // for(EmptyStreams)
      //   BIT IsAntiFile
      break;
    case kCreationTime:
      debug_message_fnc("Got kCreationTime(0x%02X), Size = %lld\n", c, size);
      goto process_time;
    case kLastAccessTime:
      debug_message_fnc("Got kLastAccessTime(0x%02X), Size = %lld\n", c, size);
      goto process_time;
    case kLastWriteTime:
      debug_message_fnc("Got kLastWriteTime(0x%02X), Size = %lld\n", c, size);
    process_time:
      c = stream_read_ch(st);
      if (c == 0) {
	/* NOT AllAreDefined */
	// if (AllAreDefined == 0)
	// {
	//   for(NumFiles)
	//     BIT TimeDefined
	// }
	err_message_fnc("Not implemented: AllAreDefined == 0\n");
      }
      c = stream_read_ch(st);
      {
	I7z_stream *time_st = st;

	if (c != 0) {
	  /* External */
	  external_index = get_uint64(st);
	  time_st = i7z_stream_create();
	  i7z_stream_make_memorystream(time_st, i7z->additional_streams[external_index], i7z->additional_pack.folders[external_index].unpacksizes[0]);
	}
	for (i = 0; i < i7z->nfiles; i++) {
	  UINT64 Time;
	  if (i7z_stream_read(time_st, (void *)&Time, sizeof(Time)) != sizeof(Time)) {
	    err_message_fnc("Unexpected EOF.\n");
	    return UNEXPECTED_EOF;
	  }
	  debug_message_fnc(" File #%d: %llu\n", i, Time);
	}
	if (time_st != st) {
	  i7z_stream_close(time_st);
	  i7z_stream_destroy(time_st);
	}
      }
      break;
    case kName:
      debug_message_fnc("Got kName(0x%02X), Size = %lld\n", c, size);
      c = stream_read_ch(st);
      if (c != 0) {
	I7z_stream *external_st = i7z_stream_create();

	external_index = get_uint64(st);
	i7z_stream_make_memorystream(external_st, i7z->additional_streams[external_index], i7z->additional_pack.folders[external_index].unpacksizes[0]);
	debug_message_fnc("Use External: index = %llu\n", external_index);
	if ((res = parse_files_info_name(i7z, external_st)) != 0)
	  return res;
	i7z_stream_close(external_st);
	i7z_stream_destroy(external_st);
      } else {
	if ((res = parse_files_info_name(i7z, st)) != 0)
	  return res;
      }
      break;
    case kAttributes:
      debug_message_fnc("Got kAttributes(0x%02X), Size = %lld\n", c, size);
      c = stream_read_ch(st);
      if (c == 0) {
	// if (AllAreDefined == 0)
	// {
	//   for(NumFiles)
	//     BIT AttributesAreDefined
	// }
	err_message_fnc("Not implemented: AllAreDefined == 0\n");
      }
      c = stream_read_ch(st);
      {
	I7z_stream *attr_st = st;

	if (c != 0) {
	  /* External */
	  external_index = get_uint64(st);
	  attr_st = i7z_stream_create();
	  i7z_stream_make_memorystream(attr_st, i7z->additional_streams[external_index], i7z->additional_pack.folders[external_index].unpacksizes[0]);
	}
	for (i = 0; i < i7z->nfiles; i++) {
	  UINT32 Attributes;
	  if (i7z_stream_read(attr_st, (void *)&Attributes, sizeof(Attributes)) != sizeof(Attributes)) {
	    err_message_fnc("Unexpected EOF.\n");
	    return UNEXPECTED_EOF;
	  }
	  debug_message_fnc(" File #%d: %u\n", i, Attributes);
	}
	if (attr_st != st) {
	  i7z_stream_close(attr_st);
	  i7z_stream_destroy(attr_st);
	}
      }
      break;
    default:
      debug_message_fnc("Got UNKNOWN Property 0x%02X, Size = %lld\n", c, size);
      break;
    }
  }

  debug_message_fn("() end\n");
  return 0;
}

static int
decode_streams(I7z_pack *pack, I7z_stream *st, unsigned int *n_r, unsigned char ***outbufs_r)
{
  unsigned char **outbufs;
  unsigned char *outbuf;
  unsigned int i;
  int res;
  UINT64 insize, outsize;
  void *dec;

  if ((outbufs = calloc(pack->npackstreams, sizeof(*outbufs))) == NULL) 
    return NO_ENOUGH_MEMORY;

  /* XXX: Assume stream = folder for Additional Streams */
  for (i = 0; i < pack->npackstreams; i++) {
    I7z_coder *coder = &pack->folders[i].coders[0];

    debug_message_fnc("seek to %lld\n", pack->folders[i].stream_base);
    i7z_stream_seek(st, pack->folders[i].stream_base, _I7z_SET);
    if ((dec = coder->create(st, coder->properties, coder->propertiessize)) == NULL)
      err_message_fnc("create() failed\n");
    debug_message_fnc("create() OK\n");
    insize = pack->packsizes[i];
    outsize = pack->folders[i].unpacksizes[0];
    outbuf = outbufs[i] = malloc(outsize);
    debug_message_fnc("outsize = %lld\n", outsize);
    if ((res = coder->decode(dec, outbuf, &outsize)) < 0)
      return res;
    coder->destroy(dec);
  }

  *n_r = pack->npackstreams;
  *outbufs_r = outbufs;

  return 0;
}

/*
  Header
  ~~~~~~
  BYTE NID::kHeader (0x01)

  []
  ArchiveProperties
  []

  []
  BYTE NID::kAdditionalStreamsInfo; (0x03)
  StreamsInfo
  []

  []
  BYTE NID::kMainStreamsInfo;    (0x04)
  StreamsInfo
  []

  []
  FilesInfo
  []

  BYTE NID::kEnd
*/
static int
parse_header(I7z *i7z, I7z_stream *st)
{
  unsigned char c;
  unsigned int n;
  int res = 0;

  debug_message_fn("()\n");

  for (;;) {
    if (res < 0)
      return res;
    if (res == 0) {
      c = stream_read_ch(st);
      if (c == kEnd)
	break;
    } else {
      c = res;
    }

    switch (c) {
    case kArchiveProperties:
      res = parse_archive_properties(i7z, st);
      break;
    case kAdditionalStreamsInfo:
      debug_message_fnc("Additional Streams Info:\n");
      res = parse_streams_info(i7z, &i7z->additional_pack, st);
      {
	unsigned int pos = i7z_stream_tell(st);
	int result;

	debug_message_fnc("Decoding Additional Streams\n");
	if ((result = decode_streams(&i7z->additional_pack, st, &n, &i7z->additional_streams)) < 0)
	  return result;
	debug_message_fnc("Decoded Additional %d Streams\n", n);
	debug_message_fnc("Additional Streams Info End\n");
	i7z_stream_seek(st, pos, _I7z_SET);
      }
      if (res != kMainStreamsInfo)
	break;
      /* Buggy?: Fall through */
    case kMainStreamsInfo:
      debug_message_fnc("Main Streams Info:\n");
      res = parse_streams_info(i7z, &i7z->main_pack, st);
      debug_message_fnc("Main Streams Info End\n");
      break;
    case kFilesInfo:
      res = parse_files_info(i7z, st);
      break;
    default:
      err_message_fnc("Unexpected property id %02X at %ld\n", c, i7z_stream_tell(st));
      return UNEXPECTED_PROPERTYID;
    }
  }

  debug_message_fn("() end\n");

  return 0;
}

int
i7z_parse_header(I7z **i7z_p, I7z_stream *st)
{
  I7z *i7z = *i7z_p;
  unsigned char c;
  int res = 0;

  debug_message_fn("()\n");

  c = stream_read_ch(st);
  switch (c) {
  case kEnd:
    break;
  case kHeader:
    if ((res = parse_header(i7z, st)) < 0)
      return res;
    break;
  case kEncodedHeader:
    if ((res = parse_streams_info(i7z, &i7z->main_pack, st)) < 0)
      return res;
    {
      unsigned int n;
      unsigned char **outbufs;
      I7z_stream *decoded_st;

      debug_message_fnc("Decoding encoded header\n");
      if ((res = decode_streams(&i7z->main_pack, st, &n, &outbufs)) < 0)
	return res;
      if ((decoded_st = i7z_stream_create()) == NULL)
	return NO_ENOUGH_MEMORY;
      i7z_stream_make_memorystream(decoded_st, outbufs[0], i7z->main_pack.folders[0].unpacksizes[0]);
      free(outbufs);
      debug_message_fnc("Parse decoded header\n");
      i7z_free(i7z);
      if ((i7z = i7z_create()) == NULL)
	return NO_ENOUGH_MEMORY;
      *i7z_p = i7z;
      res = i7z_parse_header(&i7z, decoded_st);
      i7z_stream_close(decoded_st);
      i7z_stream_destroy(decoded_st);
    }
    break;
  default:
    err_message_fnc("Unexpected property id %02X\n", c);
    return UNEXPECTED_PROPERTYID;
  }

  debug_message_fn("() end: i7z = %p\n", i7z);

  return res;
}

int
i7z_identify(I7z *i7z, I7z_stream *st)
{
  char buf[32];
  static unsigned char id[] = { '7', 'z', 0xbc, 0xaf, 0x27, 0x1c };
  UINT64 offset;
  UINT64 size;
  UINT32 crc;

  if (i7z_stream_read(st, buf, 32) != 32)
    return UNEXPECTED_EOF;
  if (memcmp(buf, id, 6) != 0)
    return NOT_7Z_FORMAT;

  i7z->major_version = buf[6];
  i7z->minor_version = buf[7];

  debug_message_fnc("Identified 7z format version %d.%d.\n", i7z->major_version, i7z->minor_version);
  /* buf[8]-buf[11]: StartHeaderCRC */

  offset = *((UINT64 *)(buf + 12));
  size = *((UINT64 *)(buf + 20));
  crc = *((UINT32 *)(buf + 28));
  debug_message_fnc("Next header: offset %lld, size %lld, crc %08X\n", offset, size, crc);

  i7z_stream_seek(st, offset, _I7z_CUR);
  return 0;
}

I7z_stream *
i7z_stream_create(void)
{
  return calloc(1, sizeof(I7z_stream));
}

void
i7z_stream_destroy(I7z_stream *st)
{
  if (st->destroy_func)
    st->destroy_func(st->obj);
  free(st);
}

I7z *
i7z_create(void)
{
  return calloc(1, sizeof(I7z));
}

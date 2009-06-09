/*
 * lzma_decode.h
 * (C)Copyright 2004 by Hiroshi Takekawa
 * This file is part of lib7z.
 *
 * Last Modified: Sun Sep  5 14:41:49 2004.
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

#if !defined(_LZMA_DECODE_H_)
#define _LZMA_DECODE_H_

#include "7z.h"

void *lzma_decoder_create(I7z_stream *st, unsigned char *prop, unsigned int propsize);
void lzma_decoder_destroy(void *_dec);
int lzma_decode(void *_dec, unsigned char *output, UINT64 *outsize);

#endif

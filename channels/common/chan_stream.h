/* -*- c-basic-offset: 8 -*-
   FreeRDP: A Remote Desktop Protocol client.

   Copyright (C) Marc-Andre Moreau <marcandre.moreau@gmail.com> 2010

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __CHAN_STREAM_H
#define __CHAN_STREAM_H

#define GET_UINT8(_p1, _offset) *(((uint8 *) _p1) + _offset)
#define GET_UINT16(_p1, _offset) ( \
	(uint16) (*(((uint8 *) _p1) + _offset)) + \
	((uint16) (*(((uint8 *) _p1) + _offset + 1)) << 8))
#define GET_UINT32(_p1, _offset) ( \
	(uint32) (*(((uint8 *) _p1) + _offset)) + \
	((uint32) (*(((uint8 *) _p1) + _offset + 1)) << 8) + \
	((uint32) (*(((uint8 *) _p1) + _offset + 2)) << 16) + \
	((uint32) (*(((uint8 *) _p1) + _offset + 3)) << 24))
#define GET_UINT64(_p1, _offset) ( \
	(uint64) (*(((uint8 *) _p1) + _offset)) + \
	((uint64) (*(((uint8 *) _p1) + _offset + 1)) << 8) + \
	((uint64) (*(((uint8 *) _p1) + _offset + 2)) << 16) + \
	((uint64) (*(((uint8 *) _p1) + _offset + 3)) << 24) + \
	((uint64) (*(((uint8 *) _p1) + _offset + 4)) << 32) + \
	((uint64) (*(((uint8 *) _p1) + _offset + 5)) << 40) + \
	((uint64) (*(((uint8 *) _p1) + _offset + 6)) << 48) + \
	((uint64) (*(((uint8 *) _p1) + _offset + 7)) << 56))

#define SET_UINT8(_p1, _offset, _value) *(((uint8 *) _p1) + _offset) = (uint8) (_value)
#define SET_UINT16(_p1, _offset, _value) \
	*(((uint8 *) _p1) + _offset) = (uint8) (((uint16) (_value)) & 0xff), \
	*(((uint8 *) _p1) + _offset + 1) = (uint8) ((((uint16) (_value)) >> 8) & 0xff)
#define SET_UINT32(_p1, _offset, _value) \
	*(((uint8 *) _p1) + _offset) = (uint8) (((uint32) (_value)) & 0xff), \
	*(((uint8 *) _p1) + _offset + 1) = (uint8) ((((uint32) (_value)) >> 8) & 0xff), \
	*(((uint8 *) _p1) + _offset + 2) = (uint8) ((((uint32) (_value)) >> 16) & 0xff), \
	*(((uint8 *) _p1) + _offset + 3) = (uint8) ((((uint32) (_value)) >> 24) & 0xff)
#define SET_UINT64(_p1, _offset, _value) \
	*(((uint8 *) _p1) + _offset) = (uint8) (((uint64) (_value)) & 0xff), \
	*(((uint8 *) _p1) + _offset + 1) = (uint8) ((((uint64) (_value)) >> 8) & 0xff), \
	*(((uint8 *) _p1) + _offset + 2) = (uint8) ((((uint64) (_value)) >> 16) & 0xff), \
	*(((uint8 *) _p1) + _offset + 3) = (uint8) ((((uint64) (_value)) >> 24) & 0xff), \
	*(((uint8 *) _p1) + _offset + 4) = (uint8) ((((uint64) (_value)) >> 32) & 0xff), \
	*(((uint8 *) _p1) + _offset + 5) = (uint8) ((((uint64) (_value)) >> 40) & 0xff), \
	*(((uint8 *) _p1) + _offset + 6) = (uint8) ((((uint64) (_value)) >> 48) & 0xff), \
	*(((uint8 *) _p1) + _offset + 7) = (uint8) ((((uint64) (_value)) >> 56) & 0xff)

int
freerdp_set_wstr(char* dst, int dstlen, char* src, int srclen);
int
freerdp_get_wstr(char* dst, int dstlen, char* src, int srclen);

#endif

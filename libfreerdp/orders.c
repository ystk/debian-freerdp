/* -*- c-basic-offset: 8 -*-
   rdesktop: A Remote Desktop Protocol client.
   RDP order processing
   Copyright (C) Matthew Chapman 1999-2008

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

#include "frdp.h"
#include "orderstypes.h"
#include "orders.h"
#include "rdp.h"
#include "pstcache.h"
#include "cache.h"
#include "bitmap.h"
#include "rdpset.h"
#include "mem.h"
#include "debug.h"

/* Read field indicating which parameters are present */
static void
rdp_in_present(STREAM s, uint32 * present, uint8 flags, int size)
{
	uint8 bits;
	int i;

	if (flags & RDP_ORDER_SMALL)
	{
		size--;
	}

	if (flags & RDP_ORDER_TINY)
	{
		if (size < 2)
			size = 0;
		else
			size -= 2;
	}

	*present = 0;
	for (i = 0; i < size; i++)
	{
		in_uint8(s, bits);
		*present |= bits << (i * 8);
	}
}

/* Read a co-ordinate (16-bit, or 8-bit delta) */
static void
rdp_in_coord(STREAM s, sint16 * coord, RD_BOOL delta)
{
	sint8 change;

	if (delta)
	{
		in_uint8(s, change);
		*coord += change;
	}
	else
	{
		in_uint16_le(s, *coord);
	}
}

/* Parse a delta co-ordinate in polyline/polygon order form */
static int
parse_delta(uint8 * buffer, int *offset)
{
	int value = buffer[(*offset)++];
	int two_byte = value & 0x80;

	if (value & 0x40)	/* sign bit */
		value |= ~0x3f;
	else
		value &= 0x3f;

	if (two_byte)
		value = (value << 8) | buffer[(*offset)++];

	return value;
}

/* Read a colour entry */
static void
rdp_in_colour(STREAM s, uint32 * colour)
{
	uint32 i;
	in_uint8(s, i);
	*colour = i;
	in_uint8(s, i);
	*colour |= i << 8;
	in_uint8(s, i);
	*colour |= i << 16;
}

/* Parse bounds information */
static RD_BOOL
rdp_parse_bounds(STREAM s, BOUNDS * bounds)
{
	uint8 present;

	in_uint8(s, present);

	if (present & 1)
		rdp_in_coord(s, &bounds->left, False);
	else if (present & 16)
		rdp_in_coord(s, &bounds->left, True);

	if (present & 2)
		rdp_in_coord(s, &bounds->top, False);
	else if (present & 32)
		rdp_in_coord(s, &bounds->top, True);

	if (present & 4)
		rdp_in_coord(s, &bounds->right, False);
	else if (present & 64)
		rdp_in_coord(s, &bounds->right, True);

	if (present & 8)
		rdp_in_coord(s, &bounds->bottom, False);
	else if (present & 128)
		rdp_in_coord(s, &bounds->bottom, True);

	return s_check(s);
}

/* Parse a pen */
static RD_BOOL
rdp_parse_pen(STREAM s, RD_PEN * pen, uint32 present)
{
	if (present & 1)
		in_uint8(s, pen->style);

	if (present & 2)
		in_uint8(s, pen->width);

	if (present & 4)
		rdp_in_colour(s, &pen->colour);

	return s_check(s);
}

static void
setup_brush(rdpOrders * orders, RD_BRUSH * out_brush, RD_BRUSH * in_brush)
{
	RD_BRUSHDATA *brush_data;
	uint8 cache_idx;
	uint8 colour_code;

	memcpy(out_brush, in_brush, sizeof(RD_BRUSH));
	if (out_brush->style & 0x80)
	{
		colour_code = out_brush->style & 0x0f;
		cache_idx = out_brush->pattern[0];
		brush_data = cache_get_brush_data(orders->rdp->cache, colour_code, cache_idx);
		if ((brush_data == NULL) || (brush_data->data == NULL))
		{
			ui_error(orders->rdp->inst, "error getting brush data, style %x\n",
				 out_brush->style);
			out_brush->bd = NULL;
			memset(out_brush->pattern, 0, 8);
		}
		else
		{
			out_brush->bd = brush_data;
		}
		out_brush->style = 3;
	}
}

/* Parse a brush */
static RD_BOOL
rdp_parse_brush(STREAM s, RD_BRUSH * brush, uint32 present)
{
	if (present & 1)
		in_uint8(s, brush->xorigin);

	if (present & 2)
		in_uint8(s, brush->yorigin);

	if (present & 4)
		in_uint8(s, brush->style);

	if (present & 8)
		in_uint8(s, brush->pattern[0]);

	if (present & 16)
		in_uint8a(s, &brush->pattern[1], 7);

	return s_check(s);
}

/* Process a destination blt order */
static void
process_destblt(rdpOrders * orders, STREAM s, DESTBLT_ORDER * os, uint32 present, RD_BOOL delta)
{
	if (present & 0x01)
		rdp_in_coord(s, &os->x, delta);

	if (present & 0x02)
		rdp_in_coord(s, &os->y, delta);

	if (present & 0x04)
		rdp_in_coord(s, &os->cx, delta);

	if (present & 0x08)
		rdp_in_coord(s, &os->cy, delta);

	if (present & 0x10)
		in_uint8(s, os->opcode);

	DEBUG("DESTBLT(op=0x%x,x=%d,y=%d,cx=%d,cy=%d)\n",
	      os->opcode, os->x, os->y, os->cx, os->cy);

	ui_destblt(orders->rdp->inst, os->opcode, os->x, os->y, os->cx, os->cy);
}

/* Process a pattern blt order */
static void
process_patblt(rdpOrders * orders, STREAM s, PATBLT_ORDER * os, uint32 present, RD_BOOL delta)
{
	RD_BRUSH brush;

	if (present & 0x0001)
		rdp_in_coord(s, &os->x, delta);

	if (present & 0x0002)
		rdp_in_coord(s, &os->y, delta);

	if (present & 0x0004)
		rdp_in_coord(s, &os->cx, delta);

	if (present & 0x0008)
		rdp_in_coord(s, &os->cy, delta);

	if (present & 0x0010)
		in_uint8(s, os->opcode);

	if (present & 0x0020)
		rdp_in_colour(s, &os->bgcolour);

	if (present & 0x0040)
		rdp_in_colour(s, &os->fgcolour);

	rdp_parse_brush(s, &os->brush, present >> 7);

	DEBUG("PATBLT(op=0x%x,x=%d,y=%d,cx=%d,cy=%d,bs=%d,bg=0x%x,fg=0x%x)\n", os->opcode, os->x,
	       os->y, os->cx, os->cy, os->brush.style, os->bgcolour, os->fgcolour);

	setup_brush(orders, &brush, &os->brush);

	ui_patblt(orders->rdp->inst, os->opcode, os->x, os->y, os->cx, os->cy,
		  &brush, os->bgcolour, os->fgcolour);
}

/* Process a screen blt order */
static void
process_screenblt(rdpOrders * orders, STREAM s, SCREENBLT_ORDER * os, uint32 present, RD_BOOL delta)
{
	if (present & 0x0001)
		rdp_in_coord(s, &os->x, delta);

	if (present & 0x0002)
		rdp_in_coord(s, &os->y, delta);

	if (present & 0x0004)
		rdp_in_coord(s, &os->cx, delta);

	if (present & 0x0008)
		rdp_in_coord(s, &os->cy, delta);

	if (present & 0x0010)
		in_uint8(s, os->opcode);

	if (present & 0x0020)
		rdp_in_coord(s, &os->srcx, delta);

	if (present & 0x0040)
		rdp_in_coord(s, &os->srcy, delta);

	DEBUG("SCREENBLT(op=0x%x,x=%d,y=%d,cx=%d,cy=%d,srcx=%d,srcy=%d)\n",
	       os->opcode, os->x, os->y, os->cx, os->cy, os->srcx, os->srcy);

	ui_screenblt(orders->rdp->inst, os->opcode, os->x, os->y, os->cx, os->cy,
		     os->srcx, os->srcy);
}

/* Process a line order */
static void
process_line(rdpOrders * orders, STREAM s, LINE_ORDER * os, uint32 present, RD_BOOL delta)
{
	if (present & 0x0001)
		in_uint16_le(s, os->mixmode);

	if (present & 0x0002)
		rdp_in_coord(s, &os->startx, delta);

	if (present & 0x0004)
		rdp_in_coord(s, &os->starty, delta);

	if (present & 0x0008)
		rdp_in_coord(s, &os->endx, delta);

	if (present & 0x0010)
		rdp_in_coord(s, &os->endy, delta);

	if (present & 0x0020)
		rdp_in_colour(s, &os->bgcolour);

	if (present & 0x0040)
		in_uint8(s, os->opcode);

	rdp_parse_pen(s, &os->pen, present >> 7);

	DEBUG("LINE(op=0x%x,sx=%d,sy=%d,dx=%d,dy=%d,fg=0x%x)\n",
	       os->opcode, os->startx, os->starty, os->endx, os->endy, os->pen.colour);

	if (os->opcode < 0x01 || os->opcode > 0x10)
	{
		ui_error(orders->rdp->inst, "bad ROP2 0x%x\n", os->opcode);
		return;
	}

	ui_line(orders->rdp->inst, os->opcode, os->startx, os->starty, os->endx,
		os->endy, &os->pen);
}

/* Process an opaque rectangle order */
static void
process_rect(rdpOrders * orders, STREAM s, RECT_ORDER * os, uint32 present, RD_BOOL delta)
{
	uint32 i;
	if (present & 0x01)
		rdp_in_coord(s, &os->x, delta);

	if (present & 0x02)
		rdp_in_coord(s, &os->y, delta);

	if (present & 0x04)
		rdp_in_coord(s, &os->cx, delta);

	if (present & 0x08)
		rdp_in_coord(s, &os->cy, delta);

	if (present & 0x10)
	{
		in_uint8(s, i);
		os->colour = (os->colour & 0xffffff00) | i;
	}

	if (present & 0x20)
	{
		in_uint8(s, i);
		os->colour = (os->colour & 0xffff00ff) | (i << 8);
	}

	if (present & 0x40)
	{
		in_uint8(s, i);
		os->colour = (os->colour & 0xff00ffff) | (i << 16);
	}

	DEBUG("RECT(x=%d,y=%d,cx=%d,cy=%d,fg=0x%x)\n", os->x, os->y, os->cx, os->cy, os->colour);

	ui_rect(orders->rdp->inst, os->x, os->y, os->cx, os->cy, os->colour);
}

/* Process a desktop save order */
static void
process_desksave(rdpOrders * orders, STREAM s, DESKSAVE_ORDER * os, uint32 present, RD_BOOL delta)
{
	int width, height;
	void * inst;

	if (present & 0x01)
		in_uint32_le(s, os->offset);

	if (present & 0x02)
		rdp_in_coord(s, &os->left, delta);

	if (present & 0x04)
		rdp_in_coord(s, &os->top, delta);

	if (present & 0x08)
		rdp_in_coord(s, &os->right, delta);

	if (present & 0x10)
		rdp_in_coord(s, &os->bottom, delta);

	if (present & 0x20)
		in_uint8(s, os->action);

	DEBUG("DESKSAVE(l=%d,t=%d,r=%d,b=%d,off=%d,op=%d)\n",
	       os->left, os->top, os->right, os->bottom, os->offset, os->action);

	width = os->right - os->left + 1;
	height = os->bottom - os->top + 1;

	inst = orders->rdp->inst;
	if (os->action == 0)
		ui_desktop_save(inst, os->offset, os->left, os->top, width, height);
	else
		ui_desktop_restore(inst, os->offset, os->left, os->top, width, height);
}

/* Process a memory blt order */
static void
process_memblt(rdpOrders * orders, STREAM s, MEMBLT_ORDER * os, uint32 present, RD_BOOL delta)
{
	RD_HBITMAP bitmap;

	if (present & 0x0001)
	{
		in_uint8(s, os->cache_id);
		in_uint8(s, os->colour_table);
	}

	if (present & 0x0002)
		rdp_in_coord(s, &os->x, delta);

	if (present & 0x0004)
		rdp_in_coord(s, &os->y, delta);

	if (present & 0x0008)
		rdp_in_coord(s, &os->cx, delta);

	if (present & 0x0010)
		rdp_in_coord(s, &os->cy, delta);

	if (present & 0x0020)
		in_uint8(s, os->opcode);

	if (present & 0x0040)
		rdp_in_coord(s, &os->srcx, delta);

	if (present & 0x0080)
		rdp_in_coord(s, &os->srcy, delta);

	if (present & 0x0100)
		in_uint16_le(s, os->cache_idx);

	DEBUG("MEMBLT(op=0x%x,x=%d,y=%d,cx=%d,cy=%d,id=%d,idx=%d)\n",
	       os->opcode, os->x, os->y, os->cx, os->cy, os->cache_id, os->cache_idx);

	bitmap = cache_get_bitmap(orders->rdp->cache, os->cache_id, os->cache_idx);
	if (bitmap == NULL)
		return;

	ui_memblt(orders->rdp->inst, os->opcode, os->x, os->y, os->cx, os->cy,
		  bitmap, os->srcx, os->srcy);
}

/* Process a 3-way blt order */
static void
process_triblt(rdpOrders * orders, STREAM s, TRIBLT_ORDER * os, uint32 present, RD_BOOL delta)
{
	RD_HBITMAP bitmap;
	RD_BRUSH brush;

	if (present & 0x000001)
	{
		in_uint8(s, os->cache_id);
		in_uint8(s, os->colour_table);
	}

	if (present & 0x000002)
		rdp_in_coord(s, &os->x, delta);

	if (present & 0x000004)
		rdp_in_coord(s, &os->y, delta);

	if (present & 0x000008)
		rdp_in_coord(s, &os->cx, delta);

	if (present & 0x000010)
		rdp_in_coord(s, &os->cy, delta);

	if (present & 0x000020)
		in_uint8(s, os->opcode);

	if (present & 0x000040)
		rdp_in_coord(s, &os->srcx, delta);

	if (present & 0x000080)
		rdp_in_coord(s, &os->srcy, delta);

	if (present & 0x000100)
		rdp_in_colour(s, &os->bgcolour);

	if (present & 0x000200)
		rdp_in_colour(s, &os->fgcolour);

	rdp_parse_brush(s, &os->brush, present >> 10);

	if (present & 0x008000)
		in_uint16_le(s, os->cache_idx);

	if (present & 0x010000)
		in_uint16_le(s, os->unknown);

	DEBUG("TRIBLT(op=0x%x,x=%d,y=%d,cx=%d,cy=%d,id=%d,idx=%d,bs=%d,bg=0x%x,fg=0x%x)\n",
	       os->opcode, os->x, os->y, os->cx, os->cy, os->cache_id, os->cache_idx,
	       os->brush.style, os->bgcolour, os->fgcolour);

	bitmap = cache_get_bitmap(orders->rdp->cache, os->cache_id, os->cache_idx);
	if (bitmap == NULL)
		return;

	setup_brush(orders, &brush, &os->brush);

	ui_triblt(orders->rdp->inst, os->opcode, os->x, os->y, os->cx, os->cy,
		  bitmap, os->srcx, os->srcy, &brush, os->bgcolour, os->fgcolour);
}

/* Process a polygon order */
static void
process_polygon(rdpOrders * orders, STREAM s, POLYGON_ORDER * os, uint32 present, RD_BOOL delta)
{
	int size;
	int index, data, next;
	uint8 flags = 0;
	RD_POINT *points;

	if (present & 0x01)
		rdp_in_coord(s, &os->x, delta);

	if (present & 0x02)
		rdp_in_coord(s, &os->y, delta);

	if (present & 0x04)
		in_uint8(s, os->opcode);

	if (present & 0x08)
		in_uint8(s, os->fillmode);

	if (present & 0x10)
		rdp_in_colour(s, &os->fgcolour);

	if (present & 0x20)
		in_uint8(s, os->npoints);

	if (present & 0x40)
	{
		in_uint8(s, os->datasize);
		in_uint8a(s, os->data, os->datasize);
	}

	DEBUG("POLYGON(x=%d,y=%d,op=0x%x,fm=%d,fg=0x%x,n=%d,sz=%d)\n",
	       os->x, os->y, os->opcode, os->fillmode, os->fgcolour, os->npoints, os->datasize);

	DEBUG("Data: ");

	for (index = 0; index < os->datasize; index++)
		DEBUG("%02x ", os->data[index]);

	DEBUG("\n");

	if (os->opcode < 0x01 || os->opcode > 0x10)
	{
		ui_error(orders->rdp->inst, "bad ROP2 0x%x\n", os->opcode);
		return;
	}

	size = (os->npoints + 1) * sizeof(RD_POINT);
	
	if (size > orders->buffer_size)
	{
		orders->buffer = xrealloc(orders->buffer, size);
		orders->buffer_size = size;
	}
	
	points = (RD_POINT *) orders->buffer;
	memset(points, 0, size);

	points[0].x = os->x;
	points[0].y = os->y;

	index = 0;
	data = ((os->npoints - 1) / 4) + 1;
	for (next = 1; (next <= os->npoints) && (next < 256) && (data < os->datasize); next++)
	{
		if ((next - 1) % 4 == 0)
			flags = os->data[index++];

		if (~flags & 0x80)
			points[next].x = parse_delta(os->data, &data);

		if (~flags & 0x40)
			points[next].y = parse_delta(os->data, &data);

		flags <<= 2;
	}

	if (next - 1 == os->npoints)
		ui_polygon(orders->rdp->inst, os->opcode, os->fillmode, points,
			   os->npoints + 1, NULL, 0, os->fgcolour);
	else
		ui_error(orders->rdp->inst, "polygon parse error\n");
}

/* Process a polygon2 order */
static void
process_polygon2(rdpOrders * orders, STREAM s, POLYGON2_ORDER * os, uint32 present, RD_BOOL delta)
{
	int size;
	int index, data, next;
	uint8 flags = 0;
	RD_POINT *points;
	RD_BRUSH brush;

	if (present & 0x0001)
		rdp_in_coord(s, &os->x, delta);

	if (present & 0x0002)
		rdp_in_coord(s, &os->y, delta);

	if (present & 0x0004)
		in_uint8(s, os->opcode);

	if (present & 0x0008)
		in_uint8(s, os->fillmode);

	if (present & 0x0010)
		rdp_in_colour(s, &os->bgcolour);

	if (present & 0x0020)
		rdp_in_colour(s, &os->fgcolour);

	rdp_parse_brush(s, &os->brush, present >> 6);

	if (present & 0x0800)
		in_uint8(s, os->npoints);

	if (present & 0x1000)
	{
		in_uint8(s, os->datasize);
		in_uint8a(s, os->data, os->datasize);
	}

	DEBUG("POLYGON2(x=%d,y=%d,op=0x%x,fm=%d,bs=%d,bg=0x%x,fg=0x%x,n=%d,sz=%d)\n",
	       os->x, os->y, os->opcode, os->fillmode, os->brush.style, os->bgcolour, os->fgcolour,
	       os->npoints, os->datasize);

	DEBUG("Data: ");

	for (index = 0; index < os->datasize; index++)
		DEBUG("%02x ", os->data[index]);

	DEBUG("\n");

	if (os->opcode < 0x01 || os->opcode > 0x10)
	{
		ui_error(orders->rdp->inst, "bad ROP2 0x%x\n", os->opcode);
		return;
	}

	setup_brush(orders, &brush, &os->brush);

	size = (os->npoints + 1) * sizeof(RD_POINT);
	
	if (size > orders->buffer_size)
	{
		orders->buffer = xrealloc(orders->buffer, size);
		orders->buffer_size = size;
	}
	
	points = (RD_POINT *) orders->buffer;
	memset(points, 0, size);

	points[0].x = os->x;
	points[0].y = os->y;

	index = 0;
	data = ((os->npoints - 1) / 4) + 1;
	for (next = 1; (next <= os->npoints) && (next < 256) && (data < os->datasize); next++)
	{
		if ((next - 1) % 4 == 0)
			flags = os->data[index++];

		if (~flags & 0x80)
			points[next].x = parse_delta(os->data, &data);

		if (~flags & 0x40)
			points[next].y = parse_delta(os->data, &data);

		flags <<= 2;
	}

	if (next - 1 == os->npoints)
		ui_polygon(orders->rdp->inst, os->opcode, os->fillmode, points,
			   os->npoints + 1, &brush, os->bgcolour, os->fgcolour);
	else
		ui_error(orders->rdp->inst, "polygon2 parse error\n");
}

/* Process a polyline order */
static void
process_polyline(rdpOrders * orders, STREAM s, POLYLINE_ORDER * os, uint32 present, RD_BOOL delta)
{
	int size;
	int index, next, data;
	uint8 flags = 0;
	RD_PEN pen;
	RD_POINT *points;

	if (present & 0x01)
		rdp_in_coord(s, &os->x, delta);

	if (present & 0x02)
		rdp_in_coord(s, &os->y, delta);

	if (present & 0x04)
		in_uint8(s, os->opcode);

	if (present & 0x10)
		rdp_in_colour(s, &os->fgcolour);

	if (present & 0x20)
		in_uint8(s, os->lines);

	if (present & 0x40)
	{
		in_uint8(s, os->datasize);
		in_uint8a(s, os->data, os->datasize);
	}

	DEBUG("POLYLINE(x=%d,y=%d,op=0x%x,fg=0x%x,n=%d,sz=%d)\n",
	       os->x, os->y, os->opcode, os->fgcolour, os->lines, os->datasize);

	DEBUG("Data: ");

	for (index = 0; index < os->datasize; index++)
		DEBUG("%02x ", os->data[index]);

	DEBUG("\n");

	if (os->opcode < 0x01 || os->opcode > 0x10)
	{
		ui_error(orders->rdp->inst, "bad ROP2 0x%x\n", os->opcode);
		return;
	}
	
	size = (os->lines + 1) * sizeof(RD_POINT);
	
	if (size > orders->buffer_size)
	{
		orders->buffer = xrealloc(orders->buffer, size);
		orders->buffer_size = size;
	}
	
	points = (RD_POINT *) orders->buffer;
	memset(points, 0, size);

	points[0].x = os->x;
	points[0].y = os->y;
	pen.style = pen.width = 0;
	pen.colour = os->fgcolour;

	index = 0;
	data = ((os->lines - 1) / 4) + 1;
	for (next = 1; (next <= os->lines) && (data < os->datasize); next++)
	{
		if ((next - 1) % 4 == 0)
			flags = os->data[index++];

		if (~flags & 0x80)
			points[next].x = parse_delta(os->data, &data);

		if (~flags & 0x40)
			points[next].y = parse_delta(os->data, &data);

		flags <<= 2;
	}

	if (next - 1 == os->lines)
		ui_polyline(orders->rdp->inst, os->opcode, points, os->lines + 1, &pen);
	else
		ui_error(orders->rdp->inst, "polyline parse error\n");
}

/* Process an ellipse order */
static void
process_ellipse(rdpOrders * orders, STREAM s, ELLIPSE_ORDER * os, uint32 present, RD_BOOL delta)
{
	if (present & 0x01)
		rdp_in_coord(s, &os->left, delta);

	if (present & 0x02)
		rdp_in_coord(s, &os->top, delta);

	if (present & 0x04)
		rdp_in_coord(s, &os->right, delta);

	if (present & 0x08)
		rdp_in_coord(s, &os->bottom, delta);

	if (present & 0x10)
		in_uint8(s, os->opcode);

	if (present & 0x20)
		in_uint8(s, os->fillmode);

	if (present & 0x40)
		rdp_in_colour(s, &os->fgcolour);

	DEBUG("ELLIPSE(l=%d,t=%d,r=%d,b=%d,op=0x%x,fm=%d,fg=0x%x)\n", os->left, os->top,
	       os->right, os->bottom, os->opcode, os->fillmode, os->fgcolour);

	ui_ellipse(orders->rdp->inst, os->opcode, os->fillmode, os->left, os->top,
		   os->right - os->left, os->bottom - os->top, NULL, 0, os->fgcolour);
}

/* Process an ellipse2 order */
static void
process_ellipse2(rdpOrders * orders, STREAM s, ELLIPSE2_ORDER * os, uint32 present, RD_BOOL delta)
{
	RD_BRUSH brush;

	if (present & 0x0001)
		rdp_in_coord(s, &os->left, delta);

	if (present & 0x0002)
		rdp_in_coord(s, &os->top, delta);

	if (present & 0x0004)
		rdp_in_coord(s, &os->right, delta);

	if (present & 0x0008)
		rdp_in_coord(s, &os->bottom, delta);

	if (present & 0x0010)
		in_uint8(s, os->opcode);

	if (present & 0x0020)
		in_uint8(s, os->fillmode);

	if (present & 0x0040)
		rdp_in_colour(s, &os->bgcolour);

	if (present & 0x0080)
		rdp_in_colour(s, &os->fgcolour);

	rdp_parse_brush(s, &os->brush, present >> 8);

	DEBUG("ELLIPSE2(l=%d,t=%d,r=%d,b=%d,op=0x%x,fm=%d,bs=%d,bg=0x%x,fg=0x%x)\n",
	       os->left, os->top, os->right, os->bottom, os->opcode, os->fillmode, os->brush.style,
	       os->bgcolour, os->fgcolour);

	setup_brush(orders, &brush, &os->brush);

	ui_ellipse(orders->rdp->inst, os->opcode, os->fillmode, os->left, os->top,
		   os->right - os->left, os->bottom - os->top, &brush, os->bgcolour,
		   os->fgcolour);
}

static void
do_glyph(rdpOrders * orders, uint8 * ttext, int * index, int * x, int * y, uint8 flags, uint8 font)
{
	int xyoffset, lindex = *index, lx = *x, ly = *y, gx, gy;
	FONTGLYPH * glyph;

	glyph = cache_get_font(orders->rdp->cache, font, ttext[lindex]);
	if (!(flags & TEXT2_IMPLICIT_X))
	{
		xyoffset = ttext[++lindex];
		if ((xyoffset & 0x80))
		{
			if (flags & TEXT2_VERTICAL)
				ly += ttext[lindex + 1] | (ttext[lindex + 2] << 8);
			else
				lx += ttext[lindex + 1] | (ttext[lindex + 2] << 8);
			lindex += 2;
		}
		else
		{
			if (flags & TEXT2_VERTICAL)
				ly += xyoffset;
			else
				lx += xyoffset;
		}
	}
	if (glyph != NULL)
	{
		gx = lx + glyph->offset;
		gy = ly + glyph->baseline;
		ui_draw_glyph(orders->rdp->inst, gx, gy, glyph->width, glyph->height,
			      glyph->pixmap);
		if (flags & TEXT2_IMPLICIT_X)
			lx += glyph->width;
	}
	*index = lindex;
	*x = lx;
	*y = ly;
}

static void
draw_text(rdpOrders * orders, uint8 font, uint8 flags, uint8 opcode, int mixmode,
	  int x, int y, int clipx, int clipy, int clipcx, int clipcy,
	  int boxx, int boxy, int boxcx, int boxcy, RD_BRUSH * brush,
	  int bgcolour, int fgcolour, uint8 * text, uint8 length)
{
	/* TODO: use brush appropriately */

	DATABLOB * entry;
	int i, j;
	uint8 * btext;

	/* Sometimes, the boxcx value is something really large, like
	   32691. This makes XCopyArea fail with Xvnc. The code below
	   is a quick fix. */
	if (boxx + boxcx > orders->rdp->settings->width)
		boxcx = orders->rdp->settings->width - boxx;

	if (boxcx > 1)
	{
		ui_rect(orders->rdp->inst, boxx, boxy, boxcx, boxcy, bgcolour);
	}
	else if (mixmode == MIX_OPAQUE)
	{
		ui_rect(orders->rdp->inst, clipx, clipy, clipcx, clipcy, bgcolour);
	}
	ui_start_draw_glyphs(orders->rdp->inst, bgcolour, fgcolour);
	/* Paint text, character by character */
	for (i = 0; i < length;)
	{
		switch (text[i])
		{
			case 0xff:
				/* At least two bytes needs to follow */
				if (i + 3 > length)
				{
					ui_warning(orders->rdp->inst, "Skipping short 0xff command:");
					for (j = 0; j < length; j++)
						fprintf(stderr, "%02x ", text[j]);
					fprintf(stderr, "\n");
					i = length = 0;
					break;
				}
				cache_put_text(orders->rdp->cache, text[i + 1], text, text[i + 2]);
				i += 3;
				length -= i;
				/* this will move pointer from start to first character after FF command */
				text = &(text[i]);
				i = 0;
				break;

			case 0xfe:
				/* At least one byte needs to follow */
				if (i + 2 > length)
				{
					ui_warning(orders->rdp->inst, "Skipping short 0xfe command:");
					for (j = 0; j < length; j++)
						fprintf(stderr, "%02x ", text[j]);
					fprintf(stderr, "\n");
					i = length = 0;
					break;
				}
				entry = cache_get_text(orders->rdp->cache, text[i + 1]);
				if (entry->data != NULL)
				{
					btext = (uint8 *) (entry->data);
					if ((btext[1] == 0) && (!(flags & TEXT2_IMPLICIT_X)) &&
					    (i + 2 < length))
					{
						if (flags & TEXT2_VERTICAL)
							y += text[i + 2];
						else
							x += text[i + 2];
					}
					for (j = 0; j < entry->size; j++)
						do_glyph(orders, btext, &j, &x, &y, flags, font);
				}
				if (i + 2 < length)
					i += 3;
				else
					i += 2;
				length -= i;
				/* this will move pointer from start to first character after FE command */
				text = &(text[i]);
				i = 0;
				break;

			default:
				do_glyph(orders, text, &i, &x, &y, flags, font);
				i++;
				break;
		}
	}
	if (boxcx > 1)
	{
		ui_end_draw_glyphs(orders->rdp->inst, boxx, boxy, boxcx, boxcy);
	}
	else
	{
		ui_end_draw_glyphs(orders->rdp->inst, clipx, clipy, clipcx, clipcy);
	}
}

/* Process a text order */
static void
process_text2(rdpOrders * orders, STREAM s, TEXT2_ORDER * os, uint32 present, RD_BOOL delta)
{
	int i;
	RD_BRUSH brush;

	if (present & 0x000001)
		in_uint8(s, os->font);

	if (present & 0x000002)
		in_uint8(s, os->flags);

	if (present & 0x000004)
		in_uint8(s, os->opcode);

	if (present & 0x000008)
		in_uint8(s, os->mixmode);

	if (present & 0x000010)
		rdp_in_colour(s, &os->fgcolour);

	if (present & 0x000020)
		rdp_in_colour(s, &os->bgcolour);

	if (present & 0x000040)
		in_uint16_le(s, os->clipleft);

	if (present & 0x000080)
		in_uint16_le(s, os->cliptop);

	if (present & 0x000100)
		in_uint16_le(s, os->clipright);

	if (present & 0x000200)
		in_uint16_le(s, os->clipbottom);

	if (present & 0x000400)
		in_uint16_le(s, os->boxleft);

	if (present & 0x000800)
		in_uint16_le(s, os->boxtop);

	if (present & 0x001000)
		in_uint16_le(s, os->boxright);

	if (present & 0x002000)
		in_uint16_le(s, os->boxbottom);

	rdp_parse_brush(s, &os->brush, present >> 14);

	if (present & 0x080000)
		in_uint16_le(s, os->x);

	if (present & 0x100000)
		in_uint16_le(s, os->y);

	if (present & 0x200000)
	{
		in_uint8(s, os->length);
		in_uint8a(s, os->text, os->length);
	}

	DEBUG("TEXT2(x=%d,y=%d,cl=%d,ct=%d,cr=%d,cb=%d,bl=%d,bt=%d,br=%d,bb=%d,bs=%d,bg=0x%x,fg=0x%x,font=%d,fl=0x%x,op=0x%x,mix=%d,n=%d)\n", os->x, os->y, os->clipleft, os->cliptop, os->clipright, os->clipbottom, os->boxleft, os->boxtop, os->boxright, os->boxbottom, os->brush.style, os->bgcolour, os->fgcolour, os->font, os->flags, os->opcode, os->mixmode, os->length);

	DEBUG("Text: ");

	for (i = 0; i < os->length; i++)
		DEBUG("%02x ", os->text[i]);

	DEBUG("\n");

	setup_brush(orders, &brush, &os->brush);

	draw_text(orders, os->font, os->flags, os->opcode, os->mixmode, os->x, os->y,
		  os->clipleft, os->cliptop, os->clipright - os->clipleft,
		  os->clipbottom - os->cliptop, os->boxleft, os->boxtop,
		  os->boxright - os->boxleft, os->boxbottom - os->boxtop,
		  &brush, os->bgcolour, os->fgcolour, os->text, os->length);
}

/* Process a raw bitmap cache order */
static void
process_raw_bmpcache(rdpOrders * orders, STREAM s)
{
	int size;
	RD_HBITMAP bitmap;
	uint16 cache_idx, bufsize;
	uint8 cache_id, width, height, bpp, Bpp;
	uint8 *data, *inverted;
	int y;

	in_uint8(s, cache_id);
	in_uint8s(s, 1);	/* pad */
	in_uint8(s, width);
	in_uint8(s, height);
	in_uint8(s, bpp);
	Bpp = (bpp + 7) / 8;
	in_uint16_le(s, bufsize);
	in_uint16_le(s, cache_idx);
	in_uint8p(s, data, bufsize);

	DEBUG("RAW_BMPCACHE(cx=%d,cy=%d,id=%d,idx=%d)\n", width, height, cache_id, cache_idx);

	size = width * height * Bpp;
	
	if (size > orders->buffer_size)
	{
		orders->buffer = xrealloc(orders->buffer, size);
		orders->buffer_size = size;
	}
	inverted = (uint8 *) orders->buffer;
	
	for (y = 0; y < height; y++)
	{
		memcpy(&inverted[(height - y - 1) * (width * Bpp)], &data[y * (width * Bpp)],
		       width * Bpp);
	}

	bitmap = ui_create_bitmap(orders->rdp->inst, width, height, inverted);
	cache_put_bitmap(orders->rdp->cache, cache_id, cache_idx, bitmap);
}

/* Process a bitmap cache order */
static void
process_bmpcache(rdpOrders * orders, STREAM s, uint16 flags)
{
	int buffer_size;
	RD_HBITMAP bitmap;
	uint16 cache_idx, size;
	uint8 cache_id, width, height, bpp, Bpp;
	uint8 *data, *bmpdata;
	uint16 bufsize, pad2, row_size, final_size;
	uint8 pad1;

	pad2 = row_size = final_size = 0xffff;	/* Shut the compiler up */

	in_uint8(s, cache_id);
	in_uint8(s, pad1);	/* pad */
	in_uint8(s, width);
	in_uint8(s, height);
	in_uint8(s, bpp);
	Bpp = (bpp + 7) / 8;
	in_uint16_le(s, bufsize);	/* bufsize */
	in_uint16_le(s, cache_idx);

	if (flags & 0x0400)
	{
		size = bufsize;
	}
	else
	{

		/* Begin compressedBitmapData */
		in_uint16_le(s, pad2);	/* pad */
		in_uint16_le(s, size);
		/*      in_uint8s(s, 4);  *//* row_size, final_size */
		in_uint16_le(s, row_size);
		in_uint16_le(s, final_size);

	}
	in_uint8p(s, data, size);

	DEBUG("BMPCACHE(cx=%d,cy=%d,id=%d,idx=%d,bpp=%d,size=%d,pad1=%d,bufsize=%d,pad2=%d,rs=%d,fs=%d)\n", width, height, cache_id, cache_idx, bpp, size, pad1, bufsize, pad2, row_size, final_size);

	buffer_size = width * height * Bpp;

	if (buffer_size > orders->buffer_size)
	{
		orders->buffer = xrealloc(orders->buffer, buffer_size);
		orders->buffer_size = buffer_size;
	}
	
	bmpdata = (uint8 *) orders->buffer;

	if (bitmap_decompress(orders->rdp->inst, bmpdata, width, height, data, size, Bpp))
	{
		bitmap = ui_create_bitmap(orders->rdp->inst, width, height, bmpdata);
		cache_put_bitmap(orders->rdp->cache, cache_id, cache_idx, bitmap);
	}
	else
	{
		DEBUG("Failed to decompress bitmap data\n");
	}
}

/* Process a bitmap cache v2 order */
static void
process_bmpcache2(rdpOrders * orders, STREAM s, uint16 flags, RD_BOOL compressed)
{
	int y;
	int size;
	RD_HBITMAP bitmap;
	uint8 cache_id, cache_idx_low, width, height, Bpp;
	uint16 cache_idx, bufsize;
	uint8 *data, *bmpdata, *bitmap_id;

	bitmap_id = NULL;	/* prevent compiler warning */
	cache_id = flags & ID_MASK;
	Bpp = ((flags & MODE_MASK) >> MODE_SHIFT) - 2;

	if (flags & PERSIST)
	{
		in_uint8p(s, bitmap_id, 8);
	}

	if (flags & SQUARE)
	{
		in_uint8(s, width);
		height = width;
	}
	else
	{
		in_uint8(s, width);
		in_uint8(s, height);
	}

	in_uint16_be(s, bufsize);
	bufsize &= BUFSIZE_MASK;
	in_uint8(s, cache_idx);

	if (cache_idx & LONG_FORMAT)
	{
		in_uint8(s, cache_idx_low);
		cache_idx = ((cache_idx ^ LONG_FORMAT) << 8) + cache_idx_low;
	}

	in_uint8p(s, data, bufsize);

	DEBUG("BMPCACHE2(compr=%d,flags=%x,cx=%d,cy=%d,id=%d,idx=%d,Bpp=%d,bs=%d)\n",
	       compressed, flags, width, height, cache_id, cache_idx, Bpp, bufsize);

	size = width * height * Bpp;

	if (size > orders->buffer_size)
	{
		orders->buffer = xrealloc(orders->buffer, size);
		orders->buffer_size = size;
	}
	
	bmpdata = (uint8 *) orders->buffer;

	if (compressed)
	{
		if (!bitmap_decompress(orders->rdp->inst, bmpdata, width, height, data, bufsize, Bpp))
		{
			DEBUG("Failed to decompress bitmap data\n");
			xfree(bmpdata);
			return;
		}
	}
	else
	{
		for (y = 0; y < height; y++)
			memcpy(&bmpdata[(height - y - 1) * (width * Bpp)],
			       &data[y * (width * Bpp)], width * Bpp);
	}

	bitmap = ui_create_bitmap(orders->rdp->inst, width, height, bmpdata);

	if (bitmap)
	{
		cache_put_bitmap(orders->rdp->cache, cache_id, cache_idx, bitmap);
		if (flags & PERSIST)
			pstcache_save_bitmap(orders->rdp->pcache, cache_id, cache_idx, bitmap_id,
					     width, height, width * height * Bpp, bmpdata);
	}
	else
	{
		DEBUG("process_bmpcache2: ui_create_bitmap failed\n");
	}
}

/* Process a colourmap cache order */
static void
process_colcache(rdpOrders * orders, STREAM s)
{
	int i;
	int size;
	RD_COLOURENTRY *entry;
	RD_COLOURMAP map;
	RD_HCOLOURMAP hmap;
	uint8 cache_id;

	in_uint8(s, cache_id);
	in_uint16_le(s, map.ncolours);

	size = sizeof(RD_COLOURENTRY) * map.ncolours;

	if (size > orders->buffer_size)
	{
		orders->buffer = xrealloc(orders->buffer, size);
		orders->buffer_size = size;
	}
	
	map.colours = (RD_COLOURENTRY *) orders->buffer;

	for (i = 0; i < map.ncolours; i++)
	{
		entry = &map.colours[i];
		in_uint8(s, entry->blue);
		in_uint8(s, entry->green);
		in_uint8(s, entry->red);
		in_uint8s(s, 1);	/* pad */
	}

	DEBUG("COLCACHE(id=%d,n=%d)\n", cache_id, map.ncolours);

	if (cache_id)
	{
		hmap = ui_create_colourmap(orders->rdp->inst, &map);
		ui_set_colourmap(orders->rdp->inst, hmap);
	}
}

/* Process a font cache order */
static void
process_fontcache(rdpOrders * orders, STREAM s)
{
	RD_HGLYPH bitmap;
	uint8 font, nglyphs;
	uint16 character, offset, baseline, width, height;
	int i, datasize;
	uint8 *data;

	in_uint8(s, font);
	in_uint8(s, nglyphs);

	DEBUG("FONTCACHE(font=%d,n=%d)\n", font, nglyphs);

	for (i = 0; i < nglyphs; i++)
	{
		in_uint16_le(s, character);
		in_uint16_le(s, offset);
		in_uint16_le(s, baseline);
		in_uint16_le(s, width);
		in_uint16_le(s, height);

		datasize = (height * ((width + 7) / 8) + 3) & ~3;
		in_uint8p(s, data, datasize);

		bitmap = ui_create_glyph(orders->rdp->inst, width, height, data);
		cache_put_font(orders->rdp->cache, font, character, offset, baseline, width,
			       height, bitmap);
	}
}

static void
process_compressed_8x8_brush_data(uint8 * in, uint8 * out, int Bpp)
{
	int x, y, pal_index, in_index, shift, do2, i;
	uint8 *pal;

	in_index = 0;
	pal = in + 16;
	/* read it bottom up */
	for (y = 7; y >= 0; y--)
	{
		/* 2 bytes per row */
		x = 0;
		for (do2 = 0; do2 < 2; do2++)
		{
			/* 4 pixels per byte */
			shift = 6;
			while (shift >= 0)
			{
				pal_index = (in[in_index] >> shift) & 3;
				/* size of palette entries depends on Bpp */
				for (i = 0; i < Bpp; i++)
				{
					out[(y * 8 + x) * Bpp + i] = pal[pal_index * Bpp + i];
				}
				x++;
				shift -= 2;
			}
			in_index++;
		}
	}
}

/* Process a brush cache order */
static void
process_brushcache(rdpOrders * orders, STREAM s, uint16 flags)
{
	RD_BRUSHDATA brush_data;
	uint8 cache_idx, colour_code, width, height, size, type;
	uint8 *comp_brush;
	int index;
	int Bpp;

	in_uint8(s, cache_idx);
	in_uint8(s, colour_code);
	in_uint8(s, width);
	in_uint8(s, height);
	in_uint8(s, type);	/* type, 0x8x = cached */
	in_uint8(s, size);

	DEBUG("BRUSHCACHE(idx=%d,dp=%d,wd=%d,ht=%d,sz=%d)\n", cache_idx, colour_code,
	       width, height, size);

	if ((width == 8) && (height == 8))
	{
		if (colour_code == 1)
		{
			brush_data.colour_code = 1;
			brush_data.data_size = 8;
			brush_data.data = (uint8 *) xmalloc(8);
			if (size == 8)
			{
				/* read it bottom up */
				for (index = 7; index >= 0; index--)
				{
					in_uint8(s, brush_data.data[index]);
				}
			}
			else
			{
				ui_warning(orders->rdp->inst, "incompatible brush, "
					   "colour_code %d size %d\n", colour_code,
					   size);
			}
			cache_put_brush_data(orders->rdp->cache, 1, cache_idx, &brush_data);
		}
		else if ((colour_code >= 3) && (colour_code <= 6))
		{
			Bpp = colour_code - 2;
			brush_data.colour_code = colour_code;
			brush_data.data_size = 8 * 8 * Bpp;
			brush_data.data = (uint8 *) xmalloc(8 * 8 * Bpp);
			if (size == 16 + 4 * Bpp)
			{
				in_uint8p(s, comp_brush, 16 + 4 * Bpp);
				process_compressed_8x8_brush_data(comp_brush, brush_data.data, Bpp);
			}
			else
			{
				in_uint8a(s, brush_data.data, 8 * 8 * Bpp);
			}
			cache_put_brush_data(orders->rdp->cache, colour_code, cache_idx, &brush_data);
		}
		else
		{
			ui_warning(orders->rdp->inst, "incompatible brush, colour_code %d "
				   "size %d\n", colour_code, size);
		}
	}
	else
	{
		ui_warning(orders->rdp->inst, "incompatible brush, width height %d %d\n",
			   width, height);
	}
}

/* Process a set drawing surface order */
static void
process_set_surface(rdpOrders * orders, STREAM s)
{
	sint16 idx;

	in_uint16_le(s, idx);
	ui_set_surface(orders->rdp->inst, idx >= 0 ?
		cache_get_bitmap(orders->rdp->cache, 255, idx) : NULL);
}

/* Process a create off-screen drawing surface order */
static void
process_create_surface(rdpOrders * orders, STREAM s)
{
	RD_HBITMAP bitmap;
	uint16 idx, width, height, free_num, free_idx;
	int i;

	in_uint16_le(s, idx);
	in_uint16_le(s, width);
	in_uint16_le(s, height);
	if (idx & 0x8000)
	{
		in_uint16_le(s, free_num);
		for (i = 0; i < free_num; i++)
		{
			in_uint16_le(s, free_idx);
			bitmap = cache_get_bitmap(orders->rdp->cache, 255, free_idx);
			ui_destroy_surface(orders->rdp->inst, bitmap);
			cache_put_bitmap(orders->rdp->cache, 255, free_idx, NULL);
		}
	}
	idx &= ~0x8000;
	bitmap = cache_get_bitmap(orders->rdp->cache, 255, idx);
	bitmap = ui_create_surface(orders->rdp->inst, width, height, bitmap);
	cache_put_bitmap(orders->rdp->cache, 255, idx, bitmap);
}

/* Process a non-standard order */
static void
process_non_standard_order(rdpOrders * orders, STREAM s, uint8 order_flags)
{
	if (!(order_flags & 0x2))
	{
		perror("order parsing failed\n");
		exit(1);
	}
	order_flags >>= 2;
	switch (order_flags)
	{
		case 0:
			process_set_surface(orders, s);
			break;
		case 1:
			process_create_surface(orders, s);
			break;
		default:
			ui_unimpl(orders->rdp->inst, "non-standard order %d:\n", order_flags);
			exit(1);
	}
}

/* Process a secondary order */
static void
process_secondary_order(rdpOrders * orders, STREAM s)
{
	/* The length isn't calculated correctly by the server.
	 * For very compact orders the length becomes negative
	 * so a signed integer must be used. */
	uint16 length;
	uint16 flags;
	uint8 type;
	uint8 *next_order;

	in_uint16_le(s, length);
	in_uint16_le(s, flags);	/* used by bmpcache2 */
	in_uint8(s, type);

	next_order = s->p + ((sint16) length) + 7;

	switch (type)
	{
		case RDP_ORDER_RAW_BMPCACHE:
			process_raw_bmpcache(orders, s);
			break;

		case RDP_ORDER_COLCACHE:
			process_colcache(orders, s);
			break;

		case RDP_ORDER_BMPCACHE:
			process_bmpcache(orders, s, flags);
			break;

		case RDP_ORDER_FONTCACHE:
			process_fontcache(orders, s);
			break;

		case RDP_ORDER_RAW_BMPCACHE2:
			process_bmpcache2(orders, s, flags, False);	/* uncompressed */
			break;

		case RDP_ORDER_BMPCACHE2:
			process_bmpcache2(orders, s, flags, True);	/* compressed */
			break;

		case RDP_ORDER_BRUSHCACHE:
			process_brushcache(orders, s, flags);
			break;

		default:
			ui_unimpl(orders->rdp->inst, "secondary order %d\n", type);
	}

	s->p = next_order;
}

/* Process an order PDU */
void
process_orders(rdpOrders * orders, STREAM s, uint16 num_orders)
{
	RDP_ORDER_STATE * os = (RDP_ORDER_STATE *) (orders->order_state);
	uint32 present;
	uint8 order_flags;
	int size, processed = 0;
	RD_BOOL delta;

	while (processed < num_orders)
	{
		in_uint8(s, order_flags);

		if (!(order_flags & RDP_ORDER_STANDARD))
		{
			process_non_standard_order(orders, s, order_flags);
		}
		else if (order_flags & RDP_ORDER_SECONDARY)
		{
			process_secondary_order(orders, s);
		}
		else
		{
			if (order_flags & RDP_ORDER_CHANGE)
			{
				in_uint8(s, os->order_type);
			}

			switch (os->order_type)
			{
				case RDP_ORDER_TRIBLT:
				case RDP_ORDER_TEXT2:
					size = 3;
					break;

				case RDP_ORDER_PATBLT:
				case RDP_ORDER_MEMBLT:
				case RDP_ORDER_LINE:
				case RDP_ORDER_POLYGON2:
				case RDP_ORDER_ELLIPSE2:
					size = 2;
					break;

				default:
					size = 1;
			}

			rdp_in_present(s, &present, order_flags, size);

			if (order_flags & RDP_ORDER_BOUNDS)
			{
				if (!(order_flags & RDP_ORDER_LASTBOUNDS))
					rdp_parse_bounds(s, &os->bounds);

				ui_set_clip(orders->rdp->inst, os->bounds.left,
					    os->bounds.top,
					    os->bounds.right -
					    os->bounds.left + 1,
					    os->bounds.bottom - os->bounds.top + 1);
			}

			delta = order_flags & RDP_ORDER_DELTA;

			switch (os->order_type)
			{
				case RDP_ORDER_DESTBLT:
					process_destblt(orders, s, &os->destblt, present, delta);
					break;

				case RDP_ORDER_PATBLT:
					process_patblt(orders, s, &os->patblt, present, delta);
					break;

				case RDP_ORDER_SCREENBLT:
					process_screenblt(orders, s, &os->screenblt, present, delta);
					break;

				case RDP_ORDER_LINE:
					process_line(orders, s, &os->line, present, delta);
					break;

				case RDP_ORDER_RECT:
					process_rect(orders, s, &os->rect, present, delta);
					break;

				case RDP_ORDER_DESKSAVE:
					process_desksave(orders, s, &os->desksave, present, delta);
					break;

				case RDP_ORDER_MEMBLT:
					process_memblt(orders, s, &os->memblt, present, delta);
					break;

				case RDP_ORDER_TRIBLT:
					process_triblt(orders, s, &os->triblt, present, delta);
					break;

				case RDP_ORDER_POLYGON:
					process_polygon(orders, s, &os->polygon, present, delta);
					break;

				case RDP_ORDER_POLYGON2:
					process_polygon2(orders, s, &os->polygon2, present, delta);
					break;

				case RDP_ORDER_POLYLINE:
					process_polyline(orders, s, &os->polyline, present, delta);
					break;

				case RDP_ORDER_ELLIPSE:
					process_ellipse(orders, s, &os->ellipse, present, delta);
					break;

				case RDP_ORDER_ELLIPSE2:
					process_ellipse2(orders, s, &os->ellipse2, present, delta);
					break;

				case RDP_ORDER_TEXT2:
					process_text2(orders, s, &os->text2, present, delta);
					break;

				default:
					ui_unimpl(orders->rdp->inst, "order %d\n", os->order_type);
					return;
			}

			if (order_flags & RDP_ORDER_BOUNDS)
				ui_reset_clip(orders->rdp->inst);
		}

		processed++;
	}
}

/* Reset order state */
void
reset_order_state(rdpOrders * orders)
{
	RDP_ORDER_STATE * os = (RDP_ORDER_STATE *) (orders->order_state);

	memset(os, 0, sizeof(RDP_ORDER_STATE));
	os->order_type = RDP_ORDER_PATBLT;
	ui_set_surface(orders->rdp->inst, NULL);
}

rdpOrders *
orders_new(struct rdp_rdp * rdp)
{
	rdpOrders * self;

	self = (rdpOrders *) xmalloc(sizeof(rdpOrders));
	if (self != NULL)
	{
		memset(self, 0, sizeof(rdpOrders));
		self->rdp = rdp;
		/* orders_state is void * */
		self->order_state = xmalloc(sizeof(RDP_ORDER_STATE));
		memset(self->order_state, 0, sizeof(RDP_ORDER_STATE));
		/* reusable buffer */
		self->buffer_size = 4096;
		self->buffer = xmalloc(self->buffer_size);
		memset(self->buffer, 0, self->buffer_size);
	}
	return self;
}

void
orders_free(rdpOrders * orders)
{
	if (orders != NULL)
	{
		xfree(orders->order_state);
		xfree(orders->buffer);
		xfree(orders);
	}
}

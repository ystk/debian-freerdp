/*
   Copyright (c) 2009-2010 Jay Sorg

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
*/

#ifndef __FRDP_H
#define __FRDP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "types_ui.h"
#include "types.h"

#ifdef _WIN32
#include "winsock2.h"
#define snprintf sprintf_s
#define strdup _strdup
#endif

#ifndef MIN
#define MIN(x,y)		(((x) < (y)) ? (x) : (y))
#endif

#ifndef MAX
#define MAX(x,y)		(((x) > (y)) ? (x) : (y))
#endif

void
ui_error(rdpInst * inst, char * format, ...);
void
ui_warning(rdpInst * inst, char * format, ...);
void
ui_unimpl(rdpInst * inst, char * format, ...);
void
hexdump(unsigned char * p, int len);
int
load_licence(unsigned char ** data);
RD_BOOL
rd_lock_file(int fd, int start, int len);
int
rd_lseek_file(int fd, int offset);
int
rd_write_file(int fd, void * ptr, int len);
RD_BOOL
rd_pstcache_mkdir(void);
void
rd_close_file(int fd);
int
rd_read_file(int fd, void * ptr, int len);
int
rd_open_file(char * filename);
void
generate_random(uint8 * random);
void
save_licence(unsigned char * data, int length);
void
ui_begin_update(rdpInst * inst);
void
ui_end_update(rdpInst * inst);
void
ui_line(rdpInst * inst, uint8 opcode, int startx, int starty, int endx, int endy, RD_PEN * pen);
void
ui_rect(rdpInst * inst, int x, int y, int cx, int cy, int colour);
void
ui_polygon(rdpInst * inst, uint8 opcode, uint8 fillmode, RD_POINT * point, int npoints,
	   RD_BRUSH * brush, int bgcolour, int fgcolour);
void
ui_polyline(rdpInst * inst, uint8 opcode, RD_POINT * points, int npoints, RD_PEN * pen);
void
ui_ellipse(rdpInst * inst, uint8 opcode, uint8 fillmode, int x, int y, int cx, int cy,
	   RD_BRUSH * brush, int bgcolour, int fgcolour);
void
ui_start_draw_glyphs(rdpInst * inst, int bgcolour, int fgcolour);
void
ui_draw_glyph(rdpInst * inst, int x, int y, int cx, int cy, RD_HGLYPH glyph);
void
ui_end_draw_glyphs(rdpInst * inst, int x, int y, int cx, int cy);
void
ui_desktop_save(rdpInst * inst, uint32 offset, int x, int y, int cx, int cy);
void
ui_desktop_restore(rdpInst * inst, uint32 offset, int x, int y, int cx, int cy);
uint32
ui_get_toggle_keys_state(rdpInst * inst);
void
ui_bell(rdpInst * inst);
void
ui_destblt(rdpInst * inst, uint8 opcode, int x, int y, int cx, int cy);
void
ui_patblt(rdpInst * inst, uint8 opcode, int x, int y, int cx, int cy, RD_BRUSH * brush,
	  int bgcolour, int fgcolour);
void
ui_screenblt(rdpInst * inst, uint8 opcode, int x, int y, int cx, int cy, int srcx, int srcy);
void
ui_memblt(rdpInst * inst, uint8 opcode, int x, int y, int cx, int cy, RD_HBITMAP src,
	  int srcx, int srcy);
void
ui_triblt(rdpInst * inst, uint8 opcode, int x, int y, int cx, int cy, RD_HBITMAP src,
	  int srcx, int srcy, RD_BRUSH * brush, int bgcolour, int fgcolour);
RD_HGLYPH
ui_create_glyph(rdpInst * inst, int width, int height, uint8 * data);
void
ui_destroy_glyph(rdpInst * inst, RD_HGLYPH glyph);
int
ui_select(rdpInst * inst, int rdp_socket);
void
ui_set_clip(rdpInst * inst, int x, int y, int cx, int cy);
void
ui_reset_clip(rdpInst * inst);
void
ui_resize_window(rdpInst * inst);
RD_HCURSOR
ui_create_cursor(rdpInst * inst, unsigned int x, unsigned int y, int width, int height,
		 uint8 * andmask, uint8 * xormask, int bpp);
void
ui_set_cursor(rdpInst * inst, RD_HCURSOR cursor);
void
ui_set_null_cursor(rdpInst * inst);
void
ui_set_default_cursor(rdpInst * inst);
void
ui_destroy_cursor(rdpInst * inst, RD_HCURSOR cursor);
RD_HBITMAP
ui_create_bitmap(rdpInst * inst, int width, int height, uint8 * data);
void
ui_paint_bitmap(rdpInst * inst, int x, int y, int cx, int cy, int width, int height, uint8 * data);
void
ui_destroy_bitmap(rdpInst * inst, RD_HBITMAP bmp);
RD_HCOLOURMAP
ui_create_colourmap(rdpInst * inst, RD_COLOURMAP * colours);
void
ui_move_pointer(rdpInst * inst, int x, int y);
void
ui_set_colourmap(rdpInst * inst, RD_HCOLOURMAP map);
RD_HBITMAP
ui_create_surface(rdpInst * inst, int width, int height, RD_HBITMAP old);
void
ui_set_surface(rdpInst * inst, RD_HBITMAP surface);
void
ui_destroy_surface(rdpInst * inst, RD_HBITMAP surface);
void
ui_channel_data(rdpInst * inst, int chan_id, char * data, int data_size,
		int flags, int total_size);

#endif

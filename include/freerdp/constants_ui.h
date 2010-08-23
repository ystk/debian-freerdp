/*
   Copyright (c) 2009 Jay Sorg

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

   these are the types needed for the ui interface
   self contained file, requires no other includes

*/

#ifndef __CONSTANTS_UI_H
#define __CONSTANTS_UI_H

/* TCP port for Remote Desktop Protocol */
#define TCP_PORT_RDP 3389

enum RDP_INPUT_DEVICE
{
	RDP_INPUT_SYNC = 0x0000,
	RDP_INPUT_SCANCODE = 0x0004,
	RDP_INPUT_UNICODE = 0x0005,
	RDP_INPUT_MOUSE = 0x8001,
	RDP_INPUT_MOUSEX = 0x8002
};

/* Device flags */
#define KBD_FLAG_RIGHT          0x0001
#define KBD_FLAG_EXT            0x0100
#define KBD_FLAG_QUIET          0x1000
#define KBD_FLAG_DOWN           0x4000
#define KBD_FLAG_UP             0x8000

/* Toggle key synchronization */
#define KBD_SYNC_SCROLL_LOCK	0x0001
#define KBD_SYNC_NUM_LOCK	0x0002
#define KBD_SYNC_CAPS_LOCK	0x0004
#define KBD_SYNC_KANA_LOCK	0x0008

/* See T.128 */
#define RDP_KEYPRESS 0
#define RDP_KEYRELEASE (KBD_FLAG_DOWN | KBD_FLAG_UP)

/* these are the old style defines, PTRFLAGS_* is the new defines, use them */
#define MOUSE_FLAG_MOVE         0x0800
#define MOUSE_FLAG_BUTTON1      0x1000
#define MOUSE_FLAG_BUTTON2      0x2000
#define MOUSE_FLAG_BUTTON3      0x4000
#define MOUSE_FLAG_BUTTON4      0x0280
#define MOUSE_FLAG_BUTTON5      0x0380
#define MOUSE_FLAG_DOWN         0x8000

#define PTRFLAGS_WHEEL_NEGATIVE 0x0100
#define PTRFLAGS_WHEEL          0x0200
#define PTRFLAGS_BUTTON1        0x1000
#define PTRFLAGS_BUTTON2        0x2000
#define PTRFLAGS_BUTTON3        0x4000
#define PTRFLAGS_MOVE           0x0800
#define PTRFLAGS_DOWN           0x8000

#define RDP5_DISABLE_NOTHING	0x00
#define RDP5_NO_WALLPAPER	0x01
#define RDP5_NO_FULLWINDOWDRAG	0x02
#define RDP5_NO_MENUANIMATIONS	0x04
#define RDP5_NO_THEMING		0x08
#define RDP5_NO_CURSOR_SHADOW	0x20
#define RDP5_NO_CURSORSETTINGS	0x40	/* disables cursor blinking */

#endif

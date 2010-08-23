/* -*- c-basic-offset: 8 -*-
   FreeRDP: A Remote Desktop Protocol client.
   User interface services - X keyboard mapping using XKB

   Copyright (C) Marc-Andre Moreau <marcandre.moreau@gmail.com> 2009

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

#ifndef __FREERDP_KBD_H
#define __FREERDP_KBD_H

#define RDP_KEYBOARD_LAYOUT_TYPE_STANDARD   1
#define RDP_KEYBOARD_LAYOUT_TYPE_VARIANT    2
#define RDP_KEYBOARD_LAYOUT_TYPE_IME        4

typedef struct rdp_keyboard_layout
{
	unsigned int code;
	char name[50];
} rdpKeyboardLayout;

rdpKeyboardLayout *
freerdp_kbd_get_layouts(int types);
unsigned int
freerdp_kbd_init(unsigned int keyboard_layout_id);
int
freerdp_kbd_get_scancode_by_keycode(uint8 keycode, int * flags);
int
freerdp_kbd_get_scancode_by_virtualkey(int vkcode);

#endif // __FREERDP_KBD_H


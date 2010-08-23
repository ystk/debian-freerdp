
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freerdp/freerdp.h>
#include "xf_types.h"
#include "xf_event.h"
#include "xf_keyboard.h"

static int
xf_handle_event_Expose(xfInfo * xfi, XEvent * xevent)
{
	int x;
	int y;
	int cx;
	int cy;

	if (xevent->xexpose.window == xfi->wnd)
	{
		x = xevent->xexpose.x;
		y = xevent->xexpose.y;
		cx = xevent->xexpose.width;
		cy = xevent->xexpose.height;
		XCopyArea(xfi->display, xfi->backstore, xfi->wnd, xfi->gc_default,
			x, y, cx, cy, x, y);
	}
	return 0;
}

static int
xf_handle_event_VisibilityNotify(xfInfo * xfi, XEvent * xevent)
{
	if (xevent->xvisibility.window == xfi->wnd)
	{
		xfi->unobscured = xevent->xvisibility.state == VisibilityUnobscured;
	}
	return 0;
}

static int
xf_handle_event_MotionNotify(xfInfo * xfi, XEvent * xevent)
{
	int x;
	int y;

	if (xevent->xmotion.window == xfi->wnd)
	{
		x = xevent->xmotion.x;
		y = xevent->xmotion.y;
		xfi->inst->rdp_send_input(xfi->inst, RDP_INPUT_MOUSE, PTRFLAGS_MOVE, x, y);
	}

	if (xfi->fullscreen)
		XSetInputFocus(xfi->display, xfi->wnd, RevertToPointerRoot, CurrentTime);

	return 0;
}

static int
xf_handle_event_ButtonPress(xfInfo * xfi, XEvent * xevent)
{
	int device_flags;
	int param1;
	int param2;

	if (xevent->xbutton.window == xfi->wnd)
	{
		device_flags = 0;
		param1 = 0;
		param2 = 0;
		switch (xevent->xbutton.button)
		{
			case 1:
				device_flags = PTRFLAGS_DOWN | PTRFLAGS_BUTTON1;
				param1 = xevent->xbutton.x;
				param2 = xevent->xbutton.y;
				break;
			case 2:
				device_flags = PTRFLAGS_DOWN | PTRFLAGS_BUTTON3;
				param1 = xevent->xbutton.x;
				param2 = xevent->xbutton.y;
				break;
			case 3:
				device_flags = PTRFLAGS_DOWN | PTRFLAGS_BUTTON2;
				param1 = xevent->xbutton.x;
				param2 = xevent->xbutton.y;
				break;
			case 4:
				device_flags = PTRFLAGS_WHEEL | 0x0078;
				break;
			case 5:
				device_flags = PTRFLAGS_WHEEL | PTRFLAGS_WHEEL_NEGATIVE | 0x0088;
				break;
		}
		if (device_flags != 0)
		{
			xfi->inst->rdp_send_input(xfi->inst, RDP_INPUT_MOUSE, device_flags,
				param1, param2);
		}
	}
	return 0;
}

static int
xf_handle_event_ButtonRelease(xfInfo * xfi, XEvent * xevent)
{
	int device_flags;
	int param1;
	int param2;

	if (xevent->xbutton.window == xfi->wnd)
	{
		device_flags = 0;
		param1 = 0;
		param2 = 0;
		switch (xevent->xbutton.button)
		{
			case 1:
				device_flags = PTRFLAGS_BUTTON1;
				param1 = xevent->xbutton.x;
				param2 = xevent->xbutton.y;
				break;
			case 2:
				device_flags = PTRFLAGS_BUTTON3;
				param1 = xevent->xbutton.x;
				param2 = xevent->xbutton.y;
				break;
			case 3:
				device_flags = PTRFLAGS_BUTTON2;
				param1 = xevent->xbutton.x;
				param2 = xevent->xbutton.y;
				break;
		}
		if (device_flags != 0)
		{
			xfi->inst->rdp_send_input(xfi->inst, RDP_INPUT_MOUSE, device_flags,
				param1, param2);
		}
	}
	return 0;
}

static int
xf_handle_event_KeyPress(xfInfo * xfi, XEvent * xevent)
{
	KeySym keysym;
	char str[256];

	XLookupString((XKeyEvent *) xevent, str, sizeof(str), &keysym, NULL);

	xf_kb_set_keypress(xevent->xkey.keycode, keysym);
	if (xfi->fs_toggle && xf_kb_handle_special_keys(xfi, keysym))
		return 0;

	xf_kb_send_key(xfi, RDP_KEYPRESS, xevent->xkey.keycode);
	return 0;
}

static RD_BOOL
xf_skip_key_release(xfInfo * xfi, XEvent * xevent)
{
	XEvent next_event;

	if (XPending(xfi->display))
	{
		memset(&next_event, 0, sizeof(next_event));
		XPeekEvent(xfi->display, &next_event);
		if (next_event.type == KeyPress)
		{
			if (next_event.xkey.keycode == xevent->xkey.keycode)
			{
				return True;
			}
		}
	}
	return False;
}

static int
xf_handle_event_KeyRelease(xfInfo * xfi, XEvent * xevent)
{
	if (xf_skip_key_release(xfi, xevent))
	{
		return 0;
	}
	xf_kb_unset_keypress(xevent->xkey.keycode);
	xf_kb_send_key(xfi, RDP_KEYRELEASE, xevent->xkey.keycode);
	return 0;
}

static int
xf_handle_event_FocusIn(xfInfo * xfi, XEvent * xevent)
{
	if (xevent->xfocus.mode == NotifyGrab)
		return 0;

	xfi->focused = True;
	if (xfi->mouse_into)
		XGrabKeyboard(xfi->display, xfi->wnd, True, GrabModeAsync, GrabModeAsync, CurrentTime);

	xf_kb_focus_in(xfi);
	return 0;
}

static int
xf_handle_event_FocusOut(xfInfo * xfi, XEvent * xevent)
{
	if (xevent->xfocus.mode == NotifyUngrab)
		return 0;

	xfi->focused = False;
	if (xevent->xfocus.mode == NotifyWhileGrabbed)
		XUngrabKeyboard(xfi->display, CurrentTime);

	return 0;
}

static int
xf_handle_event_MappingNotify(xfInfo * xfi, XEvent * xevent)
{
	if (xevent->xmapping.request == MappingModifier)
	{
		XFreeModifiermap(xfi->mod_map);
		xfi->mod_map = XGetModifierMapping(xfi->display);
	}
	return 0;
}

static int
xf_handle_event_ClientMessage(xfInfo * xfi, XEvent * xevent)
{
	Atom protocol_atom = XInternAtom(xfi->display, "WM_PROTOCOLS", True);
	Atom kill_atom = XInternAtom(xfi->display, "WM_DELETE_WINDOW", True);

	if ((xevent->xclient.message_type == protocol_atom)
	    && ((Atom) xevent->xclient.data.l[0] == kill_atom))
	{
		printf("xf_handle_event: ClientMessage user quit received\n");
		return 1;
	}

	return 0;
}

static int
xf_handle_event_EnterNotify(xfInfo * xfi, XEvent * xevent)
{
	xfi->mouse_into = True;

	if (xfi->fullscreen)
		XSetInputFocus(xfi->display, xfi->wnd, RevertToPointerRoot, CurrentTime);

	if (xfi->focused)
		XGrabKeyboard(xfi->display, xfi->wnd, True, GrabModeAsync, GrabModeAsync, CurrentTime);

	return 0;
}

static int
xf_handle_event_LeaveNotify(xfInfo * xfi, XEvent * xevent)
{
	xfi->mouse_into = False;
	XUngrabKeyboard(xfi->display, CurrentTime);
	return 0;
}

int
xf_handle_event(xfInfo * xfi, XEvent * xevent)
{
	int rv;

	rv = 0;
	switch (xevent->type)
	{
		case Expose:
			rv = xf_handle_event_Expose(xfi, xevent);
			break;
		case VisibilityNotify:
			rv = xf_handle_event_VisibilityNotify(xfi, xevent);
			break;
		case MotionNotify:
			rv = xf_handle_event_MotionNotify(xfi, xevent);
			break;
		case ButtonPress:
			rv = xf_handle_event_ButtonPress(xfi, xevent);
			break;
		case ButtonRelease:
			rv = xf_handle_event_ButtonRelease(xfi, xevent);
			break;
		case KeyPress:
			rv = xf_handle_event_KeyPress(xfi, xevent);
			break;
		case KeyRelease:
			rv = xf_handle_event_KeyRelease(xfi, xevent);
			break;
		case FocusIn:
			rv = xf_handle_event_FocusIn(xfi, xevent);
			break;
		case FocusOut:
			rv = xf_handle_event_FocusOut(xfi, xevent);
			break;
		case EnterNotify:
			rv = xf_handle_event_EnterNotify(xfi, xevent);
			break;
		case LeaveNotify:
			rv = xf_handle_event_LeaveNotify(xfi, xevent);
			break;
		case NoExpose:
			printf("xf_handle_event: NoExpose\n");
			break;
		case GraphicsExpose:
			printf("xf_handle_event: GraphicsExpose\n");
			break;
		case ConfigureNotify:
			printf("xf_handle_event: ConfigureNotify\n");
			break;
		case MapNotify:
			printf("xf_handle_event: MapNotify\n");
			break;
		case ReparentNotify:
			printf("xf_handle_event: ReparentNotify\n");
			break;
		case MappingNotify:
			rv = xf_handle_event_MappingNotify(xfi, xevent);
			break;
		case ClientMessage:
			/* the window manager told us to quit */
			rv = xf_handle_event_ClientMessage(xfi, xevent);
			break;
		default:
			printf("xf_handle_event unknown event %d\n", xevent->type);
			break;
	}
	return rv;
}

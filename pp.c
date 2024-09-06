#include <X11/Xlib.h>
#include <dlfcn.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

// const long MOUSE_STUFF_MASK = ButtonMotionMask | Button1MotionMask | Button2MotionMask
//     | Button3MotionMask | Button4MotionMask | Button5MotionMask
//     | ButtonPressMask | ButtonReleaseMask;

bool first_win_found = false;
bool win_locked = false;
Display *display = NULL;
Window win_to_lock = BadWindow;
long toggle_lock_keycode = 105; // right control
GC gc;
XWindowAttributes attrs;
XColor toggle_color = {.red = 0xFFFF, .green = 0, .blue = 0, .flags = DoRed | DoGreen | DoBlue };
time_t toggle_time;
int BORDER_FLASH_TIME = 2;

void toggle_lock()
{
    if(!win_locked)
    {
        int status = XGrabPointer(display, win_to_lock, True, 0, GrabModeAsync, GrabModeAsync, win_to_lock, None, CurrentTime);
        if(status != GrabSuccess)
        {
            fprintf(stderr, "Failed to grab window: %d\n", status);
            return;
        }
        fprintf(stderr, "Grabbed window\n");
        win_locked = true;
        time(&toggle_time);
    }
    else
    {
        XUngrabPointer(display, CurrentTime);
        fprintf(stderr, "Ungrabbed window\n");
        win_locked = false;
        time(&toggle_time);
    }
}

void init_window_drawing()
{
    XGetWindowAttributes(display, win_to_lock, &attrs);
    XGCValues values;
    gc = XCreateGC(display, win_to_lock, 0, &values);
    XSetLineAttributes(display, gc, 40, LineDoubleDash, CapButt, JoinBevel);
    XSetFillStyle(display, gc, FillSolid);
    XAllocColor(display, attrs.colormap, &toggle_color);
    XSetForeground(display, gc, toggle_color.pixel);
}

/** implements hooking the root window of the first created window */
void init_first_window(Display *disp, Window win)
{
    // we want to hook only windows that use mouse events to prevent false errors
    if(first_win_found)
    {
        return;
    }
    Window root;
    Window parent;
    Window *children;
    unsigned int nchildren_return;
    Status status = XQueryTree(disp, win, &root, &parent, &children, &nchildren_return);
    if(status == BadWindow)
    {
        return;
    }
    first_win_found = true;
    display = disp;
    win_to_lock = root;
    if(children != NULL)
    {
        XFree(children);
    }
    fprintf(stderr, "found initial root window: %ld\n", win_to_lock);
    
    // this is also a good time to initialize and read environment variables, since shared objects dont get main()'s
    init_window_drawing();

    // TODO

    // lock the mouse pointer unless the env vars tell us not to
    // const char *init_lock_str = getenv("PP_NO_INITIAL_LOCK");
    // if(init_lock_str != nullptr)
    // {
    //     std::string init_lock{init_lock_str};
    //     if(init_lock == "1" || init_lock == "Y" || init_lock == "y")
    //     {
    //         return;
    //     }
    // }
    toggle_lock();
    return;
}
void xevent_hook(XEvent *evt)
{
    if(evt == NULL)
    {
        return;
    }
    if(evt->type == KeyPress)
    {
        if(evt->xkey.keycode == toggle_lock_keycode)
        {
            toggle_lock();
        }
    }
    if(evt->type == Expose && !(evt->xexpose.count))
    {
        time_t now;
        time(&now);
        //fprintf(stderr, "%f\n", difftime(now, toggle_time));
        if(difftime(now, toggle_time) < BORDER_FLASH_TIME)
        {
        }
    }
    XDrawRectangle(display, win_to_lock, gc, attrs.x, attrs.y, attrs.width, attrs.height);
    fprintf(stderr, "%d, %d, %d, %d\n", attrs.x, attrs.y, attrs.width, attrs.height);
}

#define IMPL(NAME) ((__typeof__(&NAME))(dlsym(RTLD_NEXT, #NAME)))

int XMapWindow(Display *d, Window w)
{
    Status impl_ret = IMPL(XMapWindow)(d, w);
    if(impl_ret != BadWindow)
    {
        init_first_window(d, w);
    }
    return impl_ret;
}

Bool XCheckMaskEvent(Display *d, long mask, XEvent* e)
{
    Bool impl_ret = IMPL(XCheckMaskEvent)(d, mask, e);
    if(impl_ret)
    {
        xevent_hook(e);
    }
    return impl_ret;
}
Bool XCheckTypedEvent(Display *d, int evt_type, XEvent *e)
{
    Bool impl_ret = IMPL(XCheckTypedEvent)(display, evt_type, e);
    if(impl_ret)
    {
        xevent_hook(e);
    }
    return impl_ret;
}
Bool XCheckTypedWindowEvent(Display *d, Window w, int evt_type, XEvent *e)
{
    Bool impl_ret = IMPL(XCheckTypedWindowEvent)(d, w, evt_type, e);
    if(impl_ret)
    {
        xevent_hook(e);
    }
    return impl_ret;
}
Bool XCheckIfEvent(Display *d, XEvent *e, Bool(*pred)(Display*, XEvent*, XPointer), XPointer a)
{
    Bool impl_ret = IMPL(XCheckIfEvent)(d, e, pred, a);
    if(impl_ret)
    {
        xevent_hook(e);
    }
    return impl_ret;
}
int XIfEvent(Display *d, XEvent *e, Bool(*pred)(Display*, XEvent*, XPointer), XPointer a)
{
    int impl_ret = IMPL(XIfEvent)(d, e, pred, a);
    if(impl_ret)
    {
        xevent_hook(e);
    }
    return impl_ret;
}
int XMaskEvent(Display *d, long mask, XEvent* e)
{
    int impl_ret = IMPL(XMaskEvent)(d, mask, e);
    if(impl_ret)
    {
        xevent_hook(e);
    }
    return impl_ret;
}
int XNextEvent(Display *d, XEvent *e)
{
    int impl_ret = IMPL(XNextEvent)(d, e);
    xevent_hook(e);
    return impl_ret;
}
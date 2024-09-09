#include <X11/Xlib.h>
#include <dlfcn.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

// const long MOUSE_STUFF_MASK = ButtonMotionMask | Button1MotionMask | Button2MotionMask
//     | Button3MotionMask | Button4MotionMask | Button5MotionMask
//     | ButtonPressMask | ButtonReleaseMask;

bool win_locked_at_least_once = false;
bool win_locked = false;
Display *display = NULL;
Window win_to_lock = BadWindow;
long toggle_lock_keycode = 105; // right control
XWindowAttributes attrs;
unsigned long next_serial_ignore = 0;

Window find_largest_nonroot_window(Window win)
{
    Window head, root, parent;
    Window *children;
    unsigned int nchildren;

    head = win;
    while (true)
    {
        Status status = XQueryTree(display, head, &root, &parent, &children, &nchildren);
        if (status == BadWindow || parent == root)
        {
            return head;
        }
        head = parent;
    }
}
void toggle_lock()
{
    if (!win_locked)
    {
        if (win_locked_at_least_once)
        {
            // now that we are re-enaging the lock, we should lock the window with focus
            Window win;
            int revert_state;
            next_serial_ignore = NextRequest(display);
            XGetInputFocus(display, &win, &revert_state);
            if (win != BadWindow)
            {
                win_to_lock = win;
                fprintf(stderr, "new window to grab: %ld\n", win_to_lock);
            }
        }
        next_serial_ignore = NextRequest(display);
        int status = XGrabPointer(display, win_to_lock, True, 0, GrabModeAsync, GrabModeAsync, win_to_lock, None, CurrentTime);
        if (status != GrabSuccess)
        {
            fprintf(stderr, "Failed to grab window: %d\n", status);
            return;
        }
        fprintf(stderr, "Grabbed window\n");
        win_locked = true;
        win_locked_at_least_once = true;
    }
    else
    {
        next_serial_ignore = NextRequest(display);
        XUngrabPointer(display, CurrentTime);
        fprintf(stderr, "Ungrabbed window\n");
        win_locked = false;
    }
}
int(*x_error_handler_impl)(Display*, XErrorEvent*) = NULL;
int x_error_handler(Display* disp, XErrorEvent *evt)
{
    if(!x_error_handler_impl)
    {
        return 0;
    }
    if(disp != display || evt == NULL)
    {
        return x_error_handler_impl(disp, evt);
    }
    if(evt->serial == next_serial_ignore)
    {
        fprintf(stderr, "Swallowed error code: %d\n", evt->error_code);
        return 0;
    }
    return x_error_handler_impl(disp, evt);
}
/** implements hooking the root window of the first created window */
void init_first_window(Display *disp, Window win)
{
    // we want to hook only windows that use mouse events to prevent false errors
    if (display != NULL)
    {
        return;
    }
    display = disp;
    win_to_lock = win;
    Status status = XGetWindowAttributes(display, win_to_lock, &attrs);
    if (status == BadWindow || status == BadDrawable)
    {
        return;
    }
    fprintf(stderr, "%d, %d, %d, %d\n", attrs.x, attrs.y, attrs.width, attrs.height);
    fprintf(stderr, "found initial root window: %ld\n", win_to_lock);
    x_error_handler_impl = XSetErrorHandler(x_error_handler);

    // this is also a good time to initialize and read environment variables, since shared objects dont get main()'s

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
    return;
}
void xevent_hook(XEvent *evt)
{
    if (evt == NULL || display == NULL)
    {
        return;
    }
    if (evt->type == KeyPress)
    {
        if (evt->xkey.keycode == toggle_lock_keycode)
        {
            toggle_lock();
        }
    }
    // wait until XEvents start coming in to try to lock the window
    if (display != NULL && !win_locked_at_least_once)
    {
        toggle_lock();
    }
}

int patch_XMapWindow(Display *d, Window w, __typeof__(&XMapWindow) impl)
{
    Status impl_ret = impl(d, w);
    if (impl_ret != BadWindow)
    {
        init_first_window(d, w);
    }
    return impl_ret;
}
int patch_XMapRaised(Display *d, Window w, __typeof__(&XMapRaised) impl)
{
    Status impl_ret = impl(d, w);
    if (impl_ret != BadWindow)
    {
        init_first_window(d, w);
    }
    return impl_ret;
}
Bool patch_XCheckMaskEvent(Display *d, long mask, XEvent *e, __typeof__(&XCheckMaskEvent) impl)
{
    Bool impl_ret = impl(d, mask, e);
    if (impl_ret)
    {
        xevent_hook(e);
    }
    return impl_ret;
}
Bool patch_XCheckTypedEvent(Display *d, int evt_type, XEvent *e, __typeof__(&XCheckTypedEvent) impl)
{
    Bool impl_ret = impl(display, evt_type, e);
    if (impl_ret)
    {
        xevent_hook(e);
    }
    return impl_ret;
}
Bool patch_XCheckTypedWindowEvent(Display *d, Window w, int evt_type, XEvent *e, __typeof__(&XCheckTypedWindowEvent) impl)
{
    Bool impl_ret = impl(d, w, evt_type, e);
    if (impl_ret)
    {
        xevent_hook(e);
    }
    return impl_ret;
}
Bool patch_XCheckIfEvent(Display *d, XEvent *e, Bool (*pred)(Display *, XEvent *, XPointer), XPointer a, __typeof__(&XCheckIfEvent) impl)
{
    Bool impl_ret = impl(d, e, pred, a);
    if (impl_ret)
    {
        xevent_hook(e);
    }
    return impl_ret;
}
int patch_XIfEvent(Display *d, XEvent *e, Bool (*pred)(Display *, XEvent *, XPointer), XPointer a, __typeof__(&XIfEvent) impl)
{
    int impl_ret = impl(d, e, pred, a);
    if (impl_ret)
    {
        xevent_hook(e);
    }
    return impl_ret;
}
int patch_XMaskEvent(Display *d, long mask, XEvent *e, __typeof__(&XMaskEvent) impl)
{
    int impl_ret = impl(d, mask, e);
    if (impl_ret)
    {
        xevent_hook(e);
    }
    return impl_ret;
}
int patch_XNextEvent(Display *d, XEvent *e, __typeof__(&XNextEvent) impl)
{
    int impl_ret = impl(d, e);
    xevent_hook(e);
    return impl_ret;
}
int patch_XUngrabPointer(Display *d, Time t, __typeof(&XUngrabPointer) impl)
{
    if(win_locked)
    {
        return 0;
    }
    //TODO: do i need to check time
    return impl(d, t);
}

enum XMethodEnum
{
#define X(name) XMethod_##name,
#include "xlib_funcs.inc"
#undef X
    NUM_XMethod
};

void *xlib_impls[NUM_XMethod];
bool xlibs_initialized = false;
void* get_xlib_impl(enum XMethodEnum method)
{
    if(!xlibs_initialized)
    {
#define X(name) xlib_impls[XMethod_##name] = dlsym(RTLD_NEXT, #name);
#include "xlib_funcs.inc"
#undef X
        xlibs_initialized = true;
    }
    return xlib_impls[method];
}

int XMapWindow(Display *d, Window w)
{
    return patch_XMapWindow(d, w, get_xlib_impl(XMethod_XMapWindow));
};
int XMapRaised(Display *d, Window w)
{
    return patch_XMapWindow(d, w, get_xlib_impl(XMethod_XMapRaised));
};
Bool XCheckMaskEvent(Display *d, long mask, XEvent *e)
{
    return patch_XCheckMaskEvent(d, mask, e, get_xlib_impl(XMethod_XCheckMaskEvent));
};
Bool XCheckTypedEvent(Display *d, int evt_type, XEvent *e)
{
    return patch_XCheckTypedEvent(d, evt_type, e, get_xlib_impl(XMethod_XCheckTypedEvent));
};
Bool XCheckTypedWindowEvent(Display *d, Window w, int evt_type, XEvent *e)
{
    return patch_XCheckTypedWindowEvent(d, w, evt_type, e, get_xlib_impl(XMethod_XCheckTypedWindowEvent));
};
Bool XCheckIfEvent(Display *d, XEvent *e, Bool (*pred)(Display *, XEvent *, XPointer), XPointer a)
{
    return patch_XCheckIfEvent(d, e, pred, a, get_xlib_impl(XMethod_XCheckIfEvent));
};
int XIfEvent(Display *d, XEvent *e, Bool (*pred)(Display *, XEvent *, XPointer), XPointer a)
{
    return patch_XIfEvent(d, e, pred, a, get_xlib_impl(XMethod_XIfEvent));
};
int XMaskEvent(Display *d, long mask, XEvent *e)
{
    return patch_XMaskEvent(d, mask, e, get_xlib_impl(XMethod_XMaskEvent));
};
int XNextEvent(Display *d, XEvent *e)
{
    return patch_XNextEvent(d, e, get_xlib_impl(XMethod_XNextEvent));
};
int XUngrabPointer(Display *d, Time t)
{
    return patch_XUngrabPointer(d, t, get_xlib_impl(XMethod_XUngrabPointer));
}

void *sdl_xlib_impls[NUM_XMethod] = {0};
void *(*SDL_LoadFunction_impl)(void*, const char*) = NULL;

int SDL_XMapWindow(Display *d, Window w)
{
    return patch_XMapWindow(d, w, sdl_xlib_impls[XMethod_XMapWindow]);
};
int SDL_XMapRaised(Display *d, Window w)
{
    return patch_XMapRaised(d, w, sdl_xlib_impls[XMethod_XMapRaised]);
};
Bool SDL_XCheckMaskEvent(Display *d, long mask, XEvent *e)
{
    return patch_XCheckMaskEvent(d, mask, e, sdl_xlib_impls[XMethod_XCheckMaskEvent]);
};
Bool SDL_XCheckTypedEvent(Display *d, int evt_type, XEvent *e)
{
    return patch_XCheckTypedEvent(d, evt_type, e, sdl_xlib_impls[XMethod_XCheckTypedEvent]);
};
Bool SDL_XCheckTypedWindowEvent(Display *d, Window w, int evt_type, XEvent *e)
{
    return patch_XCheckTypedWindowEvent(d, w, evt_type, e, sdl_xlib_impls[XMethod_XCheckTypedWindowEvent]);
};
Bool SDL_XCheckIfEvent(Display *d, XEvent *e, Bool (*pred)(Display *, XEvent *, XPointer), XPointer a)
{
    return patch_XCheckIfEvent(d, e, pred, a, sdl_xlib_impls[XMethod_XCheckIfEvent]);
};
int SDL_XIfEvent(Display *d, XEvent *e, Bool (*pred)(Display *, XEvent *, XPointer), XPointer a)
{
    return patch_XIfEvent(d, e, pred, a, sdl_xlib_impls[XMethod_XIfEvent]);
};
int SDL_XMaskEvent(Display *d, long mask, XEvent *e)
{
    return patch_XMaskEvent(d, mask, e, sdl_xlib_impls[XMethod_XMaskEvent]);
};
int SDL_XNextEvent(Display *d, XEvent *e)
{
    return patch_XNextEvent(d, e, sdl_xlib_impls[XMethod_XNextEvent]);
};
int SDL_XUngrabPointer(Display *d, Time t)
{
    return patch_XUngrabPointer(d, t, sdl_xlib_impls[XMethod_XUngrabPointer]);
};

enum XMethodEnum name_to_xmethod(const char *str)
{
#define X(name) if(strcmp(str, #name) == 0) { return XMethod_##name; }
#include "xlib_funcs.inc"
#undef X
    return NUM_XMethod;
}

void *SDL_LoadFunction(void *handle, const char *name)
{
    fprintf(stderr, "loading func: %s\n", name);
    void *ret;
    if(!SDL_LoadFunction_impl)
    {
        SDL_LoadFunction_impl = dlsym(RTLD_NEXT, "SDL_LoadFunction");
    }
    ret = SDL_LoadFunction_impl(handle, name);
    if(ret == NULL)
    {
        return ret;
    }
    enum XMethodEnum method = name_to_xmethod(name);
    if(method == NUM_XMethod)
    {
        return ret;
    }
    sdl_xlib_impls[method] = ret;
    switch(method)
    {
#define X(name) case XMethod_##name: return &SDL_##name;
#include "xlib_funcs.inc"
#undef X
    default: return NULL;
    }
}
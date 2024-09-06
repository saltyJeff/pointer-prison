#include <X11/Xlib.h>
#include <dlfcn.h>
#include <iostream>

const long MOUSE_STUFF_MASK = ButtonMotionMask | Button1MotionMask | Button2MotionMask
    | Button3MotionMask | Button4MotionMask | Button5MotionMask
    | ButtonPressMask | ButtonReleaseMask;

bool first_win_found = false;
bool win_locked = false;
Display *display = nullptr;
Window win_to_lock = BadWindow;
long lock_keycode = 105; // right control

void toggle_lock()
{
    if(!win_locked)
    {
        int status = XGrabPointer(display, win_to_lock, True, MOUSE_STUFF_MASK, GrabModeAsync, GrabModeAsync, win_to_lock, None, CurrentTime);
        if(status != GrabSuccess)
        {
            std::clog << "Failed to grab window: " << status << std::endl;
            return;
        }
        win_locked = true;
    }
    else
    {
        XUngrabPointer(display, CurrentTime);
        std::clog << "Ungrabbed window" << std::endl;
        win_locked = false;
    }
}

void toggle_lock_keypress_hook(XKeyEvent)

/** implements hooking the root window of the first created window */
void create_window_hook(Display *disp, Window win, long evt_mask)
{
    
    // we want to hook only windows that use mouse events to prevent false errors
    if(first_win_found || (evt_mask & MOUSE_STUFF_MASK) == 0)
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
    if(children != nullptr)
    {
        XFree(children);
    }
    std::clog << "found initial root window: " << win_to_lock << std::endl;
    
    // this is also a good time to read environment variables, since shared objects dont get main()'s
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
}

#pragma region monkey patches
template <typename Ret, typename... Args>
Ret PATCHER(const char *name, Args... args)
{
    static Ret (*func)(Args...) = nullptr;
    if (func == nullptr)
    {
        func = reinterpret_cast<decltype(func)>(dlsym(RTLD_NEXT, name));
    }
    return func(args...);
}

Window XCreateWindow(Display *disp, Window win, int a0, int a1, unsigned int a2, unsigned int a3, unsigned int a4, int a5, unsigned int a6, Visual *a7, unsigned long a8, XSetWindowAttributes *attrs)
{
    auto ret = PATCHER<int, Display *, Window, int, int, unsigned int, unsigned int, 
        unsigned int, int, unsigned int, Visual*, unsigned long, XSetWindowAttributes*>("XCreateWindow", disp, win, a0, 
        a1, a2, a3, a4, a5, a6, a7, a8, attrs);
    if(ret != BadAlloc &&
        ret != BadColor &&
        ret != BadCursor &&
        ret != BadMatch &&
        ret != BadPixmap && 
        ret != BadValue && 
        ret != BadWindow)
    {
        if(attrs != nullptr)
        {
            create_window_hook(disp, win, attrs->event_mask);
        }
    }
    return ret;
}

int XChangeWindowAttributes(Display *disp, Window win, unsigned long a0, XSetWindowAttributes *attrs)
{
    auto ret = PATCHER<int, Display *, Window, unsigned long, XSetWindowAttributes *>("XChangeWindowAttributes", disp, win, a0, attrs);
    if(ret != BadAccess &&
        ret != BadColor &&
        ret != BadCursor &&
        ret != BadMatch &&
        ret != BadPixmap && 
        ret != BadValue && 
        ret != BadWindow)
    {
        if(attrs != nullptr)
        {
            create_window_hook(disp, win, attrs->event_mask);
        }
    }
    return ret;
}

int XSelectInput(Display *disp, Window win, long event_mask)
{
    auto ret = PATCHER<int, Display *, Window, long>("XSelectInput", disp, win, event_mask);
    if(ret != BadWindow)
    {
        create_window_hook(disp, win, event_mask);
    }
    return ret;
}

#pragma endregion
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrandr.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

struct rect {
    int x;
    int y;
    int width;
    int height;
};

struct motif_wm_hints {
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
    long input_mode;
    unsigned long status;
};

struct app_state {
    Display *display;
    int screen;
    Window root;
    Window window;
    Window titlebar_window;
    Window close_button_window;
    Window body_window;
    Atom wm_delete;
    Atom wm_change_state;
    Atom net_wm_state;
    Atom wm_maximized_vert;
    Atom wm_maximized_horz;
    GC close_gc;
    unsigned long titlebar_color;
    unsigned long close_button_color;
    int use_titlebar;
    int show_titlebar_minmax;
    int width;
    int height;
    int window_x;
    int window_y;
    int titlebar_height;
    int close_button_width;
    int dragging;
    int drag_root_x;
    int drag_root_y;
    int drag_window_x;
    int drag_window_y;
};

enum {
    MWM_HINTS_FUNCTIONS = 1UL << 0,
    MWM_HINTS_DECORATIONS = 1UL << 1
};

enum {
    MWM_FUNC_RESIZE = 1UL << 1,
    MWM_FUNC_MOVE = 1UL << 2,
    MWM_FUNC_MINIMIZE = 1UL << 3,
    MWM_FUNC_MAXIMIZE = 1UL << 4,
    MWM_FUNC_CLOSE = 1UL << 5
};

enum {
    MWM_DECOR_BORDER = 1UL << 1,
    MWM_DECOR_RESIZEH = 1UL << 2,
    MWM_DECOR_TITLE = 1UL << 3,
    MWM_DECOR_MENU = 1UL << 4,
    MWM_DECOR_MINIMIZE = 1UL << 5,
    MWM_DECOR_MAXIMIZE = 1UL << 6
};

static int env_startup_request_delay_ms(void)
{
    const char *value = getenv("TEST_STARTUP_REQUEST_DELAY_MS");
    char *end = NULL;
    long parsed;
    const int fallback = 20;

    if (value == NULL || *value == '\0') {
        return fallback;
    }

    errno = 0;
    parsed = strtol(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0') {
        return fallback;
    }

    return (int)parsed;
}

static int env_use_titlebar(void)
{
    const char *value = getenv("TEST_SSD");
    char *end = NULL;
    long parsed;
    const int fallback = 1;

    if (value == NULL || *value == '\0') {
        return fallback;
    }

    errno = 0;
    parsed = strtol(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0') {
        return fallback;
    }

    return parsed != 0;
}

static int env_titlebar_minmax(void)
{
    const char *value = getenv("TEST_TITLEBAR_MINMAX");
    char *end = NULL;
    long parsed;
    const int fallback = 1;

    if (value == NULL || *value == '\0') {
        return fallback;
    }

    errno = 0;
    parsed = strtol(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0') {
        return fallback;
    }

    return parsed != 0;
}

static void debug_drag_log(struct app_state *app, const char *message)
{
    fprintf(stdout, "drag-debug: %s\n", message);
    fflush(stdout);
}

static void debug_drag_event(struct app_state *app,
                             const char *phase,
                             Window event_window,
                             int event_x,
                             int event_y,
                             int root_x,
                             int root_y,
                             int new_x,
                             int new_y)
{
    fprintf(stdout,
            "drag-debug: %s event_window=0x%lx event=(%d,%d) root=(%d,%d) start_window=(%d,%d) current_window=(%d,%d) target=(%d,%d) dragging=%d\n",
            phase,
            (unsigned long)event_window,
            event_x,
            event_y,
            root_x,
            root_y,
            app->drag_window_x,
            app->drag_window_y,
            app->window_x,
            app->window_y,
            new_x,
            new_y,
            app->dragging);
    fflush(stdout);
}

static void sleep_ms(int delay_ms)
{
    struct timespec delay;

    if (delay_ms <= 0) {
        return;
    }

    delay.tv_sec = delay_ms / 1000;
    delay.tv_nsec = (long)(delay_ms % 1000) * 1000000L;
    nanosleep(&delay, NULL);
}

static void set_undecorated(Display *display, Window window)
{
    Atom motif_hints_atom;
    Atom net_wm_window_type;
    Atom net_wm_window_type_normal;
    Atom kde_override;
    Atom types[2];
    struct motif_wm_hints hints;

    motif_hints_atom = XInternAtom(display, "_MOTIF_WM_HINTS", False);
    hints.flags = MWM_HINTS_FUNCTIONS | MWM_HINTS_DECORATIONS;
    hints.functions = MWM_FUNC_RESIZE | MWM_FUNC_MOVE | MWM_FUNC_MINIMIZE | MWM_FUNC_MAXIMIZE | MWM_FUNC_CLOSE;
    hints.decorations = 0;
    hints.input_mode = 0;
    hints.status = 0;

    XChangeProperty(display,
                    window,
                    motif_hints_atom,
                    motif_hints_atom,
                    32,
                    PropModeReplace,
                    (unsigned char *)&hints,
                    5);

    net_wm_window_type = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
    net_wm_window_type_normal = XInternAtom(display, "_NET_WM_WINDOW_TYPE_NORMAL", False);
    kde_override = XInternAtom(display, "_KDE_NET_WM_WINDOW_TYPE_OVERRIDE", False);
    types[0] = kde_override;
    types[1] = net_wm_window_type_normal;
    XChangeProperty(display,
                    window,
                    net_wm_window_type,
                    XA_ATOM,
                    32,
                    PropModeReplace,
                    (unsigned char *)types,
                    2);
}

static void set_decorated_with_minmax(Display *display, Window window)
{
    Atom motif_hints_atom;
    struct motif_wm_hints hints;

    motif_hints_atom = XInternAtom(display, "_MOTIF_WM_HINTS", False);
    hints.flags = MWM_HINTS_FUNCTIONS | MWM_HINTS_DECORATIONS;
    hints.functions = MWM_FUNC_RESIZE | MWM_FUNC_MOVE | MWM_FUNC_MINIMIZE | MWM_FUNC_MAXIMIZE | MWM_FUNC_CLOSE;
    hints.decorations = MWM_DECOR_BORDER | MWM_DECOR_RESIZEH | MWM_DECOR_TITLE | MWM_DECOR_MENU
                        | MWM_DECOR_MINIMIZE | MWM_DECOR_MAXIMIZE;
    hints.input_mode = 0;
    hints.status = 0;

    XChangeProperty(display,
                    window,
                    motif_hints_atom,
                    motif_hints_atom,
                    32,
                    PropModeReplace,
                    (unsigned char *)&hints,
                    5);
}

static unsigned long alloc_named_color(Display *display,
                                       int screen,
                                       const char *name,
                                       unsigned long fallback)
{
    Colormap colormap;
    XColor exact;
    XColor screen_color;

    colormap = DefaultColormap(display, screen);
    if (XAllocNamedColor(display, colormap, name, &screen_color, &exact)) {
        return screen_color.pixel;
    }

    return fallback;
}

static struct rect pick_target_rect(Display *display, Window root, int width, int height)
{
    struct rect target;
    int count = 0;
    XRRMonitorInfo *monitors;
    int screen = DefaultScreen(display);

    target.x = (DisplayWidth(display, screen) - width) / 2;
    target.y = (DisplayHeight(display, screen) - height) / 2;
    target.width = width;
    target.height = height;

    monitors = XRRGetMonitors(display, root, True, &count);
    if (monitors != NULL && count > 0) {
        XRRMonitorInfo *chosen = NULL;
        int i;

        for (i = 0; i < count; ++i) {
            if (monitors[i].primary) {
                chosen = &monitors[i];
                break;
            }
        }

        if (chosen == NULL) {
            chosen = &monitors[count - 1];
        }

        target.x = chosen->x + (chosen->width - width) / 2;
        target.y = chosen->y + (chosen->height - height) / 2;
    }

    if (monitors != NULL) {
        XRRFreeMonitors(monitors);
    }

    return target;
}

static void layout_children(struct app_state *app, int width, int height)
{
    if (width < 1) {
        width = 1;
    }
    if (height < 1) {
        height = 1;
    }

    app->width = width;
    app->height = height;

    if (app->use_titlebar) {
        XMoveResizeWindow(app->display,
                          app->body_window,
                          0,
                          0,
                          (unsigned int)width,
                          (unsigned int)height);
        return;
    }

    XMoveResizeWindow(app->display,
                      app->body_window,
                      0,
                      0,
                      (unsigned int)width,
                      (unsigned int)height);
}

static int titlebar_control_count(const struct app_state *app)
{
    return app->show_titlebar_minmax ? 3 : 1;
}

static int titlebar_controls_width(const struct app_state *app)
{
    return app->close_button_width * titlebar_control_count(app);
}

static void draw_close_button(struct app_state *app)
{
    int width;
    int height;
    int close_x;
    int maximize_x;
    int minimize_x;
    int pad;
    Window draw_window;

    if (app->use_titlebar || app->window == None || app->close_gc == None) {
        return;
    }

    draw_window = app->body_window;
    if (draw_window == None) {
        return;
    }

    width = app->close_button_width;
    if (width < 1) {
        width = 1;
    }
    height = app->titlebar_height;
    if (height < 1) {
        height = 1;
    }

    pad = width < height ? width / 4 : height / 4;
    if (pad < 6) {
        pad = 6;
    }
    if (pad * 2 >= width) {
        pad = width / 3;
    }
    if (pad * 2 >= height) {
        pad = height / 3;
    }
    if (pad < 1) {
        pad = 1;
    }

    close_x = app->width - width;
    maximize_x = close_x - width;
    minimize_x = maximize_x - width;

    XSetForeground(app->display, app->close_gc, app->titlebar_color);
    XFillRectangle(app->display,
                    draw_window,
                   app->close_gc,
                   0,
                   0,
                   (unsigned int)app->width,
                   (unsigned int)height);
    XSetForeground(app->display, app->close_gc, app->close_button_color);
    if (app->show_titlebar_minmax) {
        XFillRectangle(app->display,
                       draw_window,
                       app->close_gc,
                       minimize_x,
                       0,
                       (unsigned int)width,
                       (unsigned int)height);
        XFillRectangle(app->display,
                       draw_window,
                       app->close_gc,
                       maximize_x,
                       0,
                       (unsigned int)width,
                       (unsigned int)height);
    }
    XFillRectangle(app->display,
                    draw_window,
                   app->close_gc,
                   close_x,
                   0,
                   (unsigned int)width,
                   (unsigned int)height);
    XSetForeground(app->display, app->close_gc, WhitePixel(app->display, app->screen));
    XDrawLine(app->display,
                draw_window,
              app->close_gc,
              close_x + pad,
              pad,
              close_x + width - pad - 1,
              height - pad - 1);
    XDrawLine(app->display,
                draw_window,
              app->close_gc,
              close_x + width - pad - 1,
              pad,
              close_x + pad,
              height - pad - 1);

    if (app->show_titlebar_minmax) {
        XDrawLine(app->display,
                  draw_window,
                  app->close_gc,
                  minimize_x + pad,
                  height - pad - 2,
                  minimize_x + width - pad - 1,
                  height - pad - 2);
        XDrawRectangle(app->display,
                       draw_window,
                       app->close_gc,
                       maximize_x + pad,
                       pad,
                       (unsigned int)(width - pad * 2 - 1),
                       (unsigned int)(height - pad * 2 - 1));
    }
}

static int point_in_titlebar(struct app_state *app, int x, int y)
{
    if (app->use_titlebar) {
        return 0;
    }

    return y >= 0 && y < app->titlebar_height && x >= 0
           && x < app->width - titlebar_controls_width(app);
}

static int point_in_close_button(struct app_state *app, int x, int y)
{
    if (app->use_titlebar) {
        return 0;
    }

    return y >= 0 && y < app->titlebar_height && x >= app->width - app->close_button_width;
}

static int point_in_maximize_button(struct app_state *app, int x, int y)
{
    if (app->use_titlebar || !app->show_titlebar_minmax) {
        return 0;
    }

    return y >= 0 && y < app->titlebar_height
           && x >= app->width - app->close_button_width * 2
           && x < app->width - app->close_button_width;
}

static int point_in_minimize_button(struct app_state *app, int x, int y)
{
    if (app->use_titlebar || !app->show_titlebar_minmax) {
        return 0;
    }

    return y >= 0 && y < app->titlebar_height
           && x >= app->width - app->close_button_width * 3
           && x < app->width - app->close_button_width * 2;
}

static void send_titlebar_client_message(struct app_state *app,
                                         Atom message_type,
                                         long data0,
                                         long data1,
                                         long data2,
                                         long data3,
                                         long data4)
{
    XEvent event;

    memset(&event, 0, sizeof(event));
    event.xclient.type = ClientMessage;
    event.xclient.window = app->window;
    event.xclient.message_type = message_type;
    event.xclient.format = 32;
    event.xclient.data.l[0] = data0;
    event.xclient.data.l[1] = data1;
    event.xclient.data.l[2] = data2;
    event.xclient.data.l[3] = data3;
    event.xclient.data.l[4] = data4;

    XSendEvent(app->display,
               app->root,
               False,
               SubstructureNotifyMask | SubstructureRedirectMask,
               &event);
    XFlush(app->display);
}

static void request_minimize(struct app_state *app)
{
    send_titlebar_client_message(app, app->wm_change_state, 3, 0, 0, 0, 0);
}

static void request_toggle_maximize(struct app_state *app)
{
    send_titlebar_client_message(app,
                                 app->net_wm_state,
                                 2,
                                 (long)app->wm_maximized_vert,
                                 (long)app->wm_maximized_horz,
                                 1,
                                 0);
}

static void begin_drag(struct app_state *app, const XButtonEvent *button_event)
{
    int root_x = button_event->x_root;
    int root_y = button_event->y_root;
    Window root_return;
    Window child_return;
    int win_x;
    int win_y;
    unsigned int mask;

    if (XQueryPointer(app->display,
                      app->root,
                      &root_return,
                      &child_return,
                      &root_x,
                      &root_y,
                      &win_x,
                      &win_y,
                      &mask) == False) {
        debug_drag_log(app, "XQueryPointer failed on begin_drag; using button root coordinates");
        root_x = button_event->x_root;
        root_y = button_event->y_root;
    }

    app->dragging = 1;
    app->drag_root_x = root_x;
    app->drag_root_y = root_y;
    app->drag_window_x = app->window_x;
    app->drag_window_y = app->window_y;

    debug_drag_event(app,
                     "begin",
                     button_event->window,
                     button_event->x,
                     button_event->y,
                     root_x,
                     root_y,
                     app->window_x,
                     app->window_y);
}

static void update_drag(struct app_state *app, const XMotionEvent *motion_event)
{
    int root_x = motion_event->x_root;
    int root_y = motion_event->y_root;
    Window root_return;
    Window child_return;
    int win_x;
    int win_y;
    unsigned int mask;
    int dx;
    int dy;

    if (!app->dragging) {
        return;
    }

    if (XQueryPointer(app->display,
                      app->root,
                      &root_return,
                      &child_return,
                      &root_x,
                      &root_y,
                      &win_x,
                      &win_y,
                      &mask) == False) {
        debug_drag_log(app, "XQueryPointer failed on update_drag; using motion root coordinates");
        root_x = motion_event->x_root;
        root_y = motion_event->y_root;
    }

    dx = root_x - app->drag_root_x;
    dy = root_y - app->drag_root_y;
    app->window_x += dx;
    app->window_y += dy;
    app->drag_root_x = root_x;
    app->drag_root_y = root_y;

    debug_drag_event(app,
                     "motion",
                     motion_event->window,
                     motion_event->x,
                     motion_event->y,
                     root_x,
                     root_y,
                     app->window_x,
                     app->window_y);

    XMoveResizeWindow(app->display,
                      app->window,
                      app->window_x,
                      app->window_y,
                      (unsigned int)app->width,
                      (unsigned int)app->height);
    XFlush(app->display);
}

static void end_drag(struct app_state *app, const XButtonEvent *button_event)
{
    if (!app->dragging) {
        return;
    }

    debug_drag_event(app,
                     "end",
                     button_event->window,
                     button_event->x,
                     button_event->y,
                     button_event->x_root,
                     button_event->y_root,
                     app->window_x,
                     app->window_y);
    app->dragging = 0;
}

int main(void)
{
    Display *display;
    int screen;
    Window root;
    Atom wm_delete;
    XEvent event;
    XSizeHints size_hints;
    XClassHint class_hint;
    XSetWindowAttributes shell_attributes;
    XSetWindowAttributes body_attributes;
    unsigned long shell_attribute_mask;
    unsigned long body_attribute_mask;
    XGCValues gc_values;
    GC info_gc = None;
    const int width = 980;
    const int height = 800;
    const int startup_request_delay_ms = env_startup_request_delay_ms();
    const int use_titlebar = env_use_titlebar();
    const int titlebar_minmax = env_titlebar_minmax();
    const int min_width = 750;
    const int min_height = 450;
    const int titlebar_height = 46;
    const int close_button_width = 52;
    int requested_position = 0;
    struct rect target;
    struct app_state app;

    display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "Unable to open X display. DISPLAY=%s\n", getenv("DISPLAY"));
        return 1;
    }

    screen = DefaultScreen(display);
    root = RootWindow(display, screen);
    target = pick_target_rect(display, root, width, height);

    memset(&app, 0, sizeof(app));
    app.display = display;
    app.screen = screen;
    app.root = root;
    app.use_titlebar = use_titlebar;
    app.show_titlebar_minmax = titlebar_minmax;
    app.width = width;
    app.height = height;
    app.titlebar_height = titlebar_height;
    app.close_button_width = close_button_width;
    app.titlebar_color = alloc_named_color(display, screen, "gray20", BlackPixel(display, screen));
    app.close_button_color = alloc_named_color(display, screen, "gray28", BlackPixel(display, screen));

    memset(&shell_attributes, 0, sizeof(shell_attributes));
    shell_attributes.background_pixmap = None;
    shell_attributes.border_pixel = 0;
    shell_attributes.colormap = DefaultColormap(display, screen);
    shell_attribute_mask = CWBackPixmap | CWBorderPixel | CWColormap;

    app.window = XCreateWindow(display,
                               root,
                               0,
                               0,
                               (unsigned int)width,
                               (unsigned int)height,
                               0,
                               CopyFromParent,
                               InputOutput,
                               CopyFromParent,
                               shell_attribute_mask,
                               &shell_attributes);
    XStoreName(display, app.window, use_titlebar ? "window-test" : "");
    class_hint.res_name = (char *)"window-test";
    class_hint.res_class = (char *)"WindowTest";

    XSetClassHint(display, app.window, &class_hint);
    if (use_titlebar) {
        set_decorated_with_minmax(display, app.window);
    } else {
        set_undecorated(display, app.window);
    }

    memset(&size_hints, 0, sizeof(size_hints));
    size_hints.flags = PPosition | PSize | PMinSize;
    size_hints.x = 0;
    size_hints.y = 0;
    size_hints.width = width;
    size_hints.height = height;
    size_hints.min_width = min_width;
    size_hints.min_height = min_height;
    XSetWMNormalHints(display, app.window, &size_hints);

    memset(&body_attributes, 0, sizeof(body_attributes));
    body_attributes.background_pixel = WhitePixel(display, screen);
    body_attributes.border_pixel = 0;
    body_attribute_mask = CWBackPixel | CWBorderPixel;

    app.body_window = XCreateWindow(display,
                                    app.window,
                                    0,
                                    0,
                                    (unsigned int)width,
                                    (unsigned int)height,
                                    0,
                                    CopyFromParent,
                                    InputOutput,
                                    CopyFromParent,
                                    body_attribute_mask,
                                    &body_attributes);

    wm_delete = XInternAtom(display, "WM_DELETE_WINDOW", False);
    app.wm_delete = wm_delete;
    app.wm_change_state = XInternAtom(display, "WM_CHANGE_STATE", False);
    app.net_wm_state = XInternAtom(display, "_NET_WM_STATE", False);
    app.wm_maximized_vert = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    app.wm_maximized_horz = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    XSetWMProtocols(display, app.window, &wm_delete, 1);

    XSelectInput(display,
                 app.window,
                 ExposureMask | StructureNotifyMask | ButtonPressMask | ButtonReleaseMask | ButtonMotionMask);
    if (app.body_window != None) {
        XSelectInput(display,
                     app.body_window,
                     ExposureMask | ButtonPressMask | ButtonReleaseMask | ButtonMotionMask);
    }

    memset(&gc_values, 0, sizeof(gc_values));
    gc_values.foreground = WhitePixel(display, screen);
    gc_values.line_width = 2;
    info_gc = XCreateGC(display, app.window, GCForeground | GCLineWidth, &gc_values);
    app.close_gc = info_gc;

    layout_children(&app, width, height);

    XMapWindow(display, app.window);
    XFlush(display);
    if (app.body_window != None) {
        XMapWindow(display, app.body_window);
    }

    fprintf(stdout,
            "mapped raw X11 shell at 0,0 size=%dx%d; flushed shell before body map; titlebar=%d startup request in %d ms\n",
            width,
            height,
            use_titlebar,
            startup_request_delay_ms);
    fflush(stdout);

    for (;;) {
        if (!requested_position) {
            sleep_ms(startup_request_delay_ms);
            XMoveResizeWindow(display,
                              app.window,
                              target.x,
                              target.y,
                              (unsigned int)width,
                              (unsigned int)height);
            XFlush(display);
            fprintf(stdout,
                    "raw repro requested position %d,%d size=%dx%d\n",
                    target.x,
                    target.y,
                    width,
                    height);
            fflush(stdout);
            requested_position = 1;
        }

        XNextEvent(display, &event);

        if (event.type == ConfigureNotify && event.xconfigure.window == app.window) {
            app.window_x = event.xconfigure.x;
            app.window_y = event.xconfigure.y;
            layout_children(&app, event.xconfigure.width, event.xconfigure.height);
            fprintf(stdout,
                    "configure: x=%d y=%d width=%d height=%d\n",
                    event.xconfigure.x,
                    event.xconfigure.y,
                    event.xconfigure.width,
                    event.xconfigure.height);
            fflush(stdout);
        } else if (event.type == Expose) {
            if (!use_titlebar
                && (event.xexpose.window == app.window || event.xexpose.window == app.body_window)
                && event.xexpose.count == 0) {
                draw_close_button(&app);
            }
        } else if (event.type == ButtonPress) {
            if (!use_titlebar
                && (event.xbutton.window == app.window || event.xbutton.window == app.body_window)
                && event.xbutton.button == Button1
                && point_in_close_button(&app, event.xbutton.x, event.xbutton.y)) {
                break;
            }
            if (!use_titlebar
                && (event.xbutton.window == app.window || event.xbutton.window == app.body_window)
                && event.xbutton.button == Button1
                && point_in_maximize_button(&app, event.xbutton.x, event.xbutton.y)) {
                request_toggle_maximize(&app);
                continue;
            }
            if (!use_titlebar
                && (event.xbutton.window == app.window || event.xbutton.window == app.body_window)
                && event.xbutton.button == Button1
                && point_in_minimize_button(&app, event.xbutton.x, event.xbutton.y)) {
                request_minimize(&app);
                continue;
            }
            if (!use_titlebar
                && (event.xbutton.window == app.window || event.xbutton.window == app.body_window)
                && event.xbutton.button == Button1
                && point_in_titlebar(&app, event.xbutton.x, event.xbutton.y)) {
                begin_drag(&app, &event.xbutton);
            }
        } else if (event.type == MotionNotify) {
            if (!use_titlebar
                && (event.xmotion.window == app.window || event.xmotion.window == app.body_window)
                && app.dragging) {
                update_drag(&app, &event.xmotion);
            }
        } else if (event.type == ButtonRelease) {
            if (!use_titlebar
                && (event.xbutton.window == app.window || event.xbutton.window == app.body_window)
                && event.xbutton.button == Button1) {
                end_drag(&app, &event.xbutton);
            }
        } else if (event.type == ClientMessage) {
            if ((Atom)event.xclient.data.l[0] == wm_delete) {
                break;
            }
        } else if (event.type == DestroyNotify) {
            break;
        }
    }

    if (info_gc != None) {
        XFreeGC(display, info_gc);
    }
    if (app.body_window != None) {
        XDestroyWindow(display, app.body_window);
    }
    XDestroyWindow(display, app.window);
    XCloseDisplay(display);
    return 0;
}

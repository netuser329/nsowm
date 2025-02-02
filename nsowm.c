// nsowm - An itsy bitsy floating window manager.
#include "patches.h"
#if BARFS_PATCH
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif
#if ICCCM_PATCH
#include <X11/Xatom.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif
#include <X11/XF86keysym.h>
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#if ROUNDED_CORNERS_PATCH // ROUNDED_CORNERS_PATCH
#include <X11/extensions/shape.h>
#endif // ROUNDED_CORNERS_PATCH
#if TITLEBAR_PATCH
#include <X11/Xutil.h>
#include <string.h>
#endif
#if WINDOWS_PATCH
#include <stdbool.h>
#endif
#include "nsowm.h"

#if WINDOWS_PATCH
static client *list = {0}, *ws_list[NUM_WS] = {0}, *cur;
#else
static client *list = {0}, *ws_list[10] = {0}, *cur;
#endif
static int ws = 1, sw, sh, wx, wy, numlock = 0;
static unsigned int ww, wh;
#if LAST_WS_PATCH
static int last_ws = 1;
#endif

#if BORDER_PATCH
static int screen;
#endif
static Display *dpy;
static XButtonEvent mouse;

static Window root;

static void (*events[LASTEvent])(XEvent *e) = {
    [ButtonPress] = button_press,
    [ButtonRelease] = button_release,
    [ConfigureRequest] = configure_request,
    [KeyPress] = key_press,
    [MapRequest] = map_request,
    [MappingNotify] = mapping_notify,
    [DestroyNotify] = notify_destroy,
    [EnterNotify] = notify_enter,
    [MotionNotify] = notify_motion};

#include "config.h"

#if BORDER_PATCH
unsigned long getcolor(const char *col) {
  Colormap m = DefaultColormap(dpy, screen);
  XColor c;
  return (!XAllocNamedColor(dpy, m, col, &c, &c)) ? 0 : c.pixel;
}
#endif

#if BARFS_PATCH
void create_dir() {
#define barfs_dir "/tmp/barfs"
  struct stat state = {0};
  if (stat(barfs_dir, &state) == -1) {
    mkdir(barfs_dir, 0700);
  }
}

void fs_true() {
  create_dir();
  FILE *fp = fopen("/tmp/barfs/fs", "w");
  if (!fp)
    return;
  fprintf(fp, "1");
  fclose(fp);
}

void fs_false() {
  create_dir();
  FILE *fp = fopen("/tmp/barfs/fs", "w");
  if (!fp)
    return;
  fprintf(fp, "0");
  fclose(fp);
}
#endif

void win_focus(client *c) {
  cur = c;
  XSetInputFocus(dpy, cur->w, RevertToParent, CurrentTime);
#if BARFS_PATCH
  // checking if current window is fullscreen than writing it to the barfs file
  if (cur->f) {
    fs_true();
  } else {
    fs_false();
  }
#endif
}

void notify_destroy(XEvent *e) {
  win_del(e->xdestroywindow.window);

  if (list)
    win_focus(list->prev);
}

#if TITLEBAR_PATCH
void title_add(client *c) {
  if (c->t)
    return;

  XClassHint cl;
  XGetClassHint(dpy, c->w, &cl);

  if (!strcmp(cl.res_name, "no-title"))
    return;

  win_size(c->w, &wx, &wy, &ww, &wh);
#if BORDER_PATCH
  c->t = XCreateSimpleWindow(dpy, root, wx, wy - TH, ww + 2 * BORDER_WIDTH, TH,
                             0, TC, TC);
#else
  c->t = XCreateSimpleWindow(dpy, root, wx, wy - TH, ww, TH, 0, TC, TC);
#endif
  XMapWindow(dpy, c->t);
}

void title_del(client *c) {
  if (!c->t)
    return;

  XUnmapWindow(dpy, c->t);
  XDestroyWindow(dpy, c->t);
  c->t = 0;
}
#endif

void notify_enter(XEvent *e) {
  while (XCheckTypedEvent(dpy, EnterNotify, e))
    ;
#if BORDER_PATCH
  while (XCheckTypedWindowEvent(dpy, mouse.subwindow, MotionNotify, e))
    ;
#endif
#if TITLEBAR_PATCH
  if (mouse.subwindow == cur->t) {
    mouse.subwindow = cur->w;
    win_size(cur->w, &wx, &wy, &ww, &wh);
  }
#endif
    for
      win if (c->w == e->xcrossing.window) win_focus(c);
}

void notify_motion(XEvent *e) {
  if (!mouse.subwindow || cur->f)
    return;

  while (XCheckTypedEvent(dpy, MotionNotify, e))
    ;

  int xd = e->xbutton.x_root - mouse.x_root;
  int yd = e->xbutton.y_root - mouse.y_root;

  XMoveResizeWindow(dpy, mouse.subwindow,

                    wx + (mouse.button == 1 ? xd : 0),
                    wy + (mouse.button == 1 ? yd : 0),
                    MAX(1, ww + (mouse.button == 3 ? xd : 0)),
                    MAX(1, wh + (mouse.button == 3 ? yd : 0)));

#if TITLEBAR_PATCH
  if (cur->t)
    XMoveResizeWindow(
        dpy, cur->t,

        wx + (mouse.button == 1 ? xd : 0),
        wy + (mouse.button == 1 ? yd : 0) - TH,
#if BORDER_PATCH
        MAX(1, ww + 2 * BORDER_WIDTH + (mouse.button == 3 ? xd : 0)), TH);
#else
        MAX(1, ww + (mouse.button == 3 ? xd : 0)), TH);
#endif
#endif
#if ROUNDED_CORNERS_PATCH
  if (mouse.button == 3) {
    win_round_corners(mouse.subwindow, ROUND_CORNERS);
  }
#endif
}

void key_press(XEvent *e) {
  KeySym keysym = XkbKeycodeToKeysym(dpy, e->xkey.keycode, 0, 0);

  for (unsigned int i = 0; i < sizeof(keys) / sizeof(*keys); ++i)
    if (keys[i].keysym == keysym &&
        mod_clean(keys[i].mod) == mod_clean(e->xkey.state))
      keys[i].function(keys[i].arg);
}

void button_press(XEvent *e) {
  if (!e->xbutton.subwindow)
    return;

  win_size(e->xbutton.subwindow, &wx, &wy, &ww, &wh);
  XRaiseWindow(dpy, e->xbutton.subwindow);

  mouse = e->xbutton;
}

void button_release(XEvent *e) { mouse.subwindow = 0; }

void win_add(Window w) {
  client *c;

  if (!(c = (client *)calloc(1, sizeof(client))))
    exit(1);

  c->w = w;

  if (list) {
    list->prev->next = c;
    c->prev = list->prev;
    list->prev = c;
    c->next = list;

  } else {
    list = c;
    list->prev = list->next = list;
  }

  ws_save(ws);
}

void win_del(Window w) {
  client *x = 0;

    for
      win if (c->w == w) x = c;

    if (!list || !x)
      return;
    if (x->prev == x)
      list = 0;
    if (list == x)
      list = x->next;
    if (x->next)
      x->next->prev = x->prev;
    if (x->prev)
      x->prev->next = x->next;

#if TITLEBAR_PATCH
    title_del(x);
#endif
    free(x);
    ws_save(ws);
}

void win_kill(const Arg arg) {
#if NORMALKILL_PATCH
  if (!cur)
    return;

  XEvent ev = {.type = ClientMessage};

  ev.xclient.window = cur->w;
  ev.xclient.format = 32;
  ev.xclient.message_type = XInternAtom(dpy, "WM_PROTOCOLS", True);
  ev.xclient.data.l[0] = XInternAtom(dpy, "WM_DELETE_WINDOW", True);
  ev.xclient.data.l[1] = CurrentTime;

  XSendEvent(dpy, cur->w, False, NoEventMask, &ev);
#else
  if (cur)
    XKillClient(dpy, cur->w);
#endif
}

void win_center(const Arg arg) {
  if (!cur)
    return;

  win_size(cur->w, &(int){0}, &(int){0}, &ww, &wh);
  XMoveWindow(dpy, cur->w, (sw - ww) / 2, (sh - wh) / 2);
#if TITLEBAR_PATCH
  if (cur->t)
    XMoveWindow(dpy, cur->t, (sw - ww) / 2, (sh - wh - TH * 2) / 2);
#endif
}

void win_fs(const Arg arg) {
  if (!cur)
    return;

  if ((cur->f = cur->f ? 0 : 1)) {
    win_size(cur->w, &cur->wx, &cur->wy, &cur->ww, &cur->wh);
#if FIXBAR_PATCH
    XMoveResizeWindow(dpy, cur->w, 0, 0 + BAR_HEIGHT, sw, sh - BAR_HEIGHT);
#else
    XMoveResizeWindow(dpy, cur->w, 0, 0, sw, sh);
#endif
#if TITLEBAR_PATCH
    XRaiseWindow(dpy, cur->w);
    title_del(cur);
#endif
#if BARFS_PATCH
    fs_true();
#endif
  } else {
#if BARFS_PATCH
    fs_false();
#endif
    XMoveResizeWindow(dpy, cur->w, cur->wx, cur->wy, cur->ww, cur->wh);
#if TITLEBAR_PATCH
    title_add(cur);
#endif
  }
#if ROUNDED_CORNERS_PATCH
  win_round_corners(cur->w, cur->f ? 0 : ROUND_CORNERS);
#endif
}

#if RESIZEFULLSCREEN_PATCH
void win_resize_fullscreen(const Arg arg) {
  if (!cur)
    return;

  // if ((cur->f = cur->f ? 0 : 1)) {
  win_size(cur->w, &cur->wx, &cur->wy, &cur->ww, &cur->wh);
#if FIXBAR_PATCH
  XMoveResizeWindow(dpy, cur->w, 0, 0 + BAR_HEIGHT, sw, sh - BAR_HEIGHT);
#else
  XMoveResizeWindow(dpy, cur->w, 0, 0, sw, sh);
#endif
  //} else {
  //    ;
  //}
}

void wrf(const Arg arg) {
  win_resize_fullscreen(arg);
  win_resize_fullscreen(arg);
}
#endif

#if AUTOSTART_PATCH
void auto_start(void) {
  // add autostart programms here
  system("nitrogen --restore");
}
#endif

#if ROUNDED_CORNERS_PATCH
void win_round_corners(Window w, int rad) {
  unsigned int ww, wh, dia = 2 * rad;

  win_size(w, &(int){1}, &(int){1}, &ww, &wh);

  if (ww < dia || wh < dia)
    return;

  Pixmap mask = XCreatePixmap(dpy, w, ww, wh, 1);

  if (!mask)
    return;

  XGCValues xgcv;
  GC shape_gc = XCreateGC(dpy, mask, 0, &xgcv);

  if (!shape_gc) {
    XFreePixmap(dpy, mask);
    return;
  }

  XSetForeground(dpy, shape_gc, 0);
  XFillRectangle(dpy, mask, shape_gc, 0, 0, ww, wh);
  XSetForeground(dpy, shape_gc, 1);
  XFillArc(dpy, mask, shape_gc, 0, 0, dia, dia, 0, 23040);
  XFillArc(dpy, mask, shape_gc, ww - dia - 1, 0, dia, dia, 0, 23040);
  XFillArc(dpy, mask, shape_gc, 0, wh - dia - 1, dia, dia, 0, 23040);
  XFillArc(dpy, mask, shape_gc, ww - dia - 1, wh - dia - 1, dia, dia, 0, 23040);
  XFillRectangle(dpy, mask, shape_gc, rad, 0, ww - dia, wh);
  XFillRectangle(dpy, mask, shape_gc, 0, rad, ww, wh - dia);
  XShapeCombineMask(dpy, w, ShapeBounding, 0, 0, mask, ShapeSet);
  XFreePixmap(dpy, mask);
  XFreeGC(dpy, shape_gc);
}
#endif

#if RESIZE_PATCH
void win_move(const Arg arg) {
  int r = arg.com[0][0] == 'r';
  char m = arg.com[1][0];

  win_size(cur->w, &wx, &wy, &ww, &wh);

  XMoveResizeWindow(dpy, cur->w,
                    wx + (r          ? 0
                          : m == 'e' ? arg.i
                          : m == 'w' ? -arg.i
                                     : 0),
                    wy + (r          ? 0
                          : m == 'n' ? -arg.i
                          : m == 's' ? arg.i
                                     : 0),
                    MAX(10, ww + (r ? m == 'e'   ? arg.i
                                      : m == 'w' ? -arg.i
                                                 : 0
                                    : 0)),
                    MAX(10, wh + (r ? m == 'n'   ? -arg.i
                                      : m == 's' ? arg.i
                                                 : 0
                                    : 0)));
}
#endif

#if SPLIT_PATCH
void split_win(const Arg arg) {
  char m = arg.com[0][0];

  win_size(cur->w, &wx, &wy, &ww, &wh);

  XMoveResizeWindow(dpy, cur->w,
                    (m == 'w'   ? wx
                     : m == 'e' ? (wx + ww / 2)
                                : wx),
                    (m == 'n'   ? wy
                     : m == 's' ? (wy + wh / 2)
                                : wy),
                    (m == 'w'   ? (ww / 2)
                     : m == 'e' ? (ww / 2)
                                : ww),
                    (m == 'n'   ? (wh / 2)
                     : m == 's' ? (wh / 2)
                                : wh));
}
#endif

void win_to_ws(const Arg arg) {
  int tmp = ws;

  if (arg.i == tmp)
    return;

  ws_sel(arg.i);
  win_add(cur->w);
  ws_save(arg.i);
#if TITLEBAR_PATCH
  title_del(cur);
#endif

  ws_sel(tmp);
  win_del(cur->w);
  XUnmapWindow(dpy, cur->w);
  ws_save(tmp);

  if (list)
    win_focus(list);
}

void win_prev(const Arg arg) {
  if (!cur)
    return;
#if TITLEBAR_PATCH
  if (cur->prev->t)
    XRaiseWindow(dpy, cur->prev->t);
#endif
  XRaiseWindow(dpy, cur->prev->w);
  win_focus(cur->prev);
}

void win_next(const Arg arg) {
  if (!cur)
    return;

  XRaiseWindow(dpy, cur->next->w);

#if TITLEBAR_PATCH
  // if (cur->next->w)
  //;
  { XRaiseWindow(dpy, cur->next->t); }
#endif
  win_focus(cur->next);
}

void ws_go(const Arg arg) {

#if BARFS_PATCH
  create_dir();

  FILE *fp = fopen("/tmp/barfs/ws", "w");
  if (!fp)
    return;
  fprintf(fp, "%d", arg.i);
  fclose(fp);

  FILE *fp2 = fopen("/tmp/barfs/wscount", "w");
  if (!fp2)
    return;
  fprintf(fp2, "%d", WS_COUNT);
  fclose(fp2);

#endif

#if LAST_WS_PATCH
  last_ws = ws;
#endif

  int tmp = ws;

  if (arg.i == ws)
    return;

  ws_save(ws);
  ws_sel(arg.i);

#if !TITLEBAR_PATCH
    for
      win XMapWindow(dpy, c->w);
#endif
#if TITLEBAR_PATCH
    for
      win {
        XMapWindow(dpy, c->w);
        title_add(c);
      }
#endif
    ws_sel(tmp);

#if !TITLEBAR_PATCH
    for
      win XUnmapWindow(dpy, c->w);
#endif
#if TITLEBAR_PATCH
    for
      win {
        XUnmapWindow(dpy, c->w);
        title_del(c);
      }
#endif

    ws_sel(arg.i);

    if (list)
      win_focus(list);
    else
      cur = 0;
}

#if EXISTING_CLIENTS_PATCH
void win_init(void) {
  Window *child;
  unsigned int i, n_child;

  XQueryTree(dpy, RootWindow(dpy, DefaultScreen(dpy)), &(Window){0},
             &(Window){0}, &child, &n_child);

  for (i = 0; i < n_child; i++) {
    XSelectInput(dpy, child[i], StructureNotifyMask | EnterWindowMask);
    XMapWindow(dpy, child[i]);
    win_add(child[i]);
  }
  if (CYCLE_WS) {
    for (i = 1; i < WS_COUNT + 1; i++) {
      int current = ws;
      ws_go((Arg){.i = i});
      ws_go((Arg){.i = current});
    }
  }
}
#endif

#if NEXT_WS_PATCH
void ws_go_add(const Arg arg) {
  if (arg.i + ws > WS_COUNT || arg.i + ws < 1)
    return;
  ws_go((Arg){.i = arg.i + ws});
}
#endif

void configure_request(XEvent *e) {
  XConfigureRequestEvent *ev = &e->xconfigurerequest;

  XConfigureWindow(dpy, ev->window, ev->value_mask,
                   &(XWindowChanges){.x = ev->x,
                                     .y = ev->y,
                                     .width = ev->width,
                                     .height = ev->height,
                                     .sibling = ev->above,
                                     .stack_mode = ev->detail});
#if ROUNDED_CORNERS_PATCH
  win_round_corners(ev->window, ROUND_CORNERS);
#endif
}

#if WINDOWS_PATCH
bool exists_win(Window w) {
  int tmp = ws;
  for (int i = 0; i < NUM_WS; ++i) {
    if (i == tmp)
      continue;
    ws_sel(i);
        for
          win if (c->w == w) {
            ws_sel(tmp);
            return true;
          }
  }
  ws_sel(tmp);
  return false;
}
#endif

void map_request(XEvent *e) {
  Window w = e->xmaprequest.window;
#if WINDOWS_PATCH
  if (exists_win(w))
    return;
#endif
  XSelectInput(dpy, w, StructureNotifyMask | EnterWindowMask);
  win_size(w, &wx, &wy, &ww, &wh);
  win_add(w);
  cur = list->prev;
#if BORDER_PATCH
  XSetWindowBorder(dpy, w, getcolor(BORDER_COLOR));
  XConfigureWindow(dpy, w, CWBorderWidth,
                   &(XWindowChanges){.border_width = BORDER_WIDTH});
#endif
  if (wx + wy == 0)
    win_center((Arg){0});
#if ROUNDED_CORNERS_PATCH
  win_round_corners(w, ROUND_CORNERS);
#endif
  XMapWindow(dpy, w);
  win_focus(list->prev);
#if TITLEBAR_PATCH
  title_add(cur);
#endif
}

void mapping_notify(XEvent *e) {
  XMappingEvent *ev = &e->xmapping;

  if (ev->request == MappingKeyboard || ev->request == MappingModifier) {
    XRefreshKeyboardMapping(ev);
    input_grab(root);
  }
}

void quit(const Arg arg) { exit(arg.i); }

void run(const Arg arg) {
  if (fork())
    return;
  if (dpy)
    close(ConnectionNumber(dpy));

  setsid();
  execvp((char *)arg.com[0], (char **)arg.com);
}

void input_grab(Window root) {
  unsigned int i, j, modifiers[] = {0, LockMask, numlock, numlock | LockMask};
  XModifierKeymap *modmap = XGetModifierMapping(dpy);
  KeyCode code;

  for (i = 0; i < 8; i++)
    for (int k = 0; k < modmap->max_keypermod; k++)
      if (modmap->modifiermap[i * modmap->max_keypermod + k] ==
          XKeysymToKeycode(dpy, 0xff7f))
        numlock = (1 << i);

  XUngrabKey(dpy, AnyKey, AnyModifier, root);

  for (i = 0; i < sizeof(keys) / sizeof(*keys); i++)
    if ((code = XKeysymToKeycode(dpy, keys[i].keysym)))
      for (j = 0; j < sizeof(modifiers) / sizeof(*modifiers); j++)
        XGrabKey(dpy, code, keys[i].mod | modifiers[j], root, True,
                 GrabModeAsync, GrabModeAsync);

  for (i = 1; i < 4; i += 2)
    for (j = 0; j < sizeof(modifiers) / sizeof(*modifiers); j++)
      XGrabButton(dpy, i, MOD | modifiers[j], root, True,
                  ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                  GrabModeAsync, GrabModeAsync, 0, 0);

  XFreeModifiermap(modmap);
}
#if LAST_WS_PATCH
void last_ws_go(const Arg arg) { ws_go((Arg){.i = last_ws}); }
#endif

#if ICCCM_PATCH
// thanks to suckless for making implementing this suck less
int status, format;
unsigned char *data = NULL;
unsigned long n, extra;
Atom netwmcheck, netwmname, utf8_string, real, netwmfullscreen, netwmstate;

void icccm_setup() {
  netwmcheck = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
  netwmname = XInternAtom(dpy, "_NET_WM_NAME", False);
  utf8_string = XInternAtom(dpy, "UTF8_STRING", False);
  netwmfullscreen =
      XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False); // unused
  netwmstate = XInternAtom(dpy, "_NET_WM_STATE", False);   // unused
}
void set_wm_name(char *wm_name) {
  XChangeProperty(dpy, root, netwmcheck, XA_WINDOW, 32, PropModeReplace,
                  (unsigned char *)&root, 1);
  XChangeProperty(dpy, root, netwmname, utf8_string, 8, PropModeReplace,
                  (unsigned char *)wm_name, strlen(wm_name));
}
// fullscreen

#endif
int main(void) {
  XEvent ev;

  if (!(dpy = XOpenDisplay(0)))
    exit(1);

  signal(SIGCHLD, SIG_IGN);
  XSetErrorHandler(xerror);

#if !BORDER_PATCH
  int screen = DefaultScreen(dpy);
#endif

  root = RootWindow(dpy, screen);
#if BORDER_PATCH
  sw = XDisplayWidth(dpy, screen) - (2 * BORDER_WIDTH);
  sh = XDisplayHeight(dpy, screen) - (2 * BORDER_WIDTH);
#else
  sw = XDisplayWidth(dpy, screen);
  sh = XDisplayHeight(dpy, screen);
#endif
  XSelectInput(dpy, root, SubstructureRedirectMask);
  XDefineCursor(dpy, root, XCreateFontCursor(dpy, 68));
  input_grab(root);
#if EXISTING_CLIENTS_PATCH
  win_init();
#endif
#if AUTOSTART_PATCH
  auto_start();
#endif
#if ICCCM_PATCH
  icccm_setup();
  set_wm_name("nsowm"); // java bug thingy
#endif
  while (1 && !XNextEvent(dpy, &ev)) // 1 && will forever be here.
    if (events[ev.type])
      events[ev.type](&ev);
}

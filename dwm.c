#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */
#include <X11/Xft/Xft.h>

#include "drw.h"
#include "util.h"

/* macros */
#define BUTTONMASK (ButtonPressMask | ButtonReleaseMask)
#define CLEANMASK(mask) (mask & ~(numlockmask | LockMask) & (ShiftMask | ControlMask | Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask))
#define INTERSECT(x, y, w, h, m) (MAX(0, MIN((x) + (w), (m)->wx + (m)->ww) - MAX((x), (m)->wx)) * MAX(0, MIN((y) + (h), (m)->wy + (m)->wh) - MAX((y), (m)->wy)))
#define ISVISIBLE(C) ((C->tags & C->mon->tagset[C->mon->seltags]))
#define MOUSEMASK (BUTTONMASK | PointerMotionMask)

#define WIDTH(X) ((X)->w + 2 * (X)->bw)
#define HEIGHT(X) ((X)->h + 2 * (X)->bw)

#define HIDDEN(C) ((getstate(C->win) == IconicState))
#define NUMTAGS (LENGTH(tags) + LENGTH(scratchpads))
#define TAGMASK ((1 << NUMTAGS) - 1)
#define SPTAG(i) ((1 << LENGTH(tags)) << (i))
#define SPTAGMASK (((1 << LENGTH(scratchpads)) - 1) << LENGTH(tags))
#define TEXTW(X) (drw_fontset_getwidth(drw, (X)) + lrpad)

#define SYSTEM_TRAY_REQUEST_DOCK 0
/* XEMBED messages */
#define XEMBED_EMBEDDED_NOTIFY 0
#define XEMBED_WINDOW_ACTIVATE 1
#define XEMBED_FOCUS_IN 4
#define XEMBED_MODALITY_ON 10
#define XEMBED_MAPPED (1 << 0)
#define XEMBED_WINDOW_ACTIVATE 1
#define XEMBED_WINDOW_DEACTIVATE 2
#define VERSION_MAJOR 0
#define VERSION_MINOR 0
#define XEMBED_EMBEDDED_VERSION (VERSION_MAJOR << 16) | VERSION_MINOR

/* enums */
enum { CurNormal, CurResize, CurMove, CurLast }; /* cursor */
enum { SchemeNorm, SchemeHov, SchemeHid, SchemeSystray, SchemeSel }; /* color schemes */
enum {
	NetSupported,
	NetWMName,
	NetWMIcon,
	NetWMState,
	NetWMCheck,
	NetSystemTray,
	NetSystemTrayOP,
	NetSystemTrayOrientation,
	NetSystemTrayOrientationHorz,
	NetWMFullscreen,
	NetActiveWindow,
	NetWMWindowType,
	NetWMWindowTypeDialog,
	NetClientList,
	NetClientInfo,
	NetLast
}; /* EWMH atoms */
enum { Manager, Xembed, XembedInfo, XLast }; /* Xembed atoms */
enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMLast }; /* default atoms */
enum { ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, ClkRootWin, ClkLast }; /* clicks */

typedef union {
	int	     i;
	unsigned int ui;
	float	     f;
	const void  *v;
} Arg;

typedef struct {
	unsigned int click;
	unsigned int mask;
	unsigned int button;
	void (*func)(const Arg *arg);
	const Arg arg;
} Button;

typedef struct Monitor Monitor;
typedef struct Client  Client;
struct Client {
	char	     name[256];
	float	     mina, maxa;
	int	     x, y, w, h;
	int	     oldx, oldy, oldw, oldh;
	int	     basew, baseh, incw, inch, maxw, maxh, minw, minh, hintsvalid;
	int	     bw, oldbw;
	unsigned int tags;
	int	     isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen;
	unsigned int icw, ich;
	Picture	     icon;
	Client	    *next;
	Client	    *snext;
	Monitor	    *mon;
	Window	     win;
};

typedef struct {
	unsigned int mod;
	KeySym	     keysym;
	void (*func)(const Arg *);
	const Arg arg;
} Key;

typedef struct {
	const char *symbol;
	void (*arrange)(Monitor *);
} Layout;

typedef struct Pertag Pertag;
struct Monitor {
	char	      ltsymbol[16];
	float	      mfact;
	int	      nmaster;
	int	      num;
	int	      by; /* bar geometry */
	int	      btw; /* width of tasks portion of bar */
	int	      bt; /* number of tasks */
	int	      mx, my, mw, mh; /* screen size */
	int	      wx, wy, ww, wh; /* window area  */
	int	      gappx; /* gaps between windows */
	unsigned int  seltags;
	unsigned int  sellt;
	unsigned int  tagset[2];
	int	      showbar;
	int	      topbar;
	int	      hidsel;
	Client	     *clients;
	Client	     *sel;
	Client	     *hov;
	Client	     *stack;
	Monitor	     *next;
	Window	      barwin;
	const Layout *lt[2];
	Pertag	     *pertag;
};

typedef struct {
	const char *class;
	const char  *instance;
	const char  *title;
	unsigned int tags;
	int	     isfloating;
	int	     monitor;
} Rule;

typedef struct Systray Systray;
struct Systray {
	Window	win;
	Client *icons;
};

/* function declarations */
static void	    applyrules(Client *c);
static int	    applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact);
static void	    arrange(Monitor *m);
static void	    arrangemon(Monitor *m);
static void	    attach(Client *c);
static void	    attachstack(Client *c);
static void	    buttonpress(XEvent *e);
static void	    checkotherwm(void);
static void	    cleanup(void);
static void	    cleanupmon(Monitor *mon);
static void	    clientmessage(XEvent *e);
static void	    configure(Client *c);
static void	    configurenotify(XEvent *e);
static void	    configurerequest(XEvent *e);
static Monitor	   *createmon(void);
static void	    destroynotify(XEvent *e);
static void	    detach(Client *c);
static void	    detachstack(Client *c);
static Monitor	   *dirtomon(int dir);
static void	    drawbar(Monitor *m);
static void	    drawbars(void);
static int	    drawstatusbar(Monitor *m, int bh, char *text);
static void	    enternotify(XEvent *e);
static void	    expose(XEvent *e);
static void	    focus(Client *c);
static void	    focusin(XEvent *e);
static void	    focusmon(const Arg *arg);
static void	    focusstackvis(const Arg *arg);
static void	    focusstackhid(const Arg *arg);
static void	    focusstack(int inc, int vis);
static Atom	    getatomprop(Client *c, Atom prop);
static Picture	    geticonprop(Window w, unsigned int *icw, unsigned int *ich);
static int	    getrootptr(int *x, int *y);
static long	    getstate(Window w);
static unsigned int getsystraywidth();
static int	    gettextprop(Window w, Atom atom, char *text, unsigned int size);
static void	    grabbuttons(Client *c, int focused);
static void	    grabkeys(void);
static void	    hide(const Arg *arg);
static void	    hidewin(Client *c);
static void	    incnmaster(const Arg *arg);
static void	    keypress(XEvent *e);
static void	    killclient(const Arg *arg);
static void	    manage(Window w, XWindowAttributes *wa);
static void	    mappingnotify(XEvent *e);
static void	    maprequest(XEvent *e);
static void	    monocle(Monitor *m);
static void	    motionnotify(XEvent *e);
static void	    movemouse(const Arg *arg);
static Client	   *nexttiled(Client *c);
static void	    pop(Client *c);
static void	    propertynotify(XEvent *e);
static void	    quit(const Arg *arg);
static void	    removesystrayicon(Client *i);
static Monitor	   *recttomon(int x, int y, int w, int h);
static void	    resize(Client *c, int x, int y, int w, int h, int interact);
static void	    resizebarwin(Monitor *m);
static void	    resizeclient(Client *c, int x, int y, int w, int h);
static void	    resizerequest(XEvent *e);
static void	    resizemouse(const Arg *arg);
static void	    restack(Monitor *m);
static void	    run(void);
static void	    runAutostart(void);
static void	    scan(void);
static int	    sendevent(Window w, Atom proto, int m, long d0, long d1, long d2, long d3, long d4);
static void	    sendmon(Client *c, Monitor *m);
static void	    setclientstate(Client *c, long state);
static void	    setclienttagprop(Client *c);
static void	    setfocus(Client *c);
static void	    setfullscreen(Client *c, int fullscreen);
static void	    setgaps(const Arg *arg);
static void	    setlayout(const Arg *arg);
static void	    setmfact(const Arg *arg);
static void	    setup(void);
static void	    seturgent(Client *c, int urg);
static void	    show(const Arg *arg);
static void	    showall(const Arg *arg);
static void	    showwin(Client *c);
static void	    showhide(Client *c);
static void	    sighup(int unused);
static void	    sigterm(int unused);
static void	    spawn(const Arg *arg);
static Monitor	   *systraytomon(Monitor *m);
static void	    tag(const Arg *arg);
static void	    tagmon(const Arg *arg);
static void	    tile(Monitor *m);
static void	    togglebar(const Arg *arg);
void		    center_window(Client *c);
static void	    togglefloating(const Arg *arg);
static void	    togglescratch(const Arg *arg);
static void	    toggletag(const Arg *arg);
static void	    toggleview(const Arg *arg);
static void	    freeicon(Client *c);
static void	    togglewin(const Arg *arg);
static void	    unfocus(Client *c, int setfocus);
static void	    unmanage(Client *c, int destroyed);
static void	    unmapnotify(XEvent *e);
static void	    updatebarpos(Monitor *m);
static void	    updatebars(void);
static void	    updateclientlist(void);
static int	    updategeom(void);
static void	    updatenumlockmask(void);
static void	    updatesizehints(Client *c);
static void	    updatestatus(void);
static void	    updatesystray(void);
static void	    updatesystrayicongeom(Client *i, int w, int h);
static void	    updatesystrayiconstate(Client *i, XPropertyEvent *ev);
static void	    updatetitle(Client *c);
static void	    updateicon(Client *c);
static void	    updatewindowtype(Client *c);
static void	    updatewmhints(Client *c);
static void	    view(const Arg *arg);
static Client	   *wintoclient(Window w);
static Monitor	   *wintomon(Window w);
static Client	   *wintosystrayicon(Window w);
static int	    xerror(Display *dpy, XErrorEvent *ee);
static int	    xerrordummy(Display *dpy, XErrorEvent *ee);
static int	    xerrorstart(Display *dpy, XErrorEvent *ee);
static void	    zoom(const Arg *arg);

/* variables */
static Systray	 *systray = NULL;
static const char broken[] = "broken";
static char	  stext[1024];
static int	  screen;
static int	  sw, sh; /* X display screen geometry width, height */
static int	  bh; /* bar height */
static int	  lrpad; /* sum of left and right padding for text */
static int	  vp; /* vertical padding for bar */
static int	  sp; /* side padding for bar */
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int numlockmask = 0;
static void (*handler[LASTEvent])(XEvent *) = { [ButtonPress] = buttonpress,
						[ClientMessage] = clientmessage,
						[ConfigureRequest] = configurerequest,
						[ConfigureNotify] = configurenotify,
						[DestroyNotify] = destroynotify,
						[EnterNotify] = enternotify,
						[Expose] = expose,
						[FocusIn] = focusin,
						[KeyPress] = keypress,
						[MappingNotify] = mappingnotify,
						[MapRequest] = maprequest,
						[MotionNotify] = motionnotify,
						[PropertyNotify] = propertynotify,
						[ResizeRequest] = resizerequest,
						[UnmapNotify] = unmapnotify };
static Atom	wmatom[WMLast], netatom[NetLast], xatom[XLast];
static int	restart = 0;
static int	running = 1;
static Cur     *cursor[CurLast];
static Clr    **scheme;
static Clr    **tagscheme;
static Display *dpy;
static Drw     *drw;
static Monitor *mons, *selmon;
static Window	root, wmcheckwin;

/* configuration, allows nested code to access above variables */
#include "config.h"

struct Pertag {
	unsigned int  curtag, prevtag; /* current and previous tag */
	int	      nmasters[LENGTH(tags) + 1]; /* number of windows in master area */
	float	      mfacts[LENGTH(tags) + 1]; /* mfacts per tag */
	unsigned int  sellts[LENGTH(tags) + 1]; /* selected layouts */
	const Layout *ltidxs[LENGTH(tags) + 1][2]; /* matrix of tags and layouts indexes  */
	int	      showbars[LENGTH(tags) + 1]; /* display bar for the current tag */
};

/* compile-time check if all tags fit into an unsigned int bit array. */
struct NumTags {
	char limitexceeded[LENGTH(tags) > 31 ? -1 : 1];
};

/* function implementations */
void applyrules(Client *c)
{
	const char *class, *instance;
	unsigned int i;
	const Rule  *r;
	Monitor	    *m;
	XClassHint   ch = { NULL, NULL };

	/* rule matching */
	c->isfloating = 0;
	c->tags = 0;
	XGetClassHint(dpy, c->win, &ch);
	class = ch.res_class ? ch.res_class : broken;
	instance = ch.res_name ? ch.res_name : broken;

	for (i = 0; i < LENGTH(rules); i++) {
		r = &rules[i];
		if ((!r->title || strstr(c->name, r->title)) && (!r->class || strstr(class, r->class)) && (!r->instance || strstr(instance, r->instance))) {
			c->isfloating = r->isfloating;
			c->tags |= r->tags;
			if ((r->tags & SPTAGMASK) && r->isfloating) {
				c->x = c->mon->wx + (c->mon->ww / 2 - WIDTH(c) / 2);
				c->y = c->mon->wy + (c->mon->wh / 2 - HEIGHT(c) / 2);
			}

			for (m = mons; m && m->num != r->monitor; m = m->next)
				;
			if (m)
				c->mon = m;
		}
	}
	if (ch.res_class)
		XFree(ch.res_class);
	if (ch.res_name)
		XFree(ch.res_name);
	c->tags = c->tags & TAGMASK ? c->tags & TAGMASK : (c->mon->tagset[c->mon->seltags] & ~SPTAGMASK);
}

int applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact)
{
	int	 baseismin;
	Monitor *m = c->mon;

	/* set minimum possible */
	*w = MAX(1, *w);
	*h = MAX(1, *h);
	if (interact) {
		if (*x > sw)
			*x = sw - WIDTH(c);
		if (*y > sh)
			*y = sh - HEIGHT(c);
		if (*x + *w + 2 * c->bw < 0)
			*x = 0;
		if (*y + *h + 2 * c->bw < 0)
			*y = 0;
	} else {
		if (*x >= m->wx + m->ww)
			*x = m->wx + m->ww - WIDTH(c);
		if (*y >= m->wy + m->wh)
			*y = m->wy + m->wh - HEIGHT(c);
		if (*x + *w + 2 * c->bw <= m->wx)
			*x = m->wx;
		if (*y + *h + 2 * c->bw <= m->wy)
			*y = m->wy;
	}
	if (*h < bh)
		*h = bh;
	if (*w < bh)
		*w = bh;
	if (resizehints || c->isfloating || !c->mon->lt[c->mon->sellt]->arrange) {
		if (!c->hintsvalid)
			updatesizehints(c);
		/* see last two sentences in ICCCM 4.1.2.3 */
		baseismin = c->basew == c->minw && c->baseh == c->minh;
		if (!baseismin) { /* temporarily remove base dimensions */
			*w -= c->basew;
			*h -= c->baseh;
		}
		/* adjust for aspect limits */
		if (c->mina > 0 && c->maxa > 0) {
			if (c->maxa < (float)*w / *h)
				*w = *h * c->maxa + 0.5;
			else if (c->mina < (float)*h / *w)
				*h = *w * c->mina + 0.5;
		}
		if (baseismin) { /* increment calculation requires this */
			*w -= c->basew;
			*h -= c->baseh;
		}
		/* adjust for increment value */
		if (c->incw)
			*w -= *w % c->incw;
		if (c->inch)
			*h -= *h % c->inch;
		/* restore base dimensions */
		*w = MAX(*w + c->basew, c->minw);
		*h = MAX(*h + c->baseh, c->minh);
		if (c->maxw)
			*w = MIN(*w, c->maxw);
		if (c->maxh)
			*h = MIN(*h, c->maxh);
	}
	return *x != c->x || *y != c->y || *w != c->w || *h != c->h;
}

void arrange(Monitor *m)
{
	if (m)
		showhide(m->stack);
	else
		for (m = mons; m; m = m->next)
			showhide(m->stack);
	if (m) {
		arrangemon(m);
		restack(m);
	} else
		for (m = mons; m; m = m->next)
			arrangemon(m);
}

void arrangemon(Monitor *m)
{
	strncpy(m->ltsymbol, m->lt[m->sellt]->symbol, sizeof m->ltsymbol);
	if (m->lt[m->sellt]->arrange)
		m->lt[m->sellt]->arrange(m);
}

void attach(Client *c)
{
	if (!newclientathead) {
		Client **tc;
		for (tc = &c->mon->clients; *tc; tc = &(*tc)->next)
			;
		*tc = c;
		c->next = NULL;
	} else {
		c->next = c->mon->clients;
		c->mon->clients = c;
	}
}

void attachstack(Client *c)
{
	c->snext = c->mon->stack;
	c->mon->stack = c;
}

void buttonpress(XEvent *e)
{
	unsigned int	     i, x, click;
	Arg		     arg = { 0 };
	Client		    *c;
	Monitor		    *m;
	XButtonPressedEvent *ev = &e->xbutton;

	click = ClkRootWin;
	/* focus monitor if necessary */
	if ((m = wintomon(ev->window)) && m != selmon) {
		unfocus(selmon->sel, 1);
		selmon = m;
		focus(NULL);
	}
	if (ev->window == selmon->barwin) {
		i = x = 0;
		do
			x += TEXTW(tags[i]);
		while (ev->x >= x && ++i < LENGTH(tags));
		if (i < LENGTH(tags)) {
			click = ClkTagBar;
			arg.ui = 1 << i;
		} else if (ev->x < x + TEXTW(selmon->ltsymbol))
			click = ClkLtSymbol;
		/* 2px right padding */
		// else if (ev->x > selmon->ww - (int)TEXTW(stext) - getsystraywidth()) bu gan du
		// else if (ev->x > selmon->ww - TEXTW(stext) + lrpad - 2 - getsystraywidth())
		else if (ev->x > drawstatusbar(selmon, bh, stext) - getsystraywidth())
			click = ClkStatusText;
		else {
			x += TEXTW(selmon->ltsymbol);
			c = m->clients;
			int tabw = m->btw / m->bt;

			if (c) {
				do {
					if (!ISVISIBLE(c))
						continue;
					else
						// x += (1.0 / (double)m->bt) * m->btw;
						x += tabw;
				} while (ev->x > x && (c = c->next));

				click = ClkWinTitle;
				arg.v = c;
			}
		}
	} else if ((c = wintoclient(ev->window))) {
		focus(c);
		restack(selmon);
		XAllowEvents(dpy, ReplayPointer, CurrentTime);
		click = ClkClientWin;
	}
	for (i = 0; i < LENGTH(buttons); i++)
		if (click == buttons[i].click && buttons[i].func && buttons[i].button == ev->button && CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state))
			buttons[i].func((click == ClkTagBar || click == ClkWinTitle) && buttons[i].arg.i == 0 ? &arg : &buttons[i].arg);
}

void checkotherwm(void)
{
	xerrorxlib = XSetErrorHandler(xerrorstart);
	/* this causes an error if some other window manager is running */
	XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
	XSync(dpy, False);
	XSetErrorHandler(xerror);
	XSync(dpy, False);
}

void cleanup(void)
{
	Arg	 a = { .ui = ~0 };
	Layout	 foo = { "", NULL };
	Monitor *m;
	size_t	 i;

	view(&a);
	selmon->lt[selmon->sellt] = &foo;
	for (m = mons; m; m = m->next)
		while (m->stack)
			unmanage(m->stack, 0);
	XUngrabKey(dpy, AnyKey, AnyModifier, root);
	while (mons)
		cleanupmon(mons);

	if (showsystray) {
		XUnmapWindow(dpy, systray->win);
		XDestroyWindow(dpy, systray->win);
		free(systray);
	}

	for (i = 0; i < CurLast; i++)
		drw_cur_free(drw, cursor[i]);
	for (i = 0; i < LENGTH(colors) + 1; i++)
		free(scheme[i]);
	free(scheme);
	XDestroyWindow(dpy, wmcheckwin);
	drw_free(drw);
	XSync(dpy, False);
	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
}

void cleanupmon(Monitor *mon)
{
	Monitor *m;

	if (mon == mons)
		mons = mons->next;
	else {
		for (m = mons; m && m->next != mon; m = m->next)
			;
		m->next = mon->next;
	}
	XUnmapWindow(dpy, mon->barwin);
	XDestroyWindow(dpy, mon->barwin);
	free(mon);
}

void clientmessage(XEvent *e)
{
	XWindowAttributes    wa;
	XSetWindowAttributes swa;
	XClientMessageEvent *cme = &e->xclient;
	Client		    *c = wintoclient(cme->window);
	unsigned int	     i;

	if (showsystray && cme->window == systray->win && cme->message_type == netatom[NetSystemTrayOP]) {
		/* add systray icons */
		if (cme->data.l[1] == SYSTEM_TRAY_REQUEST_DOCK) {
			if (!(c = (Client *)calloc(1, sizeof(Client))))
				die("fatal: could not malloc() %u bytes\n", sizeof(Client));
			if (!(c->win = cme->data.l[2])) {
				free(c);
				return;
			}
			c->mon = selmon;
			c->next = systray->icons;
			systray->icons = c;
			if (!XGetWindowAttributes(dpy, c->win, &wa)) {
				/* use sane defaults */
				wa.width = bh;
				wa.height = bh;
				wa.border_width = 0;
			}
			c->x = c->oldx = c->y = c->oldy = 0;
			c->w = c->oldw = wa.width;
			c->h = c->oldh = wa.height;
			c->oldbw = wa.border_width;
			c->bw = 0;
			c->isfloating = True;
			/* reuse tags field as mapped status */
			c->tags = 1;
			updatesizehints(c);
			updatesystrayicongeom(c, wa.width, wa.height);
			XAddToSaveSet(dpy, c->win);
			XSelectInput(dpy, c->win, StructureNotifyMask | PropertyChangeMask | ResizeRedirectMask);
			XReparentWindow(dpy, c->win, systray->win, 0, 0);
			/* use parents background color */
			swa.background_pixel = scheme[SchemeNorm][ColBg].pixel;
			XChangeWindowAttributes(dpy, c->win, CWBackPixel, &swa);
			sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_EMBEDDED_NOTIFY, 0, systray->win, XEMBED_EMBEDDED_VERSION);
			/* FIXME not sure if I have to send these events, too */
			sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_FOCUS_IN, 0, systray->win, XEMBED_EMBEDDED_VERSION);
			sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_WINDOW_ACTIVATE, 0, systray->win, XEMBED_EMBEDDED_VERSION);
			sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_MODALITY_ON, 0, systray->win, XEMBED_EMBEDDED_VERSION);
			XSync(dpy, False);
			resizebarwin(selmon);
			updatesystray();
			setclientstate(c, NormalState);
		}
		return;
	}

	if (!c)
		return;
	if (cme->message_type == netatom[NetWMState]) {
		if (cme->data.l[1] == netatom[NetWMFullscreen] || cme->data.l[2] == netatom[NetWMFullscreen])
			setfullscreen(c, (cme->data.l[0] == 1 /* _NET_WM_STATE_ADD    */
					  || (cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */ && !c->isfullscreen)));
	} else if (cme->message_type == netatom[NetActiveWindow]) {
		for (i = 0; i < LENGTH(tags) && !((1 << i) & c->tags); i++)
			;
		if (i < LENGTH(tags)) {
			const Arg a = { .ui = 1 << i };
			selmon = c->mon;
			view(&a);
			focus(c);
			restack(selmon);
		}
	}
}

void configure(Client *c)
{
	XConfigureEvent ce;

	ce.type = ConfigureNotify;
	ce.display = dpy;
	ce.event = c->win;
	ce.window = c->win;
	ce.x = c->x;
	ce.y = c->y;
	ce.width = c->w;
	ce.height = c->h;
	ce.border_width = c->bw;
	ce.above = None;
	ce.override_redirect = False;
	XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *)&ce);
}

void configurenotify(XEvent *e)
{
	Monitor		*m;
	Client		*c;
	XConfigureEvent *ev = &e->xconfigure;
	int		 dirty;

	/* TODO: updategeom handling sucks, needs to be simplified */
	if (ev->window == root) {
		dirty = (sw != ev->width || sh != ev->height);
		sw = ev->width;
		sh = ev->height;
		if (updategeom() || dirty) {
			drw_resize(drw, sw, bh);
			updatebars();
			for (m = mons; m; m = m->next) {
				for (c = m->clients; c; c = c->next)
					if (c->isfullscreen)
						resizeclient(c, m->mx, m->my, m->mw, m->mh);
				resizebarwin(m);
			}
			focus(NULL);
			arrange(NULL);
		}
	}
}

void configurerequest(XEvent *e)
{
	Client		       *c;
	Monitor		       *m;
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	XWindowChanges		wc;

	if ((c = wintoclient(ev->window))) {
		if (ev->value_mask & CWBorderWidth)
			c->bw = ev->border_width;
		else if (c->isfloating || !selmon->lt[selmon->sellt]->arrange) {
			m = c->mon;
			if (ev->value_mask & CWX) {
				c->oldx = c->x;
				c->x = m->mx + ev->x;
			}
			if (ev->value_mask & CWY) {
				c->oldy = c->y;
				c->y = m->my + ev->y;
			}
			if (ev->value_mask & CWWidth) {
				c->oldw = c->w;
				c->w = ev->width;
			}
			if (ev->value_mask & CWHeight) {
				c->oldh = c->h;
				c->h = ev->height;
			}
			if ((c->x + c->w) > m->mx + m->mw && c->isfloating)
				c->x = m->mx + (m->mw / 2 - WIDTH(c) / 2); /* center in x direction */
			if ((c->y + c->h) > m->my + m->mh && c->isfloating)
				c->y = m->my + (m->mh / 2 - HEIGHT(c) / 2); /* center in y direction */
			if ((ev->value_mask & (CWX | CWY)) && !(ev->value_mask & (CWWidth | CWHeight)))
				configure(c);
			if (ISVISIBLE(c))
				XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
		} else
			configure(c);
	} else {
		wc.x = ev->x;
		wc.y = ev->y;
		wc.width = ev->width;
		wc.height = ev->height;
		wc.border_width = ev->border_width;
		wc.sibling = ev->above;
		wc.stack_mode = ev->detail;
		XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
	}
	XSync(dpy, False);
}

Monitor *createmon(void)
{
	Monitor	    *m;
	unsigned int i;

	m = ecalloc(1, sizeof(Monitor));
	m->tagset[0] = m->tagset[1] = 1;
	m->mfact = mfact;
	m->nmaster = nmaster;
	m->showbar = showbar;
	m->topbar = topbar;
	m->gappx = gappx;
	m->lt[0] = &layouts[0];
	m->lt[1] = &layouts[1 % LENGTH(layouts)];
	strncpy(m->ltsymbol, layouts[0].symbol, sizeof m->ltsymbol);
	m->pertag = ecalloc(1, sizeof(Pertag));
	m->pertag->curtag = m->pertag->prevtag = 1;

	for (i = 0; i <= LENGTH(tags); i++) {
		m->pertag->nmasters[i] = m->nmaster;
		m->pertag->mfacts[i] = m->mfact;

		m->pertag->ltidxs[i][0] = m->lt[0];
		m->pertag->ltidxs[i][1] = m->lt[1];
		m->pertag->sellts[i] = m->sellt;

		m->pertag->showbars[i] = m->showbar;
	}

	return m;
}

void destroynotify(XEvent *e)
{
	Client		    *c;
	XDestroyWindowEvent *ev = &e->xdestroywindow;

	if ((c = wintoclient(ev->window)))
		unmanage(c, 1);
	else if ((c = wintosystrayicon(ev->window))) {
		removesystrayicon(c);
		resizebarwin(selmon);
		updatesystray();
	}
}

void detach(Client *c)
{
	Client **tc;

	for (tc = &c->mon->clients; *tc && *tc != c; tc = &(*tc)->next)
		;
	*tc = c->next;
}

void detachstack(Client *c)
{
	Client **tc, *t;

	for (tc = &c->mon->stack; *tc && *tc != c; tc = &(*tc)->snext)
		;
	*tc = c->snext;

	if (c == c->mon->sel) {
		for (t = c->mon->stack; t && !ISVISIBLE(t); t = t->snext)
			;
		c->mon->sel = t;
	}
}

Monitor *dirtomon(int dir)
{
	Monitor *m = NULL;

	if (dir > 0) {
		if (!(m = selmon->next))
			m = mons;
	} else if (selmon == mons)
		for (m = mons; m->next; m = m->next)
			;
	else
		for (m = mons; m->next != selmon; m = m->next)
			;
	return m;
}

void drawbar(Monitor *m)
{
	int x, w, tw = 0, stw = 0, scm, n = 0;
	// int x, w, tw = 0, n = 0, scm; AGR__A
	int	     boxs = drw->fonts->h / 9;
	int	     boxw = drw->fonts->h / 6 + 2;
	unsigned int i, occ = 0, urg = 0;
	Client	    *c;

	drw_setscheme(drw, scheme[SchemeNorm]);
	drw_rect(drw, 0, 0, m->ww - stw, bh, 1, 1);
	if (!m->showbar)
		return;

	if (showsystray && m == systraytomon(m) && !systrayonleft)
		stw = getsystraywidth();

	/* draw status first so it can be overdrawn by tags later */
	if (m == selmon) { /* status is only drawn on selected monitor */
		tw = m->ww - drawstatusbar(m, bh, stext); /// deng hui
	}

	resizebarwin(m);
	for (c = m->clients; c; c = c->next) {
		if (ISVISIBLE(c))
			n++;
		occ |= c->tags;
		if (c->isurgent)
			urg |= c->tags;
	}
	x = 0;
	for (i = 0; i < LENGTH(tags); i++) {
		w = TEXTW(tags[i]);
		// drw_setscheme(drw, scheme[m->tagset[m->seltags] & 1 << i ? SchemeSel : SchemeNorm]);
		drw_setscheme(drw, (m->tagset[m->seltags] & 1 << i ? tagscheme[i] : scheme[SchemeNorm]));
		drw_text(drw, x, 0, w, bh, lrpad / 2, tags[i], urg & 1 << i);
		if (ulineall || m->tagset[m->seltags] &
					1 << i) /* if there are conflicts, just move these lines directly underneath both 'drw_setscheme' and 'drw_text' :) */
			drw_rect(drw, x + ulinepad, bh - ulinestroke - ulinevoffset, w - (ulinepad * 2), ulinestroke, 1, 0);
		if (occ & 1 << i)
			drw_rect(drw, x + boxs, boxs, boxw, boxw, m == selmon && selmon->sel && selmon->sel->tags & 1 << i, urg & 1 << i);
		x += w;
	}
	w = TEXTW(m->ltsymbol);
	drw_setscheme(drw, scheme[SchemeNorm]);
	x = drw_text(drw, x, 0, w, bh, lrpad / 2, m->ltsymbol, 0);
	if ((w = m->ww - tw - stw - x) > bh) {
		if (n > 0) {
			// if (m->sel) {
			int remainder = w % n;
			// int tabw = (1.0 / (double)n) * w + 1;
			int tabw = w / n;
			for (c = m->clients; c; c = c->next) {
				if (!ISVISIBLE(c))
					continue;
				if (m->hov == c)
					scm = SchemeHov;
				else if (m->sel == c)
					scm = SchemeSel;
				else if (HIDDEN(c))
					scm = SchemeHid;
				else
					scm = SchemeNorm;
				drw_setscheme(drw, scheme[scm]);

				drw_text(drw, x, 0, tabw - 2 * sp, bh, lrpad / 2 + (c->icon ? c->icw + ICONSPACING : 0), c->name, 0);

				if (remainder >= 0) {
					if (remainder == 0) {
						tabw--;
					}
					remainder--;
				}

				if (c->icon) // 改为c->icon
					drw_pic(drw, x + lrpad / 2,
						(bh - c->ich) / 2, // 改为c->ich
						c->icw,
						c->ich, // 改为c->icw和c->ich
						c->icon); // 改为c->icon

				if (c->isfloating) // 改为c->isfloating
					drw_rect(drw, x + boxs, boxs, boxw, boxw, c->isfixed, 0);

				x += tabw;
			}
		}
	} else {
		drw_setscheme(drw, scheme[SchemeNorm]);
		drw_rect(drw, x, 0, m->ww - stw - x - 2 * sp, bh, 1, 1);
	}
	m->bt = n;
	m->btw = w;
	drw_map(drw, m->barwin, 0, 0, m->ww - stw, bh);
}

void drawbars(void)
{
	Monitor *m;

	for (m = mons; m; m = m->next)
		drawbar(m);
}
int drawstatusbar(Monitor *m, int bh, char *stext)
{
	int   ret, i, w, x, len;
	short isCode = 0;
	char *text;
	char *p;

	len = strlen(stext) + 1;
	if (!(text = (char *)malloc(sizeof(char) * len)))
		die("malloc");
	p = text;
	memcpy(text, stext, len);

	/* compute width of the status text */
	w = 0;
	i = -1;
	while (text[++i]) {
		if (text[i] == '^') {
			if (!isCode) {
				isCode = 1;
				text[i] = '\0';
				w += TEXTW(text) - lrpad;
				text[i] = '^';
				if (text[++i] == 'f')
					w += atoi(text + ++i);
			} else {
				isCode = 0;
				text = text + i + 1;
				i = -1;
			}
		}
	}
	if (!isCode)
		w += TEXTW(text) - lrpad;
	else
		isCode = 0;
	text = p;

	// w += 2; /* 1px padding on both sides */
	w += lrpad;
	ret = x = m->ww - w;
	x = m->ww - w - getsystraywidth();

	drw_setscheme(drw, scheme[LENGTH(colors)]);
	drw->scheme[ColFg] = scheme[SchemeNorm][ColFg];
	drw->scheme[ColBg] = scheme[SchemeNorm][ColBg];
	drw_rect(drw, x, 0, w, bh, 1, 1);
	// x++;
	x += lrpad / 2;

	/* process status text */
	i = -1;
	while (text[++i]) {
		if (text[i] == '^' && !isCode) {
			isCode = 1;

			text[i] = '\0';
			w = TEXTW(text) - lrpad;
			// drw_text(drw, x, 0, w, bh, 0, text, 0);
			drw_text(drw, x - 2 * sp, vertpadbar / 2, w, bh - vertpadbar, 0, text, 0);

			x += w;
			/* process code */
			while (text[++i] != '^') {
				if (text[i] == 'c') {
					char buf[8];
					memcpy(buf, (char *)text + i + 1, 7);
					buf[7] = '\0';
					drw_clr_create(drw, &drw->scheme[ColFg], buf);
					i += 7;
				} else if (text[i] == 'b') {
					char buf[8];
					memcpy(buf, (char *)text + i + 1, 7);
					buf[7] = '\0';
					drw_clr_create(drw, &drw->scheme[ColBg], buf);
					i += 7;
				} else if (text[i] == 'd') {
					drw->scheme[ColFg] = scheme[SchemeNorm][ColFg];
					drw->scheme[ColBg] = scheme[SchemeNorm][ColBg];
				} else if (text[i] == 'r') {
					int rx = atoi(text + ++i);
					while (text[++i] != ',')
						;
					int ry = atoi(text + ++i);
					while (text[++i] != ',')
						;
					int rw = atoi(text + ++i);
					while (text[++i] != ',')
						;
					int rh = atoi(text + ++i);

					// drw_rect(drw, rx + x, ry, rw, rh, 1, 0);
					drw_rect(drw, rx + x, ry + vertpadbar / 2, rw, MIN(rh, bh - vertpadbar), 1, 0);
				} else if (text[i] == 'f') {
					x += atoi(text + ++i);
				}
			}

			text = text + i + 1;
			i = -1;
			isCode = 0;
		}
	}

	if (!isCode) {
		w = TEXTW(text) - lrpad;
		drw_text(drw, x, 0, w, bh, 0, text, 0);
	}

	drw_setscheme(drw, scheme[SchemeNorm]);
	free(p);

	return ret;
}

void enternotify(XEvent *e)
{
	Client	       *c;
	Monitor	       *m;
	XCrossingEvent *ev = &e->xcrossing;

	if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) && ev->window != root)
		return;
	c = wintoclient(ev->window);
	m = c ? c->mon : wintomon(ev->window);
	if (m != selmon) {
		unfocus(selmon->sel, 1);
		selmon = m;
	} else if (!c || c == selmon->sel)
		return;
	focus(c);
}

void expose(XEvent *e)
{
	Monitor	     *m;
	XExposeEvent *ev = &e->xexpose;

	if (ev->count == 0 && (m = wintomon(ev->window))) {
		drawbar(m);
		if (m == selmon)
			updatesystray();
	}
}

void focus(Client *c)
{
	if (!c || !ISVISIBLE(c))
		for (c = selmon->stack; c && (!ISVISIBLE(c) || HIDDEN(c)); c = c->snext)
			;
	if (selmon->sel && selmon->sel != c) {
		unfocus(selmon->sel, 0);

		if (selmon->hidsel) {
			hidewin(selmon->sel);
			if (c)
				arrange(c->mon);
			selmon->hidsel = 0;
		}
	}
	if (c) {
		if (c->mon != selmon)
			selmon = c->mon;
		if (c->isurgent)
			seturgent(c, 0);
		detachstack(c);
		attachstack(c);
		grabbuttons(c, 1);
		XSetWindowBorder(dpy, c->win, scheme[SchemeSel][ColBorder].pixel);
		setfocus(c);
	} else {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
	selmon->sel = c;
	drawbars();
}

/* there are some broken focus acquiring clients needing extra handling */
void focusin(XEvent *e)
{
	XFocusChangeEvent *ev = &e->xfocus;

	if (selmon->sel && ev->window != selmon->sel->win)
		setfocus(selmon->sel);
}

void focusmon(const Arg *arg)
{
	Monitor *m;

	if (!mons->next)
		return;
	if ((m = dirtomon(arg->i)) == selmon)
		return;
	unfocus(selmon->sel, 0);
	selmon = m;
	focus(NULL);
}

void focusstackvis(const Arg *arg)
{
	focusstack(arg->i, 0);
}

void focusstackhid(const Arg *arg)
{
	focusstack(arg->i, 1);
}

void focusstack(int inc, int hid)
{
	Client *c = NULL, *i;

	if ((!selmon->sel && !hid) || (selmon->sel && selmon->sel->isfullscreen && lockfullscreen))
		return;
	if (inc > 0) {
		if (selmon->sel)
			for (c = selmon->sel->next; c && (!ISVISIBLE(c) || (!hid && HIDDEN(c))); c = c->next)
				;
		;
		if (!c)
			for (c = selmon->clients; c && (!ISVISIBLE(c) || (!hid && HIDDEN(c))); c = c->next)
				;
	} else {
		if (selmon->sel) {
			for (i = selmon->clients; i != selmon->sel; i = i->next)
				if (ISVISIBLE(i) && !(!hid && HIDDEN(i)))
					c = i;
		} else
			c = selmon->clients;
		if (!c)
			for (; i; i = i->next)
				if (ISVISIBLE(i) && !(!hid && HIDDEN(i)))
					c = i;
	}
	if (c) {
		focus(c);
		restack(selmon);
		if (HIDDEN(c)) {
			showwin(c);
			c->mon->hidsel = 1;
		}
	}
}

Atom getatomprop(Client *c, Atom prop)
{
	int	       di;
	unsigned long  dl;
	unsigned char *p = NULL;
	Atom	       da, atom = None;
	Atom	       req = XA_ATOM;

	if (prop == xatom[XembedInfo])
		req = xatom[XembedInfo];

	if (XGetWindowProperty(dpy, c->win, prop, 0L, sizeof atom, False, req, &da, &di, &dl, &dl, &p) == Success && p) {
		atom = *(Atom *)p;
		if (da == xatom[XembedInfo] && dl == 2)
			atom = ((Atom *)p)[1];
		XFree(p);
	}
	return atom;
}

static uint32_t prealpha(uint32_t p)
{
	uint8_t	 a = p >> 24u;
	uint32_t rb = (a * (p & 0xFF00FFu)) >> 8u;
	uint32_t g = (a * (p & 0x00FF00u)) >> 8u;
	return (rb & 0xFF00FFu) | (g & 0x00FF00u) | (a << 24u);
}

Picture geticonprop(Window win, unsigned int *picw, unsigned int *pich)
{
	int	      format;
	unsigned long n, extra, *p = NULL;
	Atom	      real;

	if (XGetWindowProperty(dpy, win, netatom[NetWMIcon], 0L, LONG_MAX, False, AnyPropertyType, &real, &format, &n, &extra, (unsigned char **)&p) != Success)
		return None;
	if (n == 0 || format != 32) {
		XFree(p);
		return None;
	}

	unsigned long *bstp = NULL;
	uint32_t       w, h, sz;
	{
		unsigned long	    *i;
		const unsigned long *end = p + n;
		uint32_t	     bstd = UINT32_MAX, d, m;
		for (i = p; i < end - 1; i += sz) {
			if ((w = *i++) >= 16384 || (h = *i++) >= 16384) {
				XFree(p);
				return None;
			}
			if ((sz = w * h) > end - i)
				break;
			if ((m = w > h ? w : h) >= ICONSIZE && (d = m - ICONSIZE) < bstd) {
				bstd = d;
				bstp = i;
			}
		}
		if (!bstp) {
			for (i = p; i < end - 1; i += sz) {
				if ((w = *i++) >= 16384 || (h = *i++) >= 16384) {
					XFree(p);
					return None;
				}
				if ((sz = w * h) > end - i)
					break;
				if ((d = ICONSIZE - (w > h ? w : h)) < bstd) {
					bstd = d;
					bstp = i;
				}
			}
		}
		if (!bstp) {
			XFree(p);
			return None;
		}
	}

	if ((w = *(bstp - 2)) == 0 || (h = *(bstp - 1)) == 0) {
		XFree(p);
		return None;
	}

	uint32_t icw, ich;
	if (w <= h) {
		ich = ICONSIZE;
		icw = w * ICONSIZE / h;
		if (icw == 0)
			icw = 1;
	} else {
		icw = ICONSIZE;
		ich = h * ICONSIZE / w;
		if (ich == 0)
			ich = 1;
	}
	*picw = icw;
	*pich = ich;

	uint32_t i, *bstp32 = (uint32_t *)bstp;
	for (sz = w * h, i = 0; i < sz; ++i)
		bstp32[i] = prealpha(bstp[i]);

	Picture ret = drw_picture_create_resized(drw, (char *)bstp, w, h, icw, ich);
	XFree(p);

	return ret;
}

unsigned int getsystraywidth()
{
	unsigned int w = 0;
	Client	    *i;
	if (showsystray)
		for (i = systray->icons; i; w += i->w + systrayspacing, i = i->next)
			;
	return w ? w + systrayspacing : 1;
}

int getrootptr(int *x, int *y)
{
	int	     di;
	unsigned int dui;
	Window	     dummy;

	return XQueryPointer(dpy, root, &dummy, &dummy, x, y, &di, &di, &dui);
}

void removesystrayicon(Client *i)
{
	Client **ii;

	if (!showsystray || !i)
		return;
	for (ii = &systray->icons; *ii && *ii != i; ii = &(*ii)->next)
		;
	if (ii)
		*ii = i->next;
	free(i);
}

long getstate(Window w)
{
	int	       format;
	long	       result = -1;
	unsigned char *p = NULL;
	unsigned long  n, extra;
	Atom	       real;

	if (XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, False, wmatom[WMState], &real, &format, &n, &extra, (unsigned char **)&p) != Success)
		return -1;
	if (n != 0)
		result = *p;
	XFree(p);
	return result;
}

int gettextprop(Window w, Atom atom, char *text, unsigned int size)
{
	char	    **list = NULL;
	int	      n;
	XTextProperty name;

	if (!text || size == 0)
		return 0;
	text[0] = '\0';
	if (!XGetTextProperty(dpy, w, &name, atom) || !name.nitems)
		return 0;
	if (name.encoding == XA_STRING) {
		strncpy(text, (char *)name.value, size - 1);
	} else if (XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success && n > 0 && *list) {
		strncpy(text, *list, size - 1);
		XFreeStringList(list);
	}
	text[size - 1] = '\0';
	XFree(name.value);
	return 1;
}

void grabbuttons(Client *c, int focused)
{
	updatenumlockmask();
	{
		unsigned int i, j;
		unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask | LockMask };
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		if (!focused)
			XGrabButton(dpy, AnyButton, AnyModifier, c->win, False, BUTTONMASK, GrabModeSync, GrabModeSync, None, None);
		for (i = 0; i < LENGTH(buttons); i++)
			if (buttons[i].click == ClkClientWin)
				for (j = 0; j < LENGTH(modifiers); j++)
					XGrabButton(dpy, buttons[i].button, buttons[i].mask | modifiers[j], c->win, False, BUTTONMASK, GrabModeAsync,
						    GrabModeSync, None, None);
	}
}

void grabkeys(void)
{
	updatenumlockmask();
	{
		unsigned int i, j, k;
		unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask | LockMask };
		int	     start, end, skip;
		KeySym	    *syms;

		XUngrabKey(dpy, AnyKey, AnyModifier, root);
		XDisplayKeycodes(dpy, &start, &end);
		syms = XGetKeyboardMapping(dpy, start, end - start + 1, &skip);
		if (!syms)
			return;
		for (k = start; k <= end; k++)
			for (i = 0; i < LENGTH(keys); i++)
				/* skip modifier codes, we do that ourselves */
				if (keys[i].keysym == syms[(k - start) * skip])
					for (j = 0; j < LENGTH(modifiers); j++)
						XGrabKey(dpy, k, keys[i].mod | modifiers[j], root, True, GrabModeAsync, GrabModeAsync);
		XFree(syms);
	}
}
void hide(const Arg *arg)
{
	hidewin(selmon->sel);
	focus(NULL);
	arrange(selmon);
}

void hidewin(Client *c)
{
	if (!c || HIDDEN(c))
		return;

	Window			 w = c->win;
	static XWindowAttributes ra, ca;

	// more or less taken directly from blackbox's hide() function
	XGrabServer(dpy);
	XGetWindowAttributes(dpy, root, &ra);
	XGetWindowAttributes(dpy, w, &ca);
	// prevent UnmapNotify events
	XSelectInput(dpy, root, ra.your_event_mask & ~SubstructureNotifyMask);
	XSelectInput(dpy, w, ca.your_event_mask & ~StructureNotifyMask);
	XUnmapWindow(dpy, w);
	setclientstate(c, IconicState);
	XSelectInput(dpy, root, ra.your_event_mask);
	XSelectInput(dpy, w, ca.your_event_mask);
	XUngrabServer(dpy);
}

void incnmaster(const Arg *arg)
{
	selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag] = MAX(selmon->nmaster + arg->i, 0);
	arrange(selmon);
}

#ifdef XINERAMA
static int isuniquegeom(XineramaScreenInfo *unique, size_t n, XineramaScreenInfo *info)
{
	while (n--)
		if (unique[n].x_org == info->x_org && unique[n].y_org == info->y_org && unique[n].width == info->width && unique[n].height == info->height)
			return 0;
	return 1;
}
#endif /* XINERAMA */

void keypress(XEvent *e)
{
	unsigned int i;
	KeySym	     keysym;
	XKeyEvent   *ev;

	ev = &e->xkey;
	keysym = XKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0);
	for (i = 0; i < LENGTH(keys); i++)
		if (keysym == keys[i].keysym && CLEANMASK(keys[i].mod) == CLEANMASK(ev->state) && keys[i].func)
			keys[i].func(&(keys[i].arg));
}

void killclient(const Arg *arg)
{
	if (!selmon->sel)
		return;

	if (!sendevent(selmon->sel->win, wmatom[WMDelete], NoEventMask, wmatom[WMDelete], CurrentTime, 0, 0, 0)) {
		XGrabServer(dpy);
		XSetErrorHandler(xerrordummy);
		XSetCloseDownMode(dpy, DestroyAll);
		XKillClient(dpy, selmon->sel->win);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
}

void manage(Window w, XWindowAttributes *wa)
{
	Client	      *c, *t = NULL;
	Window	       trans = None;
	XWindowChanges wc;

	c = ecalloc(1, sizeof(Client));
	c->win = w;
	/* geometry */
	c->x = c->oldx = wa->x;
	c->y = c->oldy = wa->y;
	c->w = c->oldw = wa->width;
	c->h = c->oldh = wa->height;
	c->oldbw = wa->border_width;

	updateicon(c);
	updatetitle(c);
	if (XGetTransientForHint(dpy, w, &trans) && (t = wintoclient(trans))) {
		c->mon = t->mon;
		c->tags = t->tags;
	} else {
		c->mon = selmon;
		applyrules(c);
	}

	if (c->x + WIDTH(c) > c->mon->wx + c->mon->ww)
		c->x = c->mon->wx + c->mon->ww - WIDTH(c);
	if (c->y + HEIGHT(c) > c->mon->wy + c->mon->wh)
		c->y = c->mon->wy + c->mon->wh - HEIGHT(c);
	c->x = MAX(c->x, c->mon->wx);
	c->y = MAX(c->y, c->mon->wy);
	c->bw = borderpx;

	wc.border_width = c->bw;
	XConfigureWindow(dpy, w, CWBorderWidth, &wc);
	XSetWindowBorder(dpy, w, scheme[SchemeNorm][ColBorder].pixel);
	configure(c); /* propagates border_width, if size doesn't change */
	updatewindowtype(c);
	updatesizehints(c);
	updatewmhints(c);
	c->x = c->mon->mx + (c->mon->mw - WIDTH(c)) / 2;
	c->y = c->mon->my + (c->mon->mh - HEIGHT(c)) / 2;
	{
		int	       format;
		unsigned long *data, n, extra;
		Monitor	      *m;
		Atom	       atom;
		if (XGetWindowProperty(dpy, c->win, netatom[NetClientInfo], 0L, 2L, False, XA_CARDINAL, &atom, &format, &n, &extra, (unsigned char **)&data) ==
			    Success &&
		    n == 2) {
			c->tags = *data;
			for (m = mons; m; m = m->next) {
				if (m->num == *(data + 1)) {
					c->mon = m;
					break;
				}
			}
		}
		if (n > 0)
			XFree(data);
	}
	setclienttagprop(c);
	XSelectInput(dpy, w, EnterWindowMask | FocusChangeMask | PropertyChangeMask | StructureNotifyMask);
	grabbuttons(c, 0);
	if (!c->isfloating)
		c->isfloating = c->oldstate = trans != None || c->isfixed;
	if (c->isfloating)
		XRaiseWindow(dpy, c->win);
	attach(c);
	attachstack(c);
	XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend, (unsigned char *)&(c->win), 1);
	XMoveResizeWindow(dpy, c->win, c->x + 2 * sw, c->y, c->w, c->h); /* some windows require this */
	if (!HIDDEN(c))
		setclientstate(c, NormalState);
	if (c->mon == selmon)
		unfocus(selmon->sel, 0);
	c->mon->sel = c;
	arrange(c->mon);
	if (!HIDDEN(c))
		XMapWindow(dpy, c->win);
	focus(NULL);
}

void mappingnotify(XEvent *e)
{
	XMappingEvent *ev = &e->xmapping;

	XRefreshKeyboardMapping(ev);
	if (ev->request == MappingKeyboard)
		grabkeys();
}

void maprequest(XEvent *e)
{
	static XWindowAttributes wa;
	XMapRequestEvent	*ev = &e->xmaprequest;

	Client *i;
	if ((i = wintosystrayicon(ev->window))) {
		sendevent(i->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_WINDOW_ACTIVATE, 0, systray->win, XEMBED_EMBEDDED_VERSION);
		resizebarwin(selmon);
		updatesystray();
	}

	if (!XGetWindowAttributes(dpy, ev->window, &wa) || wa.override_redirect)
		return;
	if (!wintoclient(ev->window))
		manage(ev->window, &wa);
}

void monocle(Monitor *m)
{
	unsigned int n = 0;
	Client	    *c;

	for (c = m->clients; c; c = c->next)
		if (ISVISIBLE(c))
			n++;
	if (n > 0) /* override layout symbol */
		snprintf(m->ltsymbol, sizeof m->ltsymbol, "[%d]", n);
	for (c = nexttiled(m->clients); c; c = nexttiled(c->next))
		resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, 0);
}

void motionnotify(XEvent *e)
{
	int		x, i;
	static Monitor *mon = NULL;
	Client	       *c;
	Monitor	       *m;
	XMotionEvent   *ev = &e->xmotion;

	if (ev->window != selmon->barwin) {
		if (selmon->hov) {
			if (selmon->hov != selmon->sel)
				XSetWindowBorder(dpy, selmon->hov->win, scheme[SchemeNorm][ColBorder].pixel);
			else
				XSetWindowBorder(dpy, selmon->hov->win, scheme[SchemeSel][ColBorder].pixel);

			selmon->hov = NULL;
			c = wintoclient(ev->window);
			m = c ? c->mon : wintomon(ev->window);
			drawbar(m);
		}

		if (ev->window == root) {
			if ((m = recttomon(ev->x_root, ev->y_root, 1, 1)) != mon && mon) {
				unfocus(selmon->sel, 1);
				selmon = m;
				focus(NULL);
			}
			mon = m;
		}

		return;
	}

	c = wintoclient(ev->window);
	m = c ? c->mon : wintomon(ev->window);
	c = m->clients;

	x = 0, i = 0;
	do
		x += TEXTW(tags[i]);
	while (ev->x >= x && ++i < LENGTH(tags));
	if (i < LENGTH(tags) || ev->x < x + TEXTW(selmon->ltsymbol) || ev->x > selmon->ww - TEXTW(stext) + lrpad - 2) {
		if (selmon->hov) {
			if (selmon->hov != selmon->sel)
				XSetWindowBorder(dpy, selmon->hov->win, scheme[SchemeNorm][ColBorder].pixel);
			else
				XSetWindowBorder(dpy, selmon->hov->win, scheme[SchemeSel][ColBorder].pixel);
			selmon->hov = NULL;
			drawbar(m);
		}
	} else {
		x += TEXTW(selmon->ltsymbol);
		if (c) {
			do {
				if (!ISVISIBLE(c))
					continue;
				else
					x += (1.0 / (double)m->bt) * m->btw;
			} while (ev->x > x && (c = c->next));
			if (c) {
				if (selmon->hov) {
					if (selmon->hov != selmon->sel)
						XSetWindowBorder(dpy, selmon->hov->win, scheme[SchemeNorm][ColBorder].pixel);
					else
						XSetWindowBorder(dpy, selmon->hov->win, scheme[SchemeSel][ColBorder].pixel);
				}
				selmon->hov = c;
				XSetWindowBorder(dpy, c->win, scheme[SchemeHov][ColBorder].pixel);
			}
		}
		drawbar(m);
	}
}

void movemouse(const Arg *arg)
{
	int	 x, y, ocx, ocy, nx, ny;
	Client	*c;
	Monitor *m;
	XEvent	 ev;
	Time	 lasttime = 0;

	if (!(c = selmon->sel))
		return;
	if (c->isfullscreen) /* no support moving fullscreen windows by mouse */
		return;
	restack(selmon);
	ocx = c->x;
	ocy = c->y;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync, None, cursor[CurMove]->cursor, CurrentTime) != GrabSuccess)
		return;
	if (!getrootptr(&x, &y))
		return;
	do {
		XMaskEvent(dpy, MOUSEMASK | ExposureMask | SubstructureRedirectMask, &ev);
		switch (ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / 60))
				continue;
			lasttime = ev.xmotion.time;

			nx = ocx + (ev.xmotion.x - x);
			ny = ocy + (ev.xmotion.y - y);
			if (abs(selmon->wx - nx) < snap)
				nx = selmon->wx;
			else if (abs((selmon->wx + selmon->ww) - (nx + WIDTH(c))) < snap)
				nx = selmon->wx + selmon->ww - WIDTH(c);
			if (abs(selmon->wy - ny) < snap)
				ny = selmon->wy;
			else if (abs((selmon->wy + selmon->wh) - (ny + HEIGHT(c))) < snap)
				ny = selmon->wy + selmon->wh - HEIGHT(c);
			if (!c->isfloating && selmon->lt[selmon->sellt]->arrange && (abs(nx - c->x) > snap || abs(ny - c->y) > snap))
				togglefloating(NULL);
			if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
				resize(c, nx, ny, c->w, c->h, 1);
			break;
		}
	} while (ev.type != ButtonRelease);
	XUngrabPointer(dpy, CurrentTime);
	if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
		sendmon(c, m);
		selmon = m;
		focus(NULL);
	}
}

Client *nexttiled(Client *c)
{
	for (; c && (c->isfloating || !ISVISIBLE(c) || HIDDEN(c)); c = c->next)
		;
	return c;
}

void pop(Client *c)
{
	detach(c);
	attach(c);
	focus(c);
	arrange(c->mon);
}

void propertynotify(XEvent *e)
{
	Client	       *c;
	Window		trans;
	XPropertyEvent *ev = &e->xproperty;

	if ((c = wintosystrayicon(ev->window))) {
		if (ev->atom == XA_WM_NORMAL_HINTS) {
			updatesizehints(c);
			updatesystrayicongeom(c, c->w, c->h);
		} else
			updatesystrayiconstate(c, ev);
		resizebarwin(selmon);
		updatesystray();
	}

	if ((ev->window == root) && (ev->atom == XA_WM_NAME))
		updatestatus();
	else if (ev->state == PropertyDelete)
		return; /* ignore */
	else if ((c = wintoclient(ev->window))) {
		switch (ev->atom) {
		default:
			break;
		case XA_WM_TRANSIENT_FOR:
			if (!c->isfloating && (XGetTransientForHint(dpy, c->win, &trans)) && (c->isfloating = (wintoclient(trans)) != NULL))
				arrange(c->mon);
			break;
		case XA_WM_NORMAL_HINTS:
			c->hintsvalid = 0;
			break;
		case XA_WM_HINTS:
			updatewmhints(c);
			drawbars();
			break;
		}
		if (ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName]) {
			updatetitle(c);
			if (c == c->mon->sel)
				drawbar(c->mon);
		} else if (ev->atom == netatom[NetWMIcon]) {
			updateicon(c);
			if (c == c->mon->sel)
				drawbar(c->mon);
		}
		if (ev->atom == netatom[NetWMWindowType])
			updatewindowtype(c);
	}
}

void quit(const Arg *arg)
{
	if (arg->i)
		restart = 1;
	// fix: reloading dwm keeps all the hidden clients hidden
	Monitor *m;
	Client	*c;
	for (m = mons; m; m = m->next) {
		if (m) {
			for (c = m->stack; c; c = c->next)
				if (c && HIDDEN(c))
					showwin(c);
		}
	}

	running = 0;
}

Monitor *recttomon(int x, int y, int w, int h)
{
	Monitor *m, *r = selmon;
	int	 a, area = 0;

	for (m = mons; m; m = m->next)
		if ((a = INTERSECT(x, y, w, h, m)) > area) {
			area = a;
			r = m;
		}
	return r;
}

void resize(Client *c, int x, int y, int w, int h, int interact)
{
	if (applysizehints(c, &x, &y, &w, &h, interact))
		resizeclient(c, x, y, w, h);
}

void resizebarwin(Monitor *m)
{
	unsigned int w = m->ww;
	if (showsystray && m == systraytomon(m) && !systrayonleft)
		w -= getsystraywidth();
	// XMoveResizeWindow(dpy, m->barwin, m->wx, m->by, w, bh); 标签栏间隙补丁1
	XMoveResizeWindow(dpy, m->barwin, m->wx + sp, m->by + vp, m->ww - 2 * sp, bh);
}

void resizeclient(Client *c, int x, int y, int w, int h)
{
	XWindowChanges wc;

	c->oldx = c->x;
	c->x = wc.x = x;
	c->oldy = c->y;
	c->y = wc.y = y;
	c->oldw = c->w;
	c->w = wc.width = w;
	c->oldh = c->h;
	c->h = wc.height = h;
	wc.border_width = c->bw;
	XConfigureWindow(dpy, c->win, CWX | CWY | CWWidth | CWHeight | CWBorderWidth, &wc);
	configure(c);
	XSync(dpy, False);
}

void resizerequest(XEvent *e)
{
	XResizeRequestEvent *ev = &e->xresizerequest;
	Client		    *i;

	if ((i = wintosystrayicon(ev->window))) {
		updatesystrayicongeom(i, ev->width, ev->height);
		resizebarwin(selmon);
		updatesystray();
	}
}

void resizemouse(const Arg *arg)
{
	int	 ocx, ocy, nw, nh;
	Client	*c;
	Monitor *m;
	XEvent	 ev;
	Time	 lasttime = 0;

	if (!(c = selmon->sel))
		return;
	if (c->isfullscreen) /* no support resizing fullscreen windows by mouse */
		return;
	restack(selmon);
	ocx = c->x;
	ocy = c->y;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync, None, cursor[CurResize]->cursor, CurrentTime) != GrabSuccess)
		return;
	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
	do {
		XMaskEvent(dpy, MOUSEMASK | ExposureMask | SubstructureRedirectMask, &ev);
		switch (ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / 60))
				continue;
			lasttime = ev.xmotion.time;

			nw = MAX(ev.xmotion.x - ocx - 2 * c->bw + 1, 1);
			nh = MAX(ev.xmotion.y - ocy - 2 * c->bw + 1, 1);
			if (c->mon->wx + nw >= selmon->wx && c->mon->wx + nw <= selmon->wx + selmon->ww && c->mon->wy + nh >= selmon->wy &&
			    c->mon->wy + nh <= selmon->wy + selmon->wh) {
				if (!c->isfloating && selmon->lt[selmon->sellt]->arrange && (abs(nw - c->w) > snap || abs(nh - c->h) > snap))
					togglefloating(NULL);
			}
			if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
				resize(c, c->x, c->y, nw, nh, 1);
			break;
		}
	} while (ev.type != ButtonRelease);
	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
	XUngrabPointer(dpy, CurrentTime);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev))
		;
	if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
		sendmon(c, m);
		selmon = m;
		focus(NULL);
	}
}

void restack(Monitor *m)
{
	Client	      *c;
	XEvent	       ev;
	XWindowChanges wc;

	drawbar(m);
	if (!m->sel)
		return;
	if (m->sel->isfloating || !m->lt[m->sellt]->arrange)
		XRaiseWindow(dpy, m->sel->win);
	if (m->lt[m->sellt]->arrange) {
		wc.stack_mode = Below;
		wc.sibling = m->barwin;
		for (c = m->stack; c; c = c->snext)
			if (!c->isfloating && ISVISIBLE(c)) {
				XConfigureWindow(dpy, c->win, CWSibling | CWStackMode, &wc);
				wc.sibling = c->win;
			}
	}
	XSync(dpy, False);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev))
		;
}

void run(void)
{
	XEvent ev;
	/* main event loop */
	XSync(dpy, False);
	while (running && !XNextEvent(dpy, &ev))
		if (handler[ev.type])
			handler[ev.type](&ev); /* call handler */
}

void runAutostart(void)
{
	system("cd ~/.dwm; ./autostart_blocking.sh");
	system("cd ~/.dwm; ./autostart.sh &");
}

void scan(void)
{
	unsigned int	  i, num;
	Window		  d1, d2, *wins = NULL;
	XWindowAttributes wa;

	if (XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
		for (i = 0; i < num; i++) {
			if (!XGetWindowAttributes(dpy, wins[i], &wa) || wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1))
				continue;
			if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
				manage(wins[i], &wa);
		}
		for (i = 0; i < num; i++) { /* now the transients */
			if (!XGetWindowAttributes(dpy, wins[i], &wa))
				continue;
			if (XGetTransientForHint(dpy, wins[i], &d1) && (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
				manage(wins[i], &wa);
		}
		if (wins)
			XFree(wins);
	}
}

void sendmon(Client *c, Monitor *m)
{
	if (c->mon == m)
		return;
	unfocus(c, 1);
	detach(c);
	detachstack(c);
	c->mon = m;
	c->tags = m->tagset[m->seltags]; /* assign tags of target monitor */
	attach(c);
	attachstack(c);
	setclienttagprop(c);
	focus(NULL);
	arrange(NULL);
}

void setclientstate(Client *c, long state)
{
	long data[] = { state, None };

	XChangeProperty(dpy, c->win, wmatom[WMState], wmatom[WMState], 32, PropModeReplace, (unsigned char *)data, 2);
}

int sendevent(Window w, Atom proto, int mask, long d0, long d1, long d2, long d3, long d4)
{
	int    n;
	Atom  *protocols, mt;
	int    exists = 0;
	XEvent ev;

	if (proto == wmatom[WMTakeFocus] || proto == wmatom[WMDelete]) {
		mt = wmatom[WMProtocols];
		if (XGetWMProtocols(dpy, w, &protocols, &n)) {
			while (!exists && n--)
				exists = protocols[n] == proto;
			XFree(protocols);
		}
	} else {
		exists = True;
		mt = proto;
	}

	if (exists) {
		ev.type = ClientMessage;
		ev.xclient.window = w;
		ev.xclient.message_type = mt;
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = d0;
		ev.xclient.data.l[1] = d1;
		ev.xclient.data.l[2] = d2;
		ev.xclient.data.l[3] = d3;
		ev.xclient.data.l[4] = d4;
		XSendEvent(dpy, w, False, mask, &ev);
	}
	return exists;
}

void setfocus(Client *c)
{
	if (!c->neverfocus) {
		XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
		XChangeProperty(dpy, root, netatom[NetActiveWindow], XA_WINDOW, 32, PropModeReplace, (unsigned char *)&(c->win), 1);
	}
	sendevent(c->win, wmatom[WMTakeFocus], NoEventMask, wmatom[WMTakeFocus], CurrentTime, 0, 0, 0);
}

void setfullscreen(Client *c, int fullscreen)
{
	if (fullscreen && !c->isfullscreen) {
		XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32, PropModeReplace, (unsigned char *)&netatom[NetWMFullscreen], 1);
		c->isfullscreen = 1;
		c->oldstate = c->isfloating;
		c->oldbw = c->bw;
		c->bw = 0;
		c->isfloating = 1;
		resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
		XRaiseWindow(dpy, c->win);
	} else if (!fullscreen && c->isfullscreen) {
		XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32, PropModeReplace, (unsigned char *)0, 0);
		c->isfullscreen = 0;
		c->isfloating = c->oldstate;
		c->bw = c->oldbw;
		c->x = c->oldx;
		c->y = c->oldy;
		c->w = c->oldw;
		c->h = c->oldh;
		resizeclient(c, c->x, c->y, c->w, c->h);
		arrange(c->mon);
	}
}
void setgaps(const Arg *arg)
{
	if ((arg->i == 0) || (selmon->gappx + arg->i < 0))
		selmon->gappx = 0;
	else
		selmon->gappx += arg->i;
	arrange(selmon);
}

void setlayout(const Arg *arg)
{
	if (!arg || !arg->v || arg->v != selmon->lt[selmon->sellt])
		selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag] ^= 1;
	if (arg && arg->v)
		selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt] = (Layout *)arg->v;
	selmon->lt[selmon->sellt] = (Layout *)arg->v;
	strncpy(selmon->ltsymbol, selmon->lt[selmon->sellt]->symbol, sizeof selmon->ltsymbol);
	if (selmon->sel)
		arrange(selmon);
	else
		drawbar(selmon);
}

/* arg > 1.0 will set mfact absolutely */
void setmfact(const Arg *arg)
{
	float f;

	if (!arg || !selmon->lt[selmon->sellt]->arrange)
		return;
	f = arg->f < 1.0 ? arg->f + selmon->mfact : arg->f - 1.0;
	if (f < 0.05 || f > 0.95)
		return;
	selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag] = f;
	arrange(selmon);
}

void setup(void)
{
	int		     i;
	XSetWindowAttributes wa;
	Atom		     utf8string;
	struct sigaction     sa;

	/* do not transform children into zombies when they terminate */
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_NOCLDSTOP | SA_NOCLDWAIT | SA_RESTART;
	sa.sa_handler = SIG_IGN;
	sigaction(SIGCHLD, &sa, NULL);

	/* clean up any zombies (inherited from .xinitrc etc) immediately */
	while (waitpid(-1, NULL, WNOHANG) > 0)
		;

	signal(SIGHUP, sighup);
	signal(SIGTERM, sigterm);

	/* init screen */
	screen = DefaultScreen(dpy);
	sw = DisplayWidth(dpy, screen);
	sh = DisplayHeight(dpy, screen);
	root = RootWindow(dpy, screen);
	drw = drw_create(dpy, screen, root, sw, sh);
	if (!drw_fontset_create(drw, fonts, LENGTH(fonts)))
		die("no fonts could be loaded.");
	lrpad = drw->fonts->h + horizpadbar;
	bh = drw->fonts->h + vertpadbar + 2;
	sp = sidepad;
	vp = (topbar == 1) ? vertpad : -vertpad;
	updategeom();
	/* init atoms */
	utf8string = XInternAtom(dpy, "UTF8_STRING", False);
	wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
	wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	wmatom[WMState] = XInternAtom(dpy, "WM_STATE", False);
	wmatom[WMTakeFocus] = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
	netatom[NetActiveWindow] = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
	xatom[Manager] = XInternAtom(dpy, "MANAGER", False);
	xatom[Xembed] = XInternAtom(dpy, "_XEMBED", False);
	xatom[XembedInfo] = XInternAtom(dpy, "_XEMBED_INFO", False);
	netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
	netatom[NetSystemTray] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_S0", False);
	netatom[NetSystemTrayOP] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_OPCODE", False);
	netatom[NetSystemTrayOrientation] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_ORIENTATION", False);
	netatom[NetSystemTrayOrientationHorz] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_ORIENTATION_HORZ", False);
	netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);
	netatom[NetWMIcon] = XInternAtom(dpy, "_NET_WM_ICON", False);
	netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
	netatom[NetWMCheck] = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
	netatom[NetWMFullscreen] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
	netatom[NetWMWindowType] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
	netatom[NetWMWindowTypeDialog] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
	netatom[NetClientList] = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
	netatom[NetClientInfo] = XInternAtom(dpy, "_NET_CLIENT_INFO", False);
	/* init cursors */
	cursor[CurNormal] = drw_cur_create(drw, XC_left_ptr);
	cursor[CurResize] = drw_cur_create(drw, XC_sizing);
	cursor[CurMove] = drw_cur_create(drw, XC_fleur);
	/* init appearance */
	if (LENGTH(tags) > LENGTH(tagsel))
		die("too few color schemes for the tags");
	scheme = ecalloc(LENGTH(colors) + 1, sizeof(Clr *));
	scheme[LENGTH(colors)] = drw_scm_create(drw, colors[0], 3);
	for (i = 0; i < LENGTH(colors); i++)
		scheme[i] = drw_scm_create(drw, colors[i], 3);
	tagscheme = ecalloc(LENGTH(tagsel), sizeof(Clr *));
	for (i = 0; i < LENGTH(tagsel); i++)
		tagscheme[i] = drw_scm_create(drw, tagsel[i], 2);
	/* init system tray */
	updatesystray();
	/* init bars */
	updatebars();
	updatestatus();
	/* supporting window for NetWMCheck */
	wmcheckwin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMCheck], XA_WINDOW, 32, PropModeReplace, (unsigned char *)&wmcheckwin, 1);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMName], utf8string, 8, PropModeReplace, (unsigned char *)"dwm", 3);
	XChangeProperty(dpy, root, netatom[NetWMCheck], XA_WINDOW, 32, PropModeReplace, (unsigned char *)&wmcheckwin, 1);
	/* EWMH support per view */
	XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32, PropModeReplace, (unsigned char *)netatom, NetLast);
	XDeleteProperty(dpy, root, netatom[NetClientList]);
	XDeleteProperty(dpy, root, netatom[NetClientInfo]);
	/* select events */
	wa.cursor = cursor[CurNormal]->cursor;
	wa.event_mask = SubstructureRedirectMask | SubstructureNotifyMask | ButtonPressMask | PointerMotionMask | EnterWindowMask | LeaveWindowMask |
			StructureNotifyMask | PropertyChangeMask;
	XChangeWindowAttributes(dpy, root, CWEventMask | CWCursor, &wa);
	XSelectInput(dpy, root, wa.event_mask);
	grabkeys();
	focus(NULL);
}

void seturgent(Client *c, int urg)
{
	XWMHints *wmh;

	c->isurgent = urg;
	if (!(wmh = XGetWMHints(dpy, c->win)))
		return;
	wmh->flags = urg ? (wmh->flags | XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
	XSetWMHints(dpy, c->win, wmh);
	XFree(wmh);
}
void show(const Arg *arg)
{
	if (selmon->hidsel)
		selmon->hidsel = 0;
	showwin(selmon->sel);
}

void showall(const Arg *arg)
{
	Client *c = NULL;
	selmon->hidsel = 0;
	for (c = selmon->clients; c; c = c->next) {
		if (ISVISIBLE(c))
			showwin(c);
	}
	if (!selmon->sel) {
		for (c = selmon->clients; c && !ISVISIBLE(c); c = c->next)
			;
		if (c)
			focus(c);
	}
	restack(selmon);
}

void showwin(Client *c)
{
	if (!c || !HIDDEN(c))
		return;

	XMapWindow(dpy, c->win);
	setclientstate(c, NormalState);
	arrange(c->mon);
}

void showhide(Client *c)
{
	if (!c)
		return;
	if (ISVISIBLE(c)) {
		if ((c->tags & SPTAGMASK) && c->isfloating) {
			c->x = c->mon->wx + (c->mon->ww / 2 - WIDTH(c) / 2);
			c->y = c->mon->wy + (c->mon->wh / 2 - HEIGHT(c) / 2);
		}
		/* show clients top down */
		XMoveWindow(dpy, c->win, c->x, c->y);
		if ((!c->mon->lt[c->mon->sellt]->arrange || c->isfloating) && !c->isfullscreen)
			resize(c, c->x, c->y, c->w, c->h, 0);
		showhide(c->snext);
	} else {
		/* hide clients bottom up */
		showhide(c->snext);
		XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y);
	}
}

void togglewin(const Arg *arg)
{
	Client *c = (Client *)arg->v;

	if (c == selmon->sel) {
		hidewin(c);
		focus(NULL);
		arrange(c->mon);
	} else {
		if (HIDDEN(c))
			showwin(c);
		focus(c);
		restack(selmon);
	}
}

void sighup(int unused)
{
	Arg a = { .i = 1 };
	quit(&a);
}

void sigterm(int unused)
{
	Arg a = { .i = 0 };
	quit(&a);
}

void spawn(const Arg *arg)
{
	struct sigaction sa;

	if (arg->v == dmenucmd)
		dmenumon[0] = '0' + selmon->num;
	if (fork() == 0) {
		if (dpy)
			close(ConnectionNumber(dpy));
		setsid();

		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
		sa.sa_handler = SIG_DFL;
		sigaction(SIGCHLD, &sa, NULL);

		execvp(((char **)arg->v)[0], (char **)arg->v);
		die("dwm: execvp '%s' failed:", ((char **)arg->v)[0]);
	}
}

void setclienttagprop(Client *c)
{
	long data[] = { (long)c->tags, (long)c->mon->num };
	XChangeProperty(dpy, c->win, netatom[NetClientInfo], XA_CARDINAL, 32, PropModeReplace, (unsigned char *)data, 2);
}

void tag(const Arg *arg)
{
	Client *c;
	if (selmon->sel && arg->ui & TAGMASK) {
		c = selmon->sel;
		selmon->sel->tags = arg->ui & TAGMASK;
		setclienttagprop(c);
		focus(NULL);
		arrange(selmon);
	}
}

void tagmon(const Arg *arg)
{
	if (!selmon->sel || !mons->next)
		return;
	sendmon(selmon->sel, dirtomon(arg->i));
}

void tile(Monitor *m)
{
	unsigned int i, n, h, mw, my, ty;
	Client	    *c;

	for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++)
		;
	if (n == 0)
		return;

	if (n > m->nmaster)
		mw = m->nmaster ? m->ww * m->mfact : 0;
	else
		mw = m->ww - m->gappx;
	for (i = 0, my = ty = m->gappx, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++)
		if (i < m->nmaster) {
			h = (m->wh - my) / (MIN(n, m->nmaster) - i) - m->gappx;
			resize(c, m->wx + m->gappx, m->wy + my, mw - (2 * c->bw) - m->gappx, h - (2 * c->bw), 0);
			if (my + HEIGHT(c) + m->gappx < m->wh)
				my += HEIGHT(c) + m->gappx;
		} else {
			h = (m->wh - ty) / (n - i) - m->gappx;
			resize(c, m->wx + mw + m->gappx, m->wy + ty, m->ww - mw - (2 * c->bw) - 2 * m->gappx, h - (2 * c->bw), 0);
			if (ty + HEIGHT(c) + m->gappx < m->wh)
				ty += HEIGHT(c) + m->gappx;
		}
}

void togglebar(const Arg *arg)
{
	selmon->showbar = selmon->pertag->showbars[selmon->pertag->curtag] = !selmon->showbar;
	updatebarpos(selmon);
	resizebarwin(selmon);
	if (showsystray) {
		XWindowChanges wc;
		if (!selmon->showbar)
			wc.y = -bh;
		else if (selmon->showbar) {
			wc.y = 0;
			if (!selmon->topbar)
				wc.y = selmon->mh - bh;
		}
		XConfigureWindow(dpy, systray->win, CWY, &wc);
	}
	arrange(selmon);
}
void togglescratch(const Arg *arg)
{
	Client	    *c;
	unsigned int found = 0;
	unsigned int scratchtag = SPTAG(arg->ui);
	Arg	     sparg = { .v = scratchpads[arg->ui].cmd };

	for (c = selmon->clients; c && !(found = c->tags & scratchtag); c = c->next)
		;
	if (found) {
		unsigned int newtagset = selmon->tagset[selmon->seltags] ^ scratchtag;
		if (newtagset) {
			selmon->tagset[selmon->seltags] = newtagset;
			focus(NULL);
			arrange(selmon);
		}
		if (ISVISIBLE(c)) {
			focus(c);
			restack(selmon);
		}
	} else {
		selmon->tagset[selmon->seltags] |= scratchtag;
		spawn(&sparg);
	}
}
void center_window(Client *c)
{
	Monitor *m = c->mon;
	if (!c || !m)
		return;

	// 计算屏幕中心坐标（考虑窗口边框）
	int center_x = m->mx + (m->mw - WIDTH(c)) / 2;
	int center_y = m->my + (m->mh - HEIGHT(c)) / 2;

	// 更新窗口位置
	c->x = center_x;
	c->y = center_y;

	// 强制刷新窗口位置
	XMoveWindow(dpy, c->win, c->x, c->y);
}

void togglefloating(const Arg *arg)
{
	Client *c = selmon->sel;
	if (!c)
		return;

	if (!selmon->sel)
		return;
	if (selmon->sel->isfullscreen)
		return;

	// selmon->sel->isfloating = !selmon->sel->isfloating || selmon->sel->isfixed;
	c->isfloating = !c->isfloating || c->isfixed;

	if (selmon->sel->isfloating)
		// resize(selmon->sel, selmon->sel->x, selmon->sel->y, selmon->sel->w, selmon->sel->h, 0);
		resize(c, c->x, c->y, c->w, c->h, 0);

	// c->isfloating = !c->isfloating || c->isfixed;
	if (c->isfloating) {
		center_window(c);
	}
	arrange(selmon);
}

void toggletag(const Arg *arg)
{
	unsigned int newtags;

	if (!selmon->sel)
		return;
	newtags = selmon->sel->tags ^ (arg->ui & TAGMASK);
	if (newtags) {
		selmon->sel->tags = newtags;
		setclienttagprop(selmon->sel);
		focus(NULL);
		arrange(selmon);
	}
}

void toggleview(const Arg *arg)
{
	unsigned int newtagset = selmon->tagset[selmon->seltags] ^ (arg->ui & TAGMASK);
	int	     i;

	if (newtagset) {
		selmon->tagset[selmon->seltags] = newtagset;

		if (newtagset == ~0) {
			selmon->pertag->prevtag = selmon->pertag->curtag;
			selmon->pertag->curtag = 0;
		}

		/* test if the user did not select the same tag */
		if (!(newtagset & 1 << (selmon->pertag->curtag - 1))) {
			selmon->pertag->prevtag = selmon->pertag->curtag;
			for (i = 0; !(newtagset & 1 << i); i++)
				;
			selmon->pertag->curtag = i + 1;
		}

		/* apply settings for this view */
		selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag];
		selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag];
		selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag];
		selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt];
		selmon->lt[selmon->sellt ^ 1] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt ^ 1];

		if (selmon->showbar != selmon->pertag->showbars[selmon->pertag->curtag])
			togglebar(NULL);

		focus(NULL);
		arrange(selmon);
	}
}

void freeicon(Client *c)
{
	if (c->icon) {
		XRenderFreePicture(dpy, c->icon);
		c->icon = None;
	}
}

void unfocus(Client *c, int setfocus)
{
	if (!c)
		return;
	grabbuttons(c, 0);
	XSetWindowBorder(dpy, c->win, scheme[SchemeNorm][ColBorder].pixel);
	if (setfocus) {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
}

void unmanage(Client *c, int destroyed)
{
	Monitor	      *m = c->mon;
	XWindowChanges wc;

	detach(c);
	detachstack(c);
	freeicon(c);
	if (!destroyed) {
		wc.border_width = c->oldbw;
		XGrabServer(dpy); /* avoid race conditions */
		XSetErrorHandler(xerrordummy);
		XSelectInput(dpy, c->win, NoEventMask);
		XConfigureWindow(dpy, c->win, CWBorderWidth, &wc); /* restore border */
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		setclientstate(c, WithdrawnState);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
	free(c);
	focus(NULL);
	updateclientlist();
	arrange(m);
}

void unmapnotify(XEvent *e)
{
	Client	    *c;
	XUnmapEvent *ev = &e->xunmap;

	if ((c = wintoclient(ev->window))) {
		if (ev->send_event)
			setclientstate(c, WithdrawnState);
		else
			unmanage(c, 0);
	} else if ((c = wintosystrayicon(ev->window))) {
		/* KLUDGE! sometimes icons occasionally unmap their windows, but do
		 * _not_ destroy them. We map those windows back */
		XMapRaised(dpy, c->win);
		updatesystray();
	}
}

void updatebars(void)
{
	unsigned int	     w;
	Monitor		    *m;
	XSetWindowAttributes wa = { .override_redirect = True, .background_pixmap = ParentRelative, .event_mask = ButtonPressMask | ExposureMask };
	XClassHint	     ch = { "dwm", "dwm" };
	for (m = mons; m; m = m->next) {
		if (m->barwin)
			continue;
		w = m->ww;
		if (showsystray && m == systraytomon(m))
			w -= getsystraywidth();
		// m->barwin = XCreateWindow(dpy, root, m->wx,      m->by,      w,              bh, 0, DefaultDepth(dpy, screen), CopyFromParent, DefaultVisual(dpy, screen), CWOverrideRedirect|CWBackPixmap|CWEventMask, &wa);
		m->barwin = XCreateWindow(dpy, root, m->wx + sp, m->by + vp, m->ww - 2 * sp, bh, 0, DefaultDepth(dpy, screen), CopyFromParent,
					  DefaultVisual(dpy, screen), CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);

		XDefineCursor(dpy, m->barwin, cursor[CurNormal]->cursor);

		if (showsystray && m == systraytomon(m))
			XMapRaised(dpy, systray->win);
		XMapRaised(dpy, m->barwin);
		XSelectInput(dpy, m->barwin, ButtonPressMask | PointerMotionMask);
		XSetClassHint(dpy, m->barwin, &ch);
	}
}

void updatebarpos(Monitor *m)
{
	m->wy = m->my;
	m->wh = m->mh;
	if (m->showbar) {
		m->wh = m->wh - vertpad - bh;
		m->by = m->topbar ? m->wy : m->wy + m->wh + vertpad;
		m->wy = m->topbar ? m->wy + bh + vp : m->wy;
	} else
		m->by = -bh - vp;
}

void updateclientlist(void)
{
	Client	*c;
	Monitor *m;

	XDeleteProperty(dpy, root, netatom[NetClientList]);
	for (m = mons; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend, (unsigned char *)&(c->win), 1);
}

int updategeom(void)
{
	int dirty = 0;

#ifdef XINERAMA
	if (XineramaIsActive(dpy)) {
		int		    i, j, n, nn;
		Client		   *c;
		Monitor		   *m;
		XineramaScreenInfo *info = XineramaQueryScreens(dpy, &nn);
		XineramaScreenInfo *unique = NULL;

		for (n = 0, m = mons; m; m = m->next, n++)
			;
		/* only consider unique geometries as separate screens */
		unique = ecalloc(nn, sizeof(XineramaScreenInfo));
		for (i = 0, j = 0; i < nn; i++)
			if (isuniquegeom(unique, j, &info[i]))
				memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
		XFree(info);
		nn = j;

		/* new monitors if nn > n */
		for (i = n; i < nn; i++) {
			for (m = mons; m && m->next; m = m->next)
				;
			if (m)
				m->next = createmon();
			else
				mons = createmon();
		}
		for (i = 0, m = mons; i < nn && m; m = m->next, i++)
			if (i >= n || unique[i].x_org != m->mx || unique[i].y_org != m->my || unique[i].width != m->mw || unique[i].height != m->mh) {
				dirty = 1;
				m->num = i;
				m->mx = m->wx = unique[i].x_org;
				m->my = m->wy = unique[i].y_org;
				m->mw = m->ww = unique[i].width;
				m->mh = m->wh = unique[i].height;
				updatebarpos(m);
			}
		/* removed monitors if n > nn */
		for (i = nn; i < n; i++) {
			for (m = mons; m && m->next; m = m->next)
				;
			while ((c = m->clients)) {
				dirty = 1;
				m->clients = c->next;
				detachstack(c);
				c->mon = mons;
				attach(c);
				attachstack(c);
			}
			if (m == selmon)
				selmon = mons;
			cleanupmon(m);
		}
		free(unique);
	} else
#endif /* XINERAMA */
	{ /* default monitor setup */
		if (!mons)
			mons = createmon();
		if (mons->mw != sw || mons->mh != sh) {
			dirty = 1;
			mons->mw = mons->ww = sw;
			mons->mh = mons->wh = sh;
			updatebarpos(mons);
		}
	}
	if (dirty) {
		selmon = mons;
		selmon = wintomon(root);
	}
	return dirty;
}

void updatenumlockmask(void)
{
	unsigned int	 i, j;
	XModifierKeymap *modmap;

	numlockmask = 0;
	modmap = XGetModifierMapping(dpy);
	for (i = 0; i < 8; i++)
		for (j = 0; j < modmap->max_keypermod; j++)
			if (modmap->modifiermap[i * modmap->max_keypermod + j] == XKeysymToKeycode(dpy, XK_Num_Lock))
				numlockmask = (1 << i);
	XFreeModifiermap(modmap);
}

void updatesizehints(Client *c)
{
	long	   msize;
	XSizeHints size;

	if (!XGetWMNormalHints(dpy, c->win, &size, &msize))
		/* size is uninitialized, ensure that size.flags aren't used */
		size.flags = PSize;
	if (size.flags & PBaseSize) {
		c->basew = size.base_width;
		c->baseh = size.base_height;
	} else if (size.flags & PMinSize) {
		c->basew = size.min_width;
		c->baseh = size.min_height;
	} else
		c->basew = c->baseh = 0;
	if (size.flags & PResizeInc) {
		c->incw = size.width_inc;
		c->inch = size.height_inc;
	} else
		c->incw = c->inch = 0;
	if (size.flags & PMaxSize) {
		c->maxw = size.max_width;
		c->maxh = size.max_height;
	} else
		c->maxw = c->maxh = 0;
	if (size.flags & PMinSize) {
		c->minw = size.min_width;
		c->minh = size.min_height;
	} else if (size.flags & PBaseSize) {
		c->minw = size.base_width;
		c->minh = size.base_height;
	} else
		c->minw = c->minh = 0;
	if (size.flags & PAspect) {
		c->mina = (float)size.min_aspect.y / size.min_aspect.x;
		c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
	} else
		c->maxa = c->mina = 0.0;
	c->isfixed = (c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh);
	c->hintsvalid = 1;
}

void updatestatus(void)
{
	if (!gettextprop(root, XA_WM_NAME, stext, sizeof(stext)))
		strcpy(stext, "dwm-" VERSION);
	drawbar(selmon);
	updatesystray();
}

void updatesystrayicongeom(Client *i, int w, int h)
{
	if (i) {
		i->h = bh;
		if (w == h)
			i->w = bh;
		else if (h == bh)
			i->w = w;
		else
			i->w = (int)((float)bh * ((float)w / (float)h));
		applysizehints(i, &(i->x), &(i->y), &(i->w), &(i->h), False);
		/* force icons into the systray dimensions if they don't want to */
		if (i->h > bh) {
			if (i->w == i->h)
				i->w = bh;
			else
				i->w = (int)((float)bh * ((float)i->w / (float)i->h));
			i->h = bh;
		}
	}
}

void updatesystrayiconstate(Client *i, XPropertyEvent *ev)
{
	long flags;
	int  code = 0;

	if (!showsystray || !i || ev->atom != xatom[XembedInfo] || !(flags = getatomprop(i, xatom[XembedInfo])))
		return;

	if (flags & XEMBED_MAPPED && !i->tags) {
		i->tags = 1;
		code = XEMBED_WINDOW_ACTIVATE;
		XMapRaised(dpy, i->win);
		setclientstate(i, NormalState);
	} else if (!(flags & XEMBED_MAPPED) && i->tags) {
		i->tags = 0;
		code = XEMBED_WINDOW_DEACTIVATE;
		XUnmapWindow(dpy, i->win);
		setclientstate(i, WithdrawnState);
	} else
		return;
	sendevent(i->win, xatom[Xembed], StructureNotifyMask, CurrentTime, code, 0, systray->win, XEMBED_EMBEDDED_VERSION);
}

void updatesystray(void)
{
	XSetWindowAttributes wa;
	XWindowChanges	     wc;
	Client		    *i;
	Monitor		    *m = systraytomon(NULL);
	unsigned int	     x = m->mx + m->mw;
	unsigned int	     sw = TEXTW(stext) - lrpad + systrayspacing;
	unsigned int	     w = 1;

	if (!showsystray)
		return;
	if (systrayonleft)
		x -= sw + lrpad / 2;
	if (!systray) {
		/* init systray */
		if (!(systray = (Systray *)calloc(1, sizeof(Systray))))
			die("fatal: could not malloc() %u bytes\n", sizeof(Systray));
		systray->win = XCreateSimpleWindow(dpy, root, x, m->by, w, bh, 0, 0, scheme[SchemeNorm][ColBg].pixel);
		wa.event_mask = ButtonPressMask | ExposureMask;
		wa.override_redirect = True;
		wa.background_pixel = scheme[SchemeSystray][ColBg].pixel;
		XSelectInput(dpy, systray->win, SubstructureNotifyMask);
		XChangeProperty(dpy, systray->win, netatom[NetSystemTrayOrientation], XA_CARDINAL, 32, PropModeReplace,
				(unsigned char *)&netatom[NetSystemTrayOrientationHorz], 1);
		XChangeWindowAttributes(dpy, systray->win, CWEventMask | CWOverrideRedirect | CWBackPixel, &wa);
		XMapRaised(dpy, systray->win);
		XSetSelectionOwner(dpy, netatom[NetSystemTray], systray->win, CurrentTime);
		if (XGetSelectionOwner(dpy, netatom[NetSystemTray]) == systray->win) {
			sendevent(root, xatom[Manager], StructureNotifyMask, CurrentTime, netatom[NetSystemTray], systray->win, 0, 0);
			XSync(dpy, False);
		} else {
			fprintf(stderr, "dwm: unable to obtain system tray.\n");
			free(systray);
			systray = NULL;
			return;
		}
	}
	for (w = 0, i = systray->icons; i; i = i->next) {
		/* make sure the background color stays the same */
		wa.background_pixel = scheme[SchemeSystray][ColBg].pixel;
		XChangeWindowAttributes(dpy, i->win, CWBackPixel, &wa);
		XMapRaised(dpy, i->win);
		w += systrayspacing;
		i->x = w;
		XMoveResizeWindow(dpy, i->win, i->x, 0, i->w, i->h);
		w += i->w;
		if (i->mon != m)
			i->mon = m;
	}
	w = w ? w + systrayspacing : 1;
	x -= w;
	XMoveResizeWindow(dpy, systray->win, x - sp, m->by + vp, w, bh);
	wc.x = x - sp;
	wc.y = m->by + vp;
	wc.width = w;
	wc.height = bh;
	wc.stack_mode = Above;
	wc.sibling = m->barwin;
	XConfigureWindow(dpy, systray->win, CWX | CWY | CWWidth | CWHeight | CWSibling | CWStackMode, &wc);
	XMapWindow(dpy, systray->win);
	XMapSubwindows(dpy, systray->win);
	/* redraw background */
	XSetForeground(dpy, drw->gc, scheme[SchemeSystray][ColBg].pixel);
	XFillRectangle(dpy, systray->win, drw->gc, 0, 0, w, bh);
	XSync(dpy, False);
}

void updatetitle(Client *c)
{
	if (!gettextprop(c->win, netatom[NetWMName], c->name, sizeof c->name))
		gettextprop(c->win, XA_WM_NAME, c->name, sizeof c->name);
	if (c->name[0] == '\0') /* hack to mark broken clients */
		strcpy(c->name, broken);
}

void updatewindowtype(Client *c)
{
	Atom state = getatomprop(c, netatom[NetWMState]);
	Atom wtype = getatomprop(c, netatom[NetWMWindowType]);

	if (state == netatom[NetWMFullscreen])
		setfullscreen(c, 1);
	if (wtype == netatom[NetWMWindowTypeDialog])
		c->isfloating = 1;
}

void updateicon(Client *c)
{
	freeicon(c);
	c->icon = geticonprop(c->win, &c->icw, &c->ich);
}

void updatewmhints(Client *c)
{
	XWMHints *wmh;

	if ((wmh = XGetWMHints(dpy, c->win))) {
		if (c == selmon->sel && wmh->flags & XUrgencyHint) {
			wmh->flags &= ~XUrgencyHint;
			XSetWMHints(dpy, c->win, wmh);
		} else
			c->isurgent = (wmh->flags & XUrgencyHint) ? 1 : 0;
		if (wmh->flags & InputHint)
			c->neverfocus = !wmh->input;
		else
			c->neverfocus = 0;
		XFree(wmh);
	}
}

void view(const Arg *arg)
{
	int	     i;
	unsigned int tmptag;

	if ((arg->ui & TAGMASK) == selmon->tagset[selmon->seltags])
		return;
	selmon->seltags ^= 1; /* toggle sel tagset */
	if (arg->ui & TAGMASK) {
		selmon->tagset[selmon->seltags] = arg->ui & TAGMASK;
		selmon->pertag->prevtag = selmon->pertag->curtag;

		if (arg->ui == ~0)
			selmon->pertag->curtag = 0;
		else {
			for (i = 0; !(arg->ui & 1 << i); i++)
				;
			selmon->pertag->curtag = i + 1;
		}
	} else {
		tmptag = selmon->pertag->prevtag;
		selmon->pertag->prevtag = selmon->pertag->curtag;
		selmon->pertag->curtag = tmptag;
	}

	selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag];
	selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag];
	selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag];
	selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt];
	selmon->lt[selmon->sellt ^ 1] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt ^ 1];

	if (selmon->showbar != selmon->pertag->showbars[selmon->pertag->curtag])
		togglebar(NULL);

	focus(NULL);
	arrange(selmon);
}

Client *wintoclient(Window w)
{
	Client	*c;
	Monitor *m;

	for (m = mons; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			if (c->win == w)
				return c;
	return NULL;
}

Client *wintosystrayicon(Window w)
{
	Client *i = NULL;

	if (!showsystray || !w)
		return i;
	for (i = systray->icons; i && i->win != w; i = i->next)
		;
	return i;
}

Monitor *wintomon(Window w)
{
	int	 x, y;
	Client	*c;
	Monitor *m;

	if (w == root && getrootptr(&x, &y))
		return recttomon(x, y, 1, 1);
	for (m = mons; m; m = m->next)
		if (w == m->barwin)
			return m;
	if ((c = wintoclient(w)))
		return c->mon;
	return selmon;
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's). Other types of errors call Xlibs
 * default error handler, which may call exit. */
int xerror(Display *dpy, XErrorEvent *ee)
{
	if (ee->error_code == BadWindow || (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch) ||
	    (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable) || (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable) ||
	    (ee->request_code == X_PolySegment && ee->error_code == BadDrawable) || (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch) ||
	    (ee->request_code == X_GrabButton && ee->error_code == BadAccess) || (ee->request_code == X_GrabKey && ee->error_code == BadAccess) ||
	    (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
		return 0;
	fprintf(stderr, "dwm: fatal error: request code=%d, error code=%d\n", ee->request_code, ee->error_code);
	return xerrorxlib(dpy, ee); /* may call exit */
}

int xerrordummy(Display *dpy, XErrorEvent *ee)
{
	return 0;
}

/* Startup Error handler to check if another window manager
 * is already running. */
int xerrorstart(Display *dpy, XErrorEvent *ee)
{
	die("dwm: another window manager is already running");
	return -1;
}

Monitor *systraytomon(Monitor *m)
{
	Monitor *t;
	int	 i, n;
	if (!systraypinning) {
		if (!m)
			return selmon;
		return m == selmon ? m : NULL;
	}
	for (n = 1, t = mons; t && t->next; n++, t = t->next)
		;
	for (i = 1, t = mons; t && t->next && i < systraypinning; i++, t = t->next)
		;
	if (systraypinningfailfirst && n < systraypinning)
		return mons;
	return t;
}

void zoom(const Arg *arg)
{
	Client *c = selmon->sel;

	if (!selmon->lt[selmon->sellt]->arrange || !c || c->isfloating)
		return;
	if (c == nexttiled(selmon->clients) && !(c = nexttiled(c->next)))
		return;
	pop(c);
}

int main(int argc, char *argv[])
{
	if (argc == 2 && !strcmp("-v", argv[1]))
		die("dwm-" VERSION);
	else if (argc != 1)
		die("usage: dwm [-v]");
	if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
		fputs("warning: no locale support\n", stderr);
	if (!(dpy = XOpenDisplay(NULL)))
		die("dwm: cannot open display");
	checkotherwm();
	setup();
#ifdef __OpenBSD__
	if (pledge("stdio rpath proc exec", NULL) == -1)
		die("pledge");
#endif /* __OpenBSD__ */
	scan();
	runAutostart();
	run();
	if (restart)
		execvp(argv[0], argv);
	cleanup();
	XCloseDisplay(dpy);
	return EXIT_SUCCESS;
}

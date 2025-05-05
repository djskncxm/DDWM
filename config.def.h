static const int newclientathead = 0; /* 定义新窗口在栈顶还是栈底 */

static const unsigned int borderpx = 1; /* border pixel of windows */
static const unsigned int snap = 32; /* snap pixel */
static const unsigned int gappx = 9; /* gaps between windows */
static const unsigned int systraypinning = 0; /* 0: sloppy systray follows selected monitor, >0: pin systray to monitor X */
static const unsigned int systrayonleft = 0; /* 0: systray in the right corner, >0: systray on left of status text */
static const unsigned int systrayspacing = 2; /* systray spacing */
static const int systraypinningfailfirst = 1; /* 1: if pinning fails, display systray on the first monitor, False: display systray on the last monitor*/
static const int showsystray = 1; /* 0 means no systray */
static const int showbar = 1; /* 0 means no bar */
static const int topbar = 1; /* 0 means bottom bar */
static const int vertpad            = 10;       /* vertical padding of bar */
static const int sidepad            = 10;       /* horizontal padding of bar */
static const int horizpadbar        = 2;        /* horizontal padding for statusbar */
static const int vertpadbar         = 0;        /* vertical padding for statusbar */
#define ICONSIZE 16 /* icon size */
#define ICONSPACING 5 /* space between icon and title */
static const char *fonts[] = {
	"LXGWWenKai:size=10",
	"Iosevka Nerd Font:size=10",
};
static const char dmenufont[] = "monospace:size=10";

static const char col_gray1[] = "#101419";
static const char col_gray2[] = "#15191e";
static const char col_gray3[] = "#1e1e2e";
static const char col_gray4[] = "#15191e";
static const char col_yellow[] = "#fcf0c7";
static const char col_green[] = "#76b97f";
static const char col_teal[] = "#70a5eb";
static const char col_magenta[] = "#c68aee";
static const char col_red[] = "#e05f65";
static const char col_blue[] = "#70a5eb";
static const char col_cyan[] = "#74bee9";
static const char col_write[] = "#FFFFFF";
static const char col_cccc[] = "#CCCCFF";
static const char col_xxxx[] = "#FFCCFF";
static const char col_liang[] = "#91d7e3";
static const char col_systray[] = "#f4dbd6";
static const char col_borderbar[] = "#101419";
static const char *colors[][3] = {
	/*               fg         bg         border   */
	[SchemeNorm] = { col_cccc, "#1e1e2e", "#1e1e2e" },
	[SchemeSel] = { col_gray4, col_cyan, col_cyan },
	[SchemeHov]  = { col_gray4, col_liang,  col_liang  },
	[SchemeHid]  = { col_liang,  col_gray1, col_liang  },
	[SchemeSystray] = { col_systray, col_systray,col_systray, },
};

typedef struct {
	const char *name;
	const void *cmd;
} Sp;
const char *spcmd1[] = { "st", "-n", "spterm", "-g", "120x34", NULL };
const char *spcmd2[] = { "st", "-n", "spfm", "-g", "145x44", "-e", "musicfox", NULL };

const char *spcmd3[] = { "st", "-n", "keepassxc", "-g", "145x44", "-e", "calcurse", NULL };
static Sp   scratchpads[] = {
	  /* name          cmd  */
	  { "spterm", spcmd1 },
	  { "spranger", spcmd2 },
	  { "keepassxc", spcmd3 },
};

/* tagging */
static const char *tags[] = { "", "", "󱍙", "󰘅", "", "󰢔", "", "", "󰹑" }; // bar显示

static const char *tagsel[][2] = {
	{ col_blue, col_gray3 },  { col_yellow, col_gray3 }, { col_green, col_gray3 }, { col_magenta, col_gray3 }, { "#FFCCFF", col_gray3 },
	{ "#99FFFF", col_gray3 }, { "#9966CC", col_gray3 },  { "#FF66CC", col_gray3 }, { "#CCCCFF", col_gray3 },
};
static const Rule rules[] = {
	/* xprop(1):
	 *	WM_CLASS(STRING) = instance, class
	 *	WM_NAME(STRING) = title
	 */
	/* class      instance    title       tags mask     isfloating   monitor */
	{ "Gimp", NULL, NULL, 0, 1, -1 },
	{ "Firefox", NULL, NULL, 1 << 8, 0, -1 },
	{ NULL, "spterm", NULL, SPTAG(0), 1, -1 },
	{ NULL, "spfm", NULL, SPTAG(1), 1, -1 },
	{ NULL, "keepassxc", NULL, SPTAG(2), 1, -1 },
	{ NULL, NULL, "图片查看器", 0, 1, -1 }, // qq图片查看器        浮动
	{ NULL, NULL, "腾讯频道", 0, 1, -1 }, // qq图片查看器        浮动
	{ NULL, NULL, "预览", 0, 1, -1 }, // qq图片查看器        浮动
	// { NULL, NULL, "图片查看器", 0, 1, -1 }, // qq图片查看器        浮动
	{ "wemeetapp", NULL, NULL, 0, 1, 1, },
};

/* layout(s) */
static const float mfact = 0.55; /* factor of master area size [0.05..0.95] */
static const int   nmaster = 1; /* number of clients in master area */
static const int   resizehints = 1; /* 1 means respect size hints in tiled resizals */
static const int   lockfullscreen = 1; /* 1 will force focus on the fullscreen window */

static const unsigned int ulinepad = 5; /* horizontal padding between the underline and tag */
static const unsigned int ulinestroke = 2; /* thickness / height of the underline */
static const unsigned int ulinevoffset = 0; /* how far above the bottom of the bar the line should appear */
static const int	  ulineall = 0; /* 1 to show underline on all tags, 0 for just the active ones */

static const Layout layouts[] = {
	/* symbol     arrange function */
	{ "[平铺]", tile }, /* first entry is default */
	{ "[散装]", NULL }, /* no layout function means floating behavior */
	{ "[重叠]", monocle },
};

/* key definitions */
#define MODKEY Mod4Mask
#define TAGKEYS(KEY, TAG)                                                                                         \
	{ MODKEY, KEY, view, { .ui = 1 << TAG } }, { MODKEY | ControlMask, KEY, toggleview, { .ui = 1 << TAG } }, \
		{ MODKEY | ShiftMask, KEY, tag, { .ui = 1 << TAG } }, { MODKEY | ControlMask | ShiftMask, KEY, toggletag, { .ui = 1 << TAG } },

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd)                                 \
	{                                          \
		.v = (const char *[])              \
		{                                  \
			"/bin/sh", "-c", cmd, NULL \
		}                                  \
	}

/* commands */
static char	   dmenumon[2] = "0"; /* component of dmenucmd, manipulated in spawn() */
static const char *dmenucmd[] = { "dmenu_run", "-m", dmenumon, "-fn", dmenufont, "-nb", col_gray1, "-nf", col_gray3, "-sb", col_cyan, "-sf", col_gray4, NULL };

static const char *volup[] = { "amixer", "set", "Master", "5%+", NULL };
static const char *voldown[] = { "amixer", "set", "Master", "5%-", NULL };

static const char *rofi[] = { "rofi", "-show", "drun", NULL };
static const char *rofi_window[] = { "rofi", "-show", "window", NULL };
static const char *termcmd[] = { "kitty", NULL };
static const char *flameshot[] = { "flameshot", "gui", NULL };

#include "movestack.c"
static const Key keys[] = {
	/* modifier                     key        function        argument */

	{ MODKEY, XK_d, spawn, { .v = rofi } },
	{ MODKEY, XK_Return, spawn, { .v = termcmd } },
	{ MODKEY, XK_b, togglebar, { 0 } },
	{ MODKEY, XK_j, focusstackvis, { .i = +1 } },
	{ MODKEY, XK_k, focusstackvis, { .i = -1 } },
	{ MODKEY | ShiftMask, XK_w, focusstackhid, { .i = +1 } },
	{ MODKEY | ShiftMask, XK_s, focusstackhid, { .i = -1 } },
	{ MODKEY, XK_i, incnmaster, { .i = +1 } },
	{ MODKEY, XK_p, incnmaster, { .i = -1 } },
	{ MODKEY, XK_h, setmfact, { .f = -0.05 } },
	{ MODKEY | ShiftMask, XK_j, movestack, { .i = +1 } },
	{ MODKEY | ShiftMask, XK_k, movestack, { .i = -1 } },
	{ MODKEY, XK_l, setmfact, { .f = +0.05 } },
	{ MODKEY, XK_Return, zoom, { 0 } },
	{ MODKEY, XK_Tab, view, { 0 } },
	{ MODKEY | ShiftMask, XK_q, killclient, { 0 } },
	{ MODKEY, XK_t, setlayout, { .v = &layouts[0] } },
	{ MODKEY, XK_f, setlayout, { .v = &layouts[1] } },
	{ MODKEY, XK_m, setlayout, { .v = &layouts[2] } },
	// { MODKEY, XK_space, setlayout, { 0 } },
	{ MODKEY | ShiftMask, XK_space, togglefloating, { 0 } },
	{ MODKEY, XK_0, view, { .ui = ~0 } },
	{ MODKEY | ShiftMask, XK_0, tag, { .ui = ~0 } },
	{ MODKEY, XK_comma, focusmon, { .i = -1 } },
	{ MODKEY, XK_period, focusmon, { .i = +1 } },
	{ MODKEY | ShiftMask, XK_comma, tagmon, { .i = -1 } },
	{ MODKEY, XK_minus, setgaps, { .i = -1 } },
	{ MODKEY, XK_equal, setgaps, { .i = +1 } },
	{ MODKEY | ShiftMask, XK_equal, setgaps, { .i = 0 } },
	{ MODKEY | ShiftMask, XK_period, tagmon, { .i = +1 } },
	{ MODKEY, XK_o, show, { 0 } },
	{ MODKEY | ShiftMask, XK_s, showall, { 0 } },
	{ MODKEY, XK_g, hide, { 0 } },
	TAGKEYS(XK_1, 0) TAGKEYS(XK_2, 1) TAGKEYS(XK_3, 2) TAGKEYS(XK_4, 3) TAGKEYS(XK_5, 4) TAGKEYS(XK_6, 5) TAGKEYS(XK_7, 6) TAGKEYS(XK_8, 7)
		TAGKEYS(XK_9, 8){ MODKEY | ShiftMask, XK_e, quit, { 0 } },
	{ MODKEY | ControlMask | ShiftMask, XK_q, quit, { 1 } },

	// ---------------------------------------------
	{ MODKEY, XK_z, spawn, { .v = flameshot } },
	{ MODKEY | ShiftMask, XK_d, spawn, { .v = rofi_window } },
	{ MODKEY, XK_space, togglescratch, { .ui = 0 } },
	{ MODKEY, XK_u, togglescratch, { .ui = 1 } },
	{ MODKEY, XK_x, togglescratch, { .ui = 2 } },
	{ MODKEY, XK_w, spawn, { .v = volup } },
	{ MODKEY, XK_s, spawn, { .v = voldown } },

	// ---------------------------------------------
};

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static const Button buttons[] = {
	/* click                event mask      button          function        argument */
	{ ClkLtSymbol, 0, Button1, setlayout, { 0 } },
	{ ClkLtSymbol, 0, Button3, setlayout, { .v = &layouts[2] } },
	{ ClkWinTitle, 0, Button1, togglewin, { 0 } },
	{ ClkWinTitle, 0, Button2, zoom, { 0 } },
	{ ClkStatusText, 0, Button2, spawn, { .v = termcmd } },
	{ ClkClientWin, MODKEY, Button1, movemouse, { 0 } },
	{ ClkClientWin, MODKEY, Button2, togglefloating, { 0 } },
	{ ClkClientWin, MODKEY, Button1, resizemouse, { 0 } },
	{ ClkTagBar, 0, Button1, view, { 0 } },
	{ ClkTagBar, 0, Button3, toggleview, { 0 } },
	{ ClkTagBar, MODKEY, Button1, tag, { 0 } },
	{ ClkTagBar, MODKEY, Button3, toggletag, { 0 } },
};

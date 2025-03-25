static const int newclientathead = 0; /* 定义新窗口在栈顶还是栈底 */
static const unsigned int borderpx = 1; /* border pixel of windows */
static const unsigned int snap = 32; /* snap pixel */
static const unsigned int gappx = 5; /* gaps between windows */
static const unsigned int systraypinning =
	0; /* 0: sloppy systray follows selected monitor, >0: pin systray to monitor X */
static const unsigned int systrayonleft =
	0; /* 0: systray in the right corner, >0: systray on left of status text */
static const unsigned int systrayspacing = 2; /* systray spacing */
static const int systraypinningfailfirst =
	1; /* 1: if pinning fails, display systray on the first monitor, False: display systray on the last monitor*/
static const int showsystray = 1; /* 0 means no systray */
static const int showbar = 1; /* 0 means no bar */
static const int topbar = 1; /* 0 means bottom bar */
static const char *fonts[] = {
	"monospace:size=10",
	"LXGWWenKai:size=10",
	"Iosevka Nerd Font:size=10",
};
static const char dmenufont[] = "monospace:size=10";

static const char col_gray1[] = "#101419";
static const char col_gray2[] = "#15191e";
static const char col_gray3[] = "#485263";
static const char col_gray4[] = "#15191e";
static const char col_yellow[] = "#f1cf8a";
static const char col_green[] = "#76b97f";
static const char col_teal[] = "#70a5eb";
static const char col_magenta[] = "#c68aee";
static const char col_red[] = "#e05f65";
static const char col_blue[] = "#70a5eb";
static const char col_cyan[] = "#74bee9";
static const char col_write[] = "#FFFFFF";
static const char col_cccc[] = "#CCCCFF";
static const char col_xxxx[] = "#FFCCFF";
static const char col_liang[] = "#99FFFF";
static const char col_borderbar[] = "#101419";
static const char *colors[][3] = {
	/*               fg         bg         border   */
	[SchemeNorm] = { col_gray3, col_gray1, col_gray2 },
	[SchemeSel] = { col_gray4, col_cyan, col_cyan },
	[SchemeSystray] = { "#fff0f5", col_blue, col_cccc, },
};

/* tagging */
static const char *tags[] = { "",  "", "󱍙", "󰘅", "",
			      "󰢔", "", "",  "󰹑" }; // bar显示

static const char *tagsel[][2] = {
	{ col_blue, col_borderbar },  { col_yellow, col_borderbar },
	{ col_green, col_borderbar }, { col_magenta, col_borderbar },
	{ "#FFCCFF", col_borderbar }, { "#99FFFF", col_borderbar },
	{ "#9966CC", col_borderbar }, { "#FF66CC", col_borderbar },
	{ "#CCCCFF", col_borderbar },
};
static const Rule rules[] = {
	/* xprop(1):
	 *	WM_CLASS(STRING) = instance, class
	 *	WM_NAME(STRING) = title
	 */
	/* class      instance    title       tags mask     isfloating   monitor */
	{ "Gimp", NULL, NULL, 0, 1, -1 },
	{ "Firefox", NULL, NULL, 1 << 8, 0, -1 },
};

/* layout(s) */
static const float mfact = 0.55; /* factor of master area size [0.05..0.95] */
static const int nmaster = 1; /* number of clients in master area */
static const int resizehints =
	1; /* 1 means respect size hints in tiled resizals */
static const int lockfullscreen =
	1; /* 1 will force focus on the fullscreen window */

static const Layout layouts[] = {
	/* symbol     arrange function */
	{ "[]=", tile }, /* first entry is default */
	{ "><>", NULL }, /* no layout function means floating behavior */
	{ "[M]", monocle },
};

/* key definitions */
#define MODKEY Mod4Mask
#define TAGKEYS(KEY, TAG)                                                      \
	{ MODKEY, KEY, view, { .ui = 1 << TAG } },                             \
		{ MODKEY | ControlMask, KEY, toggleview, { .ui = 1 << TAG } }, \
		{ MODKEY | ShiftMask, KEY, tag, { .ui = 1 << TAG } },          \
		{ MODKEY | ControlMask | ShiftMask,                            \
		  KEY,                                                         \
		  toggletag,                                                   \
		  { .ui = 1 << TAG } },

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd)                                 \
	{                                          \
		.v = (const char *[])              \
		{                                  \
			"/bin/sh", "-c", cmd, NULL \
		}                                  \
	}

/* commands */
static char dmenumon[2] =
	"0"; /* component of dmenucmd, manipulated in spawn() */
static const char *dmenucmd[] = { "dmenu_run", "-m",  dmenumon,	 "-fn",
				  dmenufont,   "-nb", col_gray1, "-nf",
				  col_gray3,   "-sb", col_cyan,	 "-sf",
				  col_gray4,   NULL };
static const char *rofi[] = { "rofi", "-show", "run", NULL };
static const char *termcmd[] = { "st", NULL };
static const char *flameshot[] = { "flameshot", "gui", NULL };

#include "movestack.c"
static const Key keys[] = {
	/* modifier                     key        function        argument */

	{ MODKEY, XK_d, spawn, { .v = rofi } },
	{ MODKEY, XK_Return, spawn, { .v = termcmd } },
	{ MODKEY, XK_b, togglebar, { 0 } },
	{ MODKEY, XK_j, focusstack, { .i = +1 } },
	{ MODKEY, XK_k, focusstack, { .i = -1 } },
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
	{ MODKEY, XK_space, setlayout, { 0 } },
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
	TAGKEYS(XK_1, 0) TAGKEYS(XK_2, 1) TAGKEYS(XK_3, 2) TAGKEYS(XK_4, 3)
		TAGKEYS(XK_5, 4) TAGKEYS(XK_6, 5) TAGKEYS(XK_7, 6) TAGKEYS(XK_8,
									   7)
			TAGKEYS(XK_9,
				8){ MODKEY | ShiftMask, XK_e, quit, { 0 } },
	{ MODKEY | ControlMask | ShiftMask, XK_q, quit, { 1 } },

	// ---------------------------------------------
	{ MODKEY, XK_n, spawn, { .v = flameshot } },
	// ---------------------------------------------
};

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static const Button buttons[] = {
	/* click                event mask      button          function        argument */
	{ ClkLtSymbol, 0, Button1, setlayout, { 0 } },
	{ ClkLtSymbol, 0, Button3, setlayout, { .v = &layouts[2] } },
	{ ClkWinTitle, 0, Button2, zoom, { 0 } },
	{ ClkStatusText, 0, Button2, spawn, { .v = termcmd } },
	{ ClkClientWin, MODKEY, Button1, movemouse, { 0 } },
	{ ClkClientWin, MODKEY, Button2, togglefloating, { 0 } },
	{ ClkClientWin, MODKEY, Button3, resizemouse, { 0 } },
	{ ClkTagBar, 0, Button1, view, { 0 } },
	{ ClkTagBar, 0, Button3, toggleview, { 0 } },
	{ ClkTagBar, MODKEY, Button1, tag, { 0 } },
	{ ClkTagBar, MODKEY, Button3, toggletag, { 0 } },
};

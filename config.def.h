/* appearance */
static int showbar           = 1; /* 0 means no bar */
static int topbar            = 1; /* 0 means bottom bar */
static const char *fonts[]   = { "SourceCodePro:size=13" };
static uint32_t colors[][3]  = {
	/*               fg          bg         */
	[SchemeNorm] = { 0xbbbbbbff, 0x000000ff },
	[SchemeSel]  = { 0xeeeeeeff, 0x427b58ff },
};

/* tagging */
static char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "0" };

/* layout name replacement values */
static const char *layouts[][2] = {
	/* layout name          replace */
	{ "rivertile - left",   "[]=" }, /* rivertile */
	{ "rivertile - right",  "=[]" },
	{ "rivertile - top",    "[^]" },
	{ "rivertile - bottom", "[_]" },
    { "left",               "[]=" }, /* rivercarro */
    { "right",              "=[]" },
    { "top",                "[^]" },
    { "bottom",             "[_]" },
    { "monocle",            "[M]" },
	{ NULL,                 "><>" }, /* no layout, last layout */
};

static const char *termcmd[] = { "footclient", NULL };

/* button definitions */
/* click can be ClkTagBar, ClkLayout, ClkMode, ClkTitle, ClkStatus */
static const Button buttons[] = {
	/* click     button      function  argument */
	{ ClkTagBar, BTN_LEFT,   command,  {.s = "set-focused-tags"} },
	{ ClkTagBar, BTN_RIGHT,  command,  {.s = "toggle-focused-tags"} },
	{ ClkTagBar, BTN_MIDDLE, command,  {.s = "set-view-tags"} },
	{ ClkTitle,  BTN_LEFT,   command,  {.s = "zoom"} },
	{ ClkStatus, BTN_MIDDLE, spawn,    {.v = termcmd } },
};

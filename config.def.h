/* appearance */
static int showbar           = 1; /* 0 means no bar */
static int topbar            = 1; /* 0 means bottom bar */
static const char *fonts[]   = { "monospace:size=10" };
static uint32_t colors[][3]  = {
	/*               fg          bg         */
	[SchemeNorm] = { 0xbbbbbbff, 0x222222ff },
	[SchemeSel]  = { 0xeeeeeeff, 0x005577ff },
};

/* tagging */
static char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

static const char *termcmd[] = { "foot", NULL };

/* button definitions */
/* click can be ClkTagBar, ClkLayout, ClkMode, ClkTitle, ClkStatus */
static const Button buttons[] = {
	/* click     button      function    argument */
	{ ClkTagBar, BTN_LEFT,   view,       {0} },
	{ ClkTagBar, BTN_RIGHT,  toggleview, {0} },
	{ ClkTagBar, BTN_MIDDLE, toggletag,  {0} },
	{ ClkTitle,  BTN_LEFT,   zoom,       {0} },
	{ ClkStatus, BTN_MIDDLE, spawn ,     {.v = termcmd } },
};

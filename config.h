/* appearance */
static const int showbar     = 1; /* 0 means no bar */
static const int topbar      = 1; /* 0 means bottom bar */
static const char *fonts[]   = { "monospace:size=10" };
static uint32_t colors[][3]  = {
	/*               fg          bg         */
	[SchemeNorm] = { 0xbbbbbbff, 0x222222ff },
	[SchemeSel]  = { 0xeeeeeeff, 0x005577ff },
};

/* tagging */
static char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

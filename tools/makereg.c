/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * This was developed by Christopher W. Olsen for the UbixOS Project.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors.
 * 2) Redistributions in binary form must reproduce the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors in the documentation and/or
 *    other materials provided with the distribution.
 * 3) Neither the name of the UbixOS Project nor the names of its contributors may be used to
 *    endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * makereg — host tool that writes the initial ubistry registry seed
 * (./ubistry.db) installed to /var/db/ubistry.db by tools/mkimage.sh.  The
 * defaults table below is the single source of truth for the out-of-the-box
 * desktop config; the daemon loads this text format at boot and round-trips it.
 *
 * Run from tools/:   cc -o /tmp/makereg makereg.c && (cd tools && /tmp/makereg)
 * The text format is one line per value: `/path = value` with "strings",
 * integers, or true/false.  Mirrors tools/makeuser.c.
 */

#include <stdio.h>
#include <stddef.h>

struct reg_entry
{
	const char *path;
	const char *value; /* already in file syntax: "quoted", 32, true */
};

static const struct reg_entry g_defaults[] = {
    /* Start menu: categories -> submenus (items/) or direct exec; an exec of
     * "@name" is a built-in action handled by the taskbar. */
    {"/views/startmenu/0/label", "\"Applications\""},
    {"/views/startmenu/0/items/0/label", "\"Terminal\""},
    {"/views/startmenu/0/items/0/exec", "\"/usr/bin/term\""},
    {"/views/startmenu/0/items/1/label", "\"NetSurf\""},
    {"/views/startmenu/0/items/1/exec", "\"/bin/nsfb\""},

    {"/views/startmenu/1/label", "\"Games\""},
    {"/views/startmenu/1/items/0/label", "\"vDoom\""},
    {"/views/startmenu/1/items/0/exec", "\"/usr/bin/vdoom\""},
    {"/views/startmenu/1/items/1/label", "\"Tessera\""},
    {"/views/startmenu/1/items/1/exec", "\"/usr/bin/tessera\""},
    {"/views/startmenu/1/items/2/label", "\"Cubitaire\""},
    {"/views/startmenu/1/items/2/exec", "\"/usr/bin/cubitaire\""},

    {"/views/startmenu/2/label", "\"Utilities\""},
    {"/views/startmenu/2/items/0/label", "\"Files\""},
    {"/views/startmenu/2/items/0/exec", "\"/usr/bin/files\""},
    {"/views/startmenu/2/items/1/label", "\"Disk Utility\""},
    {"/views/startmenu/2/items/1/exec", "\"/usr/bin/diskutil\""},
    {"/views/startmenu/2/items/2/label", "\"Activity Monitor\""},
    {"/views/startmenu/2/items/2/exec", "\"/usr/bin/activity\""},

    {"/views/startmenu/3/label", "\"Settings\""},
    {"/views/startmenu/3/exec", "\"/usr/bin/settings\""},

    {"/views/startmenu/4/label", "\"About\""},
    {"/views/startmenu/4/exec", "\"@about\""},

    /* System identity.  The machine name (uname nodename / gethostname) defaults
     * to uBix-WS001 — also the kernel default — and is the registry's source of
     * truth; a boot step sethostname()s it from here.  The network domain joins
     * later from network settings. */
    {"/system/hostname", "\"uBix-WS001\""},

    /* Desktop background: mode = image | solid | jailbars; each mode uses its
     * own params.  Colours are packed 0xRRGGBB stored as integers.  The system
     * default is jailbars; a user's choice persists and overrides it. */
    {"/views/desktop/mode", "\"image\""},
    {"/views/desktop/image", "\"/var/background/ubix.bmp\""}, /* system default wallpaper */
    {"/views/desktop/color", "2900136"},                      /* 0x2C60A8 solid blue        */
    {"/views/desktop/barcolor", "1710638"},                   /* 0x1A1A2E jailbar base shade */

    /* Per-user wallpaper overrides: root and reddawg get the tropical-miami
     * wallpaper; everyone else (and the login screen, which is user-agnostic)
     * uses the ubix.bmp system default above. */
    {"/users/root/views/desktop/image", "\"/var/background/tropical-miami.png\""},
    {"/users/reddawg/views/desktop/image", "\"/var/background/tropical-miami.png\""},

    /* Theme: accent colour for focused window title bars (0xRRGGBB as int).
     * Retro magenta/purple to match the default synthwave (miami) wallpaper. */
    {"/views/theme/accent", "12595340"}, /* 0xC0308C magenta-purple */

    /* Image choices offered by the Settings Desktop pane. */
    {"/settings/wallpapers/0", "\"/var/background/ubix.bmp\""},
    {"/settings/wallpapers/1", "\"/var/background/tropical-sunset.png\""},
    {"/settings/wallpapers/2", "\"/var/background/tropical-palms.png\""},
    {"/settings/wallpapers/3", "\"/var/background/tropical-miami.png\""},
    {"/settings/wallpapers/4", "\"/var/background/synthwave-classic.png\""},
    {"/settings/wallpapers/5", "\"/var/background/synthwave-outrun.png\""},
    {"/settings/wallpapers/6", "\"/var/background/synthwave-mountains.png\""},
    {"/settings/wallpapers/7", "\"/var/background/synthwave-road.png\""},
    {"/settings/wallpapers/8", "\"/var/background/synthwave-vapor.png\""},

    /* Base desktop settings. */
    {"/views/taskbar/height", "32"},

    /* Timezone: a POSIX TZ string.  The kernel clock is UTC; the taskbar (and any
     * POSIX app via $TZ) converts with localtime().  "EST5EDT,M3.2.0,M11.1.0" =
     * US Eastern with automatic DST (EST winter / EDT summer). */
    {"/system/timezone", "\"EST5EDT,M3.2.0,M11.1.0\""},

    /* Audio: master volume (0..100) and mute, owned by the aural mixer server
     * (read at startup, applied to the codec) and managed by the Settings Sound pane. */
    {"/aural/volume", "100"},
    {"/aural/mute", "false"},

    /* Display: the VBE mode number views sets at startup (0x118 = 1024x768x24,
     * the kernel default).  Settings writes the user's chosen mode here. */
    {"/display/mode", "\"0x118\""},

    /* Network: mode = dhcp | static.  The static ip/netmask/gateway/dns are
     * defaults shown by the Settings Network pane and applied only in static
     * mode.  bin/netcfg reads these at boot and pushes them to the kernel. */
    {"/net/mode", "\"dhcp\""},
    {"/net/ip", "\"10.0.2.50\""},
    {"/net/netmask", "\"255.255.255.0\""},
    {"/net/gateway", "\"10.0.2.2\""},
    {"/net/dns", "\"10.0.2.3\""},
};

int main(void)
{
	FILE *out = fopen("./ubistry.db", "w");
	size_t i;

	if (out == NULL)
	{
		perror("ubistry.db");
		return (1);
	}

	fprintf(out, "# UbixOS registry seed — generated by tools/makereg.c\n");
	fprintf(out, "# Format: /path = value   (\"strings\", integers, true/false)\n\n");

	for (i = 0; i < sizeof(g_defaults) / sizeof(g_defaults[0]); i++)
	{
		fprintf(out, "%s = %s\n", g_defaults[i].path, g_defaults[i].value);
		printf("%s = %s\n", g_defaults[i].path, g_defaults[i].value);
	}

	fclose(out);
	return (0);
}

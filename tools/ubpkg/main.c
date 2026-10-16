/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * ubpkg — host package tool.  See docs/design/packages-plan.md.
 */
#include <stdio.h>
#include <string.h>
#include "ubpkg.h"

/** Print the command summary. */
static void usage(void)
{
	fputs("usage: ubpkg <command> [options]\n"
	      "  keygen  <basename>                       make <basename>.key / .pub\n"
	      "  build   -n NAME -v VER -a ARCH -A ABI ... STAGEDIR   build a .upkg\n"
	      "  index   -R REPO -s SERIAL -a ARCH -A ABI -k KEY ... PKG...   build and sign an index\n"
	      "  verify  -k PUB INDEX                     check an index and its signature\n"
	      "  install --root DIR (--repo REPO -k PUB NAME | --no-verify FILE)\n",
	      stderr);
}

/** ubpkg entry point. */
int main(int argc, char **argv)
{
	if (argc < 2)
	{
		usage();
		return 2;
	}
	if (strcmp(argv[1], "keygen") == 0)
		return cmd_keygen(argc - 1, argv + 1);
	if (strcmp(argv[1], "build") == 0)
		return cmd_build(argc - 1, argv + 1);
	if (strcmp(argv[1], "index") == 0)
		return cmd_index(argc - 1, argv + 1);
	if (strcmp(argv[1], "verify") == 0)
		return cmd_verify(argc - 1, argv + 1);
	if (strcmp(argv[1], "install") == 0)
		return cmd_install(argc - 1, argv + 1);
	usage();
	return 2;
}

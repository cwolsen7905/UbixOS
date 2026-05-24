/*
 *	termcap.c	1.1	20/7/87		agc	Joypace Ltd
 *
 *	Copyright Joypace Ltd, London, UK, 1987. All rights reserved.
 *	This file may be freely distributed provided that this notice
 *	remains attached.
 *
 *	A public domain implementation of the termcap(3) routines.
 */
#include "sh.h"

#if defined(_VMS_POSIX) || defined(_OSD_POSIX) || defined(__ANDROID__) || defined(UBIXOS)
/*    efth      1988-Apr-29

    - Correct when TERM != name and TERMCAP is defined   [tgetent]
    - Correct the comparison for the terminal name       [tgetent]
    - Correct the value of ^x escapes                    [tgetstr]
    - Added %r to reverse row/column			 [tgoto]

     Paul Gillingwater <paul@actrix.gen.nz> July 1992
	- Modified to allow terminal aliases in termcap file
	- Uses TERMCAP environment variable for file only
*/

#include	<stdio.h>
#include	<string.h>

#define CAPABLEN	2

#define ISSPACE(c)  ((c) == ' ' || (c) == '\t' || (c) == '\r' || (c) == '\n')
#define ISDIGIT(x)  ((x) >= '0' && (x) <= '9')

char		*capab;		/* the capability itself */

#ifdef STDC_HEADERS
# include <stdlib.h>
# include <stdio.h>
#else
extern char	*getenv();	/* new, improved getenv */
# ifndef fopen
extern FILE	*fopen();	/* old fopen */
# endif
#endif

/*
 *	tgetent - get the termcap entry for terminal name, and put it
 *	in bp (which must be an array of 1024 chars). Returns 1 if
 *	termcap entry found, 0 if not found, and -1 if file not found.
 */
int
tgetent(char *bp, char *name)
{
#if defined(__ANDROID__) || defined(UBIXOS)
	/*
	 * Try /etc/termcap first so users can install proper entries.
	 * Fall back to a built-in vt100/linux entry when the file is absent.
	 */
	{
		FILE *fp = fopen("/etc/termcap", "r");
		if (fp != NULL) {
			/* Read the whole file and search for the terminal name. */
			char line[1024];
			size_t len = name ? strlen(name) : 0;
			int found = 0;
			bp[0] = '\0';
			while (fgets(line, sizeof(line), fp)) {
				/* Skip comments and blank lines. */
				if (line[0] == '#' || line[0] == '\n')
					continue;
				/* Match "name|..." at start of line. */
				if (len > 0 && strncmp(line, name, len) == 0 &&
				    (line[len] == '|' || line[len] == ':' ||
				     line[len] == '\n' || line[len] == '\\')) {
					/* Concatenate continuation lines. */
					size_t pos = 0;
					do {
						size_t ll = strlen(line);
						/* strip trailing newline / backslash */
						while (ll > 0 && (line[ll-1] == '\n' || line[ll-1] == '\\'))
							ll--;
						if (pos + ll >= 1023) ll = 1023 - pos;
						strncpy(bp + pos, line, ll);
						pos += ll;
						bp[pos] = '\0';
						/* continuation? peek at next line */
						{
							int c = fgetc(fp);
							if (c == EOF) break;
							ungetc(c, fp);
							if (c != ' ' && c != '\t') break;
						}
					} while (fgets(line, sizeof(line), fp));
					found = 1;
					break;
				}
			}
			fclose(fp);
			if (found && bp[0] != '\0') {
				capab = bp;
				return 1;
			}
			/* name not found in file */
			capab = bp;
			bp[0] = '\0';
			return 0;
		}
	}
	/* No /etc/termcap — use built-in vt100-compatible entry. */
	capab = bp;
	strcpy(bp,
	"vt100|VT100 compatible terminal:"
	":am:xn:"
	":co#80:li#24:"
	":cl=\\E[H\\E[J:cm=\\E[%i%d;%dH:cr=^M:"
	":do=^J:ho=\\E[H:le=^H:nd=\\E[C:up=\\E[A:"
	":ce=\\E[K:cd=\\E[J:sf=^J:sr=\\EM:"
	":so=\\E[7m:se=\\E[m:us=\\E[4m:ue=\\E[m:"
	":mb=\\E[5m:md=\\E[1m:mr=\\E[7m:me=\\E[m:"
	":k1=\\EOP:k2=\\EOQ:k3=\\EOR:k4=\\EOS:"
	":ku=\\E[A:kd=\\E[B:kr=\\E[C:kl=\\E[D:kb=^H:"
	":al=\\E[L:dl=\\E[M:ic=\\E[@:dc=\\E[P:"
	":sc=\\E7:rc=\\E8:"
	);
	return(1);
#else
	FILE	*fp;
	char	*termfile;
	char	*cp,
		*ptr,		/* temporary pointer */
		tmp[1024];	/* buffer for terminal name *//*FIXBUF*/
	size_t	len = strlen(name);

	capab = bp;

	/* Use TERMCAP to override default. */

	termfile = getenv("TERMCAP");
	if (termfile == NULL ) termfile = "/etc/termcap";

	if ((fp = fopen(termfile, "r")) == (FILE *) NULL) {
		fprintf(stderr, CGETS(31, 1,
		        "Can't open TERMCAP: [%s]\n"), termfile);
		fprintf(stderr, CGETS(31, 2, "Can't open %s.\n"), termfile);
		sleep(1);
		return(-1);
	}

	while (fgets(bp, 1024, fp) != NULL) {
		/* Any line starting with # or NL is skipped as a comment */
		if ((*bp == '#') || (*bp == '\n')) continue;

		/* Look for lines which end with two backslashes,
		and then append the next line. */
		while (*(cp = &bp[strlen(bp) - 2]) == '\\')
			fgets(cp, 1024, fp);

		/* Skip over any spaces or tabs */
		for (++cp ; ISSPACE(*cp) ; cp++);

		/*  Make sure "name" matches exactly  (efth)  */

/* Here we might want to look at any aliases as well.  We'll use
sscanf to look at aliases.  These are delimited by '|'. */

		sscanf(bp,"%[^|]",tmp);
		if (strncmp(name, tmp, len) == 0) {
			fclose(fp);
#ifdef DEBUG
	fprintf(stderr, CGETS(31, 3, "Found %s in %s.\n"), name, termfile);
	sleep(1);
#endif /* DEBUG */
			return(1);
		}
		ptr = bp;
		while ((ptr = strchr(ptr,'|')) != NULL) {
			ptr++;
			if (strchr(ptr,'|') == NULL) break;
			sscanf(ptr,"%[^|]",tmp);
			if (strncmp(name, tmp, len) == 0) {
				fclose(fp);
#ifdef DEBUG
	fprintf(stderr,CGETS(31, 3, "Found %s in %s.\n"), name, termfile);
	sleep(1);
#endif /* DEBUG */
				return(1);
			}
		}
	}
	/* If we get here, then we haven't found a match. */
	fclose(fp);
#ifdef DEBUG
	fprintf(stderr,CGETS(31, 4, "No match found for %s in file %s\n"),
		name, termfile);
	sleep(1);
#endif /* DEBUG */
	return(0);
#endif /* ANDROID */
}

/*
 *	tgetnum - get the numeric terminal capability corresponding
 *	to id. Returns the value, -1 if invalid.
 */
int
tgetnum(char *id)
{
	char	*cp;
	int	ret;

	if ((cp = capab) == NULL || id == NULL)
		return(-1);
	while (*++cp != ':')
		;
	for (++cp ; *cp ; cp++) {
		while (ISSPACE(*cp))
			cp++;
		if (strncmp(cp, id, CAPABLEN) == 0) {
			while (*cp && *cp != ':' && *cp != '#')
				cp++;
			if (*cp != '#')
				return(-1);
			for (ret = 0, cp++ ; *cp && ISDIGIT(*cp) ; cp++)
				ret = ret * 10 + *cp - '0';
			return(ret);
		}
		while (*cp && *cp != ':')
			cp++;
	}
	return(-1);
}

/*
 *	tgetflag - get the boolean flag corresponding to id. Returns -1
 *	if invalid, 0 if the flag is not in termcap entry, or 1 if it is
 *	present.
 */
int
tgetflag(char *id)
{
	char	*cp;

	if ((cp = capab) == NULL || id == NULL)
		return(-1);
	while (*++cp != ':')
		;
	for (++cp ; *cp ; cp++) {
		while (ISSPACE(*cp))
			cp++;
		if (strncmp(cp, id, CAPABLEN) == 0)
			return(1);
		while (*cp && *cp != ':')
			cp++;
	}
	return(0);
}

/*
 *	tgetstr - get the string capability corresponding to id and place
 *	it in area (advancing area at same time). Expand escape sequences
 *	etc. Returns the string, or NULL if it can't do it.
 */
char *
tgetstr(char *id, char **area)
{
	char	*cp;
	char	*ret;
	int	i;

	if ((cp = capab) == NULL || id == NULL)
		return(NULL);
	while (*++cp != ':')
		;
	for (++cp ; *cp ; cp++) {
		while (ISSPACE(*cp))
			cp++;
		if (strncmp(cp, id, CAPABLEN) == 0) {
			while (*cp && *cp != ':' && *cp != '=')
				cp++;
			if (*cp != '=')
				return(NULL);
			for (ret = *area, cp++; *cp && *cp != ':' ;
				(*area)++, cp++)
				switch(*cp) {
				case '^' :
					**area = *++cp - '@'; /* fix (efth)*/
					break;
				case '\\' :
					switch(*++cp) {
					case 'E' :
						**area = CTL_ESC('\033');
						break;
					case 'n' :
						**area = '\n';
						break;
					case 'r' :
						**area = '\r';
						break;
					case 't' :
						**area = '\t';
						break;
					case 'b' :
						**area = '\b';
						break;
					case 'f' :
						**area = '\f';
						break;
					case '0' :
					case '1' :
					case '2' :
					case '3' :
						for (i=0 ; *cp && ISDIGIT(*cp) ;
							 cp++)
							i = i * 8 + *cp - '0';
						**area = i;
						cp--;
						break;
					case '^' :
					case '\\' :
						**area = *cp;
						break;
					}
					break;
				default :
					**area = *cp;
				}
			*(*area)++ = '\0';
			return(ret);
		}
		while (*cp && *cp != ':')
			cp++;
	}
	return(NULL);
}

/*
 *	tgoto - given the cursor motion string cm, make up the string
 *	for the cursor to go to (destcol, destline), and return the string.
 *	Returns "OOPS" if something's gone wrong, or the string otherwise.
 */
char *
tgoto(char *cm, int destcol, int destline)
{
	char	*rp;
	static char	ret[24];
	int		incr = 0;
	int 		argno = 0, numval;

	for (rp = ret ; *cm ; cm++) {
		switch(*cm) {
		case '%' :
			switch(*++cm) {
			case '+' :
				numval = (argno == 0 ? destline : destcol);
				argno = 1 - argno;
				*rp++ = numval + incr + *++cm;
				break;

			case '%' :
				*rp++ = '%';
				break;

			case 'i' :
				incr = 1;
				break;

			case 'd' :
				numval = (argno == 0 ? destline : destcol);
				numval += incr;
				argno = 1 - argno;
				*rp++ = '0' + (numval/10);
				*rp++ = '0' + (numval%10);
				break;

			case 'r' :
				argno = 1;
				break;
			}

			break;
		default :
			*rp++ = *cm;
		}
	}
	*rp = '\0';
	return(ret);
}

/*
 *	tputs - put the string cp out onto the terminal, using the function
 *	outc. This should do padding for the terminal, but I can't find a
 *	terminal that needs padding at the moment...
 */
int
tputs(char *cp, int affcnt, int (*outc)(int))
{
	unsigned long delay = 0;

	if (cp == NULL)
		return(1);
	/* do any padding interpretation - left null for MINIX just now */
	for (delay = 0; *cp && ISDIGIT(*cp) ; cp++)
		delay = delay * 10 + *cp - '0';
	while (*cp)
		(*outc)(*cp++);
#ifdef _OSD_POSIX
	usleep(delay*100); /* strictly spoken, it should be *1000 */
#endif
	return(1);
}
#endif /* _VMS_POSIX || _OSD_POSIX */

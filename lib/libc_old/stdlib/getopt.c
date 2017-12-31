#include <sys/cdefs.h>
#include "../include/namespace.h"
#include <err.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/un-namespace.h"

#include "../include/libc_private.h"

int     opterr = 1,             /* if error message should be printed */
        optind = 1,             /* index into parent argv vector */
        optopt,                 /* character checked for validity */
        optreset;               /* reset getopt */
char    *optarg;                /* argument associated with option */

#define BADCH   (int)'?'
#define BADARG  (int)':'
#define EMSG    ""

int getopt(int nargc,char * const nargv[],const char *ostr) {
  static char *place = EMSG;              /* option letter processing */
  char *oli;                              /* option letter list index */

        if (optreset || *place == 0) {          /* update scanning pointer */
                optreset = 0;
                place = nargv[optind];
                if (optind >= nargc || *place++ != '-') {
                        /* Argument is absent or is not an option */
                        place = EMSG;
                        return (-1);
                }
                optopt = *place++;
                if (optopt == '-' && *place == 0) {
                        /* "--" => end of options */
                        ++optind;
                        place = EMSG;
                        return (-1);
                }
                if (optopt == 0) {
                        /* Solitary '-', treat as a '-' option
                           if the program (eg su) is looking for it. */
                        place = EMSG;
                        if (strchr(ostr, '-') == NULL)
                                return (-1);
                        optopt = '-';
                }
        } else
                optopt = *place++;

          /* See if option letter is one the caller wanted... */
        if (optopt == ':' || (oli = strchr(ostr, optopt)) == NULL) {
                if (*place == 0)
                        ++optind;
                if (opterr && *ostr != ':') {
                     printf("%s: illegal option -- %c\n","BUG",optopt);
                    /*
                        (void)fprintf(stderr,
                            "%s: illegal option -- %c\n", _getprogname(),
                            optopt);
                    */
                  }
                return (BADCH);
        }

    
       /* Does this option need an argument? */
        if (oli[1] != ':') {
                /* don't need argument */
                optarg = NULL;
                if (*place == 0)
                        ++optind;
        } else {
                /* Option-argument is either the rest of this argument or the
                   entire next argument. */
                if (*place)
                        optarg = place;
                else if (nargc > ++optind)
                        optarg = nargv[optind];
                else {
                        /* option-argument absent */
                        place = EMSG;
                        if (*ostr == ':')
                                return (BADARG);
                        if (opterr)
             printf("%s: option requires an arguement -- %c\n","BUG",optopt);
                                /*
                                (void)fprintf(stderr,
                                    "%s: option requires an argument -- %c\n",
                                    _getprogname(), optopt);
                                */
                        return (BADCH);
                }
                place = EMSG;
                ++optind;
        }
        return (optopt);                        /* return option letter */


  }

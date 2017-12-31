char **environ;
const char *__progname = "";

void _start(char *ap, ...) {
  int argc;
  char **argv;
  char **env;
  const char *s;

  argv = &ap;

  argc = *(long *)(void *)(argv - 1);
  env = argv + argc + 1;
  environ = env;

  if (env[0] != 0) 
  printf("env[0]: 0x%X\n",env[0]);

  /*
  printf("(&ap: 0x%X)\n",&ap);
  printf("(argv[0]: 0x%X\n",argv[0]);
  printf("(argv[-1]: 0x%X:0x%X)\n",argv[-1],*(argv - 1));
  */

  if (argc > 0 && argv[0] != 0x0) {
     __progname = argv[0];
     for (s = __progname; *s != '\0'; s++)
       if (*s == '/')
    __progname = s + 1;
    }

  exit(main(argc, argv, env));

  }
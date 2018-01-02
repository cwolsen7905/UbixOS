char **environ;
const char *__progname = "";

typedef struct {
  int a_type;
  int a_val;
} Elf32_Auxinfo;

void _start(unsigned int *ap, ...) {
  Elf32_Auxinfo *aux, *auxp;
  unsigned int *argcp;
  int argc;
  char **argv;
  char **env;

printf("{0x%X}", ap);

  argcp = ap;
  argc = *ap++;
  argv = (char **)ap;
  ap += argc + 1;
  env = (char **)ap;
  environ = (char **)ap;
  while (*ap++ != 0)
    ;
  aux = (Elf32_Auxinfo *) ap;

  for (auxp = aux; auxp->a_type != 0x0; auxp++) {
    printf("TEST");
  }
  printf("TEST2");
  

//  if (env[0] != 0) 
 //   printf("env[0]: 0x%X\n",env[0]);

  /*
  printf("(&ap: 0x%X)\n",&ap);
  printf("(argv[0]: 0x%X\n",argv[0]);
  printf("(argv[-1]: 0x%X:0x%X)\n",argv[-1],*(argv - 1));
  */

/*
  if (argc > 0 && argv[0] != 0x0) {
     __progname = argv[0];
     for (s = __progname; *s != '\0'; s++)
       if (*s == '/')
    __progname = s + 1;
    }
*/

  exit(main(argc, argv, env));

  }

#include <sys/types.h>

extern char *cwd;

struct argsStruct {
  struct argsStruct *next;
  char *arg;
  };
  
typedef struct {
  int argc;
  char **argv;
  char **envp;
  uInt8 bg;
  struct argsStruct *args;
  } inputBuffer;

void parseInput(inputBuffer *,char *);
int commands(inputBuffer *);
void execProgram(inputBuffer *);
void freeArgs(inputBuffer *ptr);
void error(int errorCode,const char *errorMsg);

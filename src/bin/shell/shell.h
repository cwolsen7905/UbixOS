#include <sys/types.h>

extern char *cwd;
extern int   boo;

struct argsStruct {
  struct argsStruct *next;
  char *arg;
  };
  
typedef struct {
  uInt8 bg;
  struct argsStruct *args;
  int argc;
  char *argv;
  char **envp;
  } inputBuffer;

void parseInput(inputBuffer *,char *);
int commands(inputBuffer *);
void execProgram(inputBuffer *);
void freeArgs(inputBuffer *ptr);
void error(int errorCode,const char *errorMsg);

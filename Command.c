#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

#include "Command.h"
#include "error.h"

typedef struct {
  char *file;
  char **argv;
  char *from;
  char *to;
} *CommandRep;

#define BIARGS CommandRep r, int *eof, Jobs jobs
#define BINAME(name) bi_##name
#define BIDEFN(name) static void BINAME(name) (BIARGS)
#define BIENTRY(name) {#name,BINAME(name)}

static char *owd=0;
static char *cwd=0;

static void builtin_args(CommandRep r, int n) {
  char **argv=r->argv;
  for (n++; *argv++; n--);
  if (n)
    ERROR("wrong number of arguments to builtin command"); // warn
}

BIDEFN(exit) {
  builtin_args(r,0);
  *eof=1;
}

BIDEFN(pwd) {
  builtin_args(r,0);
  if (!cwd)
    cwd=getcwd(0,0);
  printf("%s\n",cwd);
}

BIDEFN(cd) {
  builtin_args(r,1);
  if (strcmp(r->argv[1],"-")==0) {
    char *twd=cwd;
    cwd=owd;
    owd=twd;
  } else {
    if (owd) free(owd);
    owd=cwd;
    cwd=strdup(r->argv[1]);
  }
  if (cwd && chdir(cwd))
    ERROR("chdir() failed"); // warn
}


typedef struct {
  char *s;
  void (*f)(BIARGS);
} Builtin;

static const Builtin builtins[]={
  BIENTRY(exit),
  BIENTRY(pwd),
  BIENTRY(cd),
  {0,0}
};

static const Builtin *findBuiltin(CommandRep r) {
  for (int i=0; builtins[i].s; i++)
    if (!strcmp(r->file,builtins[i].s))
      return &builtins[i];
  return 0;
}

static int builtin(BIARGS) {
  const Builtin *b=findBuiltin(r);
  if (!b)
    return 0;
  b->f(r,eof,jobs);
  return 1;
}

static char **getargs(T_words words) {
  int n=0;
  T_words p=words;
  while (p) {
    p=p->words;
    n++;
  }
  char **argv=(char **)malloc(sizeof(char *)*(n+1));
  if (!argv)
    ERROR("malloc() failed");
  p=words;
  int i=0;
  while (p) {
    argv[i++]=strdup(p->word->s);
    p=p->words;
  }
  argv[i]=0;
  return argv;
}

extern Command newCommand(T_words words, char *from, char *to) {
  CommandRep r=(CommandRep)malloc(sizeof(*r));
  if (!r)
    ERROR("malloc() failed");
  r->argv=getargs(words);
  r->file=r->argv[0];
  r->from=from ? strdup(from) : 0;
  r->to=to ? strdup(to) : 0;
  return r;
}

static void redirectChild(CommandRep r) {
  if (r->from) {
    int fd=open(r->from,O_RDONLY);
    if (fd<0) { perror(r->from); _exit(1); }
    dup2(fd,STDIN_FILENO);
    close(fd);
  }
  if (r->to) {
    int fd=open(r->to,O_WRONLY|O_CREAT|O_TRUNC,0666);
    if (fd<0) { perror(r->to); _exit(1); }
    dup2(fd,STDOUT_FILENO);
    close(fd);
  }
}

static void child(CommandRep r, int fg) {
  int eof=0;
  redirectChild(r);
  Jobs jobs=newJobs();
  if (builtin(r,&eof,jobs)) {
    freeJobs(jobs);
    fflush(stdout);
    _exit(0);
  }
  
  execvp(r->argv[0],r->argv);
  // ERROR("execvp() failed");
  WARN("%s: execvp() failed", r->argv[0]);
  _exit(127);
}


static void restoreRedirect(int oin, int oout) {
  fflush(stdout);   // push builtin output into the file BEFORE switching back
  if (oin>=0)  { dup2(oin,STDIN_FILENO);   close(oin); }
  if (oout>=0) { dup2(oout,STDOUT_FILENO); close(oout); }
}

// Returns 0 on success. On failure, warns and leaves fds as they were.
static int redirectShell(CommandRep r, int *oin, int *oout) {
  *oin=*oout=-1;
  if (r->from) {
    int fd=open(r->from,O_RDONLY);
    if (fd<0) { WARN("%s: %s",r->from,strerror(errno)); return -1; }
    *oin=dup(STDIN_FILENO);
    dup2(fd,STDIN_FILENO);
    close(fd);
  }
  if (r->to) {
    int fd=open(r->to,O_WRONLY|O_CREAT|O_TRUNC,0666);
    if (fd<0) {
      WARN("%s: %s",r->to,strerror(errno));
      restoreRedirect(*oin,-1);
      return -1;
    }
    fflush(stdout);
    *oout=dup(STDOUT_FILENO);
    dup2(fd,STDOUT_FILENO);
    close(fd);
  }
  return 0;
}

extern int execCommand(Command command, Pipeline pipeline, Jobs jobs,
			int *jobbed, int *eof, int fg) {
  CommandRep r=command;
  if (fg && findBuiltin(r)) {
    int oin, oout;
    if (redirectShell(r, &oin, &oout)==0) {
      builtin(r,eof,jobs);
      restoreRedirect(oin, oout);
    }
    return 0; // no child wait 
  }
  fflush(stdout); // child wont get unflushed output
  int pid=fork();
  if (pid==-1) 
    ERROR("fork() failed");
  if (pid==0)
    child(r, fg);
  return pid; // waits
}

extern void freeCommand(Command command) {
  CommandRep r=command;
  char **argv=r->argv;
  while (*argv)
    free(*argv++);
  free(r->argv);
  if (r->from) free(r->from);
  if (r->to) free(r->to);
  free(r);
}

extern void freestateCommand() {
  if (cwd) free(cwd);
  if (owd) free(owd);
}

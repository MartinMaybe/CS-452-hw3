#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <readline/history.h>

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
// static char *cwd=0;

// Returns -1 after a warning if there are more than max args
static int builtin_args(CommandRep r, int max) {
  int n=0;
  while (r->argv[n+1])
    n++;
  if (n>max) {
    WARN("wrong number of arguments to builtin command");
    return -1;
  }
  return 0;
}

// Handles exit
BIDEFN(exit) {
  if (builtin_args(r,0)) return;
  *eof=1;
}

// Handles pwd builtin, added warnings
BIDEFN(pwd) {
  if (builtin_args(r,0)) return;
  char *d=getcwd(0,0);
  if (!d) { WARN("getcwd: %s",strerror(errno)); return; }
  printf("%s\n",d);
  free(d);
}

// Handles cd builtin, added HOME and - to return to previous directory
BIDEFN(cd) {
  if (builtin_args(r,1)) return;
  const char *target=r->argv[1];
  if (!target) {
    target=getenv("HOME");
    if (!target) { WARN("cd: HOME not set"); return; }
  } else if (!strcmp(target,"-")) {
    if (!owd) { WARN("cd: no previous directory"); return; }
    target=owd;
  }
  char *prev=getcwd(0,0);
  if (chdir(target)) {
    WARN("cd: %s: %s",target,strerror(errno));
    free(prev);
    return;
  }
  free(owd);
  owd=prev;
}

// Handles history builtin, prints them with entry numbers
BIDEFN(history) {
  if (builtin_args(r,0)) return;
  HIST_ENTRY **l=history_list();
  if (!l) return;
  for (int i=0; l[i]; i++)
    printf("%5d  %s\n",history_base+i,l[i]->line);
}

// Stores the builtin commands and handlers
typedef struct {
  char *s;
  void (*f)(BIARGS);
} Builtin;

static const Builtin builtins[]={
  BIENTRY(exit),
  BIENTRY(pwd),
  BIENTRY(cd),
  BIENTRY(history),
  {0,0}
};

// Finds command from table
static const Builtin *findBuiltin(CommandRep r) {
  for (int i=0; builtins[i].s; i++)
    if (!strcmp(r->file,builtins[i].s))
      return &builtins[i];
  return 0;
}

// Finds and executes command 
static int builtin(BIARGS) {
  const Builtin *b=findBuiltin(r);
  if (!b)
    return 0;
  b->f(r,eof,jobs);
  return 1;
}

// Allocates memory for the array and duplicates argument string
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

// Creates a command structure and redirection paths, stores everything
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

// Applies redirection in a child process
// Exits the child process if a redirection file can't be opened
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

// Configures the child's input/output file descriptor
static void child(CommandRep r, int fg, int in, int out, int unused) {
  if (unused>=0)
    close(unused);
  if (in>=0) {
    dup2(in,STDIN_FILENO);
    close(in);
  }
  if (out>=0) {
    dup2(out,STDOUT_FILENO);
    close(out);
  }
  redirectChild(r);           // < and > override the pipe

  int eof=0;
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

// Restores the shell's standard input and output after redirection
static void restoreRedirect(int oin, int oout) {
  fflush(stdout);   // push builtin output into the file BEFORE switching back
  if (oin>=0)  { dup2(oin,STDIN_FILENO);   close(oin); }
  if (oout>=0) { dup2(oout,STDOUT_FILENO); close(oout); }
}

// Returns 0 on success
// On failure, warns and leaves fds as they were
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

// Executes a command either directly or in a new child process
// Handles redirection and process creation
extern int execCommand(Command command, Pipeline pipeline, Jobs jobs,
			int *jobbed, int *eof, int fg,
      int in, int out, int unused) {
  CommandRep r=command;
  if (fg && sizePipeline(pipeline)==1 && findBuiltin(r)) {
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
    child(r, fg, in, out, unused);
  return pid; // waits
}

// Frees all dynamically allocated memory associated with a command
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
  // if (cwd) free(cwd);
  if (owd) free(owd);
}

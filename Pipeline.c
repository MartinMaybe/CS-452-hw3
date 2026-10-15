#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#include "Pipeline.h"
#include "deq.h"
#include "error.h"

typedef struct {
  Deq processes;
  int fg;			// not "&"
  int *pids;
  int npids;
} *PipelineRep;

// New pipeline empty queue
extern Pipeline newPipeline(int fg) {
  PipelineRep r=(PipelineRep)malloc(sizeof(*r));
  if (!r)
    ERROR("malloc() failed");
  r->processes=deq_new();
  r->fg=fg;
  r->pids=0;
  r->npids=0;
  return r;
}

// Add command to queue
extern void addPipeline(Pipeline pipeline, Command command) {
  PipelineRep r=(PipelineRep)pipeline;
  deq_tail_put(r->processes,command);
}

// return command # in pipeline
extern int sizePipeline(Pipeline pipeline) {
  PipelineRep r=(PipelineRep)pipeline;
  return deq_len(r->processes);
}

// Executes each command in the pipeline, connecting commands through pipes and tracking child process IDs
static void execute(Pipeline pipeline, Jobs jobs, int *jobbed, int *eof) {
  PipelineRep r=(PipelineRep)pipeline;
  int n=sizePipeline(r);
  r->pids=calloc(n>0?n:1,sizeof(int));
  if (!r->pids)
    ERROR("calloc() failed");

  int in=-1;                         // read end feeding the next command
  for (int i=0; i<n && !*eof; i++) {
    int fd[2]={-1,-1};
    if (i<n-1 && pipe(fd)==-1)
      ERROR("pipe() failed");
    int pid=execCommand(deq_head_ith(r->processes,i),pipeline,jobs,
			jobbed,eof,r->fg,in,fd[1],fd[0]);
    if (pid>0)
      r->pids[r->npids++]=pid;
    if (in>=0)                       // parent is done with these
      close(in);
    if (fd[1]>=0)
      close(fd[1]);
    in=fd[0];
  }
  if (in>=0)                         // loop ended early
    close(in);
}

// Return 1 if every child has been reaped, block=0 polls
extern int waitPipeline(Pipeline pipeline, int block) {
  PipelineRep r=(PipelineRep)pipeline;
  int alive=0;
  for (int i=0; i<r->npids; i++) {
    if (r->pids[i]==0)
      continue;
    int status;
    pid_t w;
    do {
      w=waitpid(r->pids[i],&status,block?0:WNOHANG);
    } while (w==-1 && errno==EINTR);
    if (w==0)
      alive++;                // still running
    else
      r->pids[i]=0;           // reaped or gone
  }
  return alive==0;
}

extern void execPipeline(Pipeline pipeline, Jobs jobs, int *eof) {
  PipelineRep r = (PipelineRep)pipeline;
  int jobbed=0;
  execute(pipeline,jobs,&jobbed,eof);
  if (r->fg || r->npids==0) {
    waitPipeline(pipeline, 1);
    freePipeline(pipeline);	// for fg builtins, and such
  } else {
    addJobs(jobs, pipeline); 
  }
}

extern void freePipeline(Pipeline pipeline) {
  PipelineRep r=(PipelineRep)pipeline;
  deq_del(r->processes,freeCommand);
  free(r->pids);
  free(r);
}

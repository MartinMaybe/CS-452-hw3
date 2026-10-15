#include "Jobs.h"
#include "deq.h"
#include "error.h"

extern Jobs newJobs() {
  return deq_new();
}

extern void addJobs(Jobs jobs, Pipeline pipeline) {
  deq_tail_put(jobs,pipeline);
}

extern int sizeJobs(Jobs jobs) {
  return deq_len(jobs);
}

extern void freeJobs(Jobs jobs) {
  deq_del(jobs,freePipeline);
}

// Drop and free jobs whose children have exited
extern void reapJobs(Jobs jobs) {
  int i=0;
  while (i<deq_len(jobs)) {
    Pipeline p=deq_head_ith(jobs,i);
    if (waitPipeline(p,0)) {
      deq_head_rem(jobs,p);
      freePipeline(p);
    } else {
      i++;
    }
  }
}

// Block until every remaining job is finished
extern void waitJobs(Jobs jobs) {
  while (deq_len(jobs)) {
    Pipeline p=deq_head_get(jobs);
    waitPipeline(p,1);
    freePipeline(p);
  }
}

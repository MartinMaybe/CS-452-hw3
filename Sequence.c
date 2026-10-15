#include "Sequence.h"
#include "deq.h"
#include "error.h"

// New and empty sequence
extern Sequence newSequence() {
  return deq_new();
}

// Add pipeline to sequence
extern void addSequence(Sequence sequence, Pipeline pipeline) {
  deq_tail_put(sequence,pipeline);
}

// Free sequence and pipelines
extern void freeSequence(Sequence sequence) {
  deq_del(sequence,freePipeline);
}

// Executes sequence
extern void execSequence(Sequence sequence, Jobs jobs, int *eof) {
  while (deq_len(sequence) && !*eof)
    execPipeline(deq_head_get(sequence),jobs,eof);
  freeSequence(sequence);
}

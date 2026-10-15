#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Interpreter.h"
#include "Parser.h"
#include "Tree.h"
#include "Scanner.h"
#include "error.h"

static Scanner scan;

#undef ERROR
#define ERROR(s) ERRORLOC(__FILE__,__LINE__,"error","%s (pos: %d)",s,posScanner(scan))

static char *next()       { return nextScanner(scan); }
static char *curr()       { return currScanner(scan); }
static int   cmp(char *s) { return cmpScanner(scan,s); }
static int   eat(char *s) { return eatScanner(scan,s); }

static T_word p_word();
static T_words p_words();
static T_command p_command();
static T_pipeline p_pipeline();
static T_sequence p_sequence();

static void f_word(T_word t);
static void f_words(T_words t);
static void f_command(T_command t);
static void f_pipeline(T_pipeline t);
static void f_sequence(T_sequence t);

// Get current scanner and advances the scanner
static T_word p_word() {
  char *s=curr();
  if (!s)
    return 0;
  T_word word=new_word();
  word->s=strdup(s);
  next();
  return word;
}

// Parses consecutive words
static T_words p_words() {
  T_word word=p_word();
  if (!word)
    return 0;
  T_words words=new_words();
  words->word=word;
  if (cmp("|") || cmp("&") || cmp(";") || cmp("<") || cmp(">"))
    return words;
  words->words=p_words();
  return words;
}

// Parses command's words and redirection
static T_command p_command() {
  T_words words=p_words();
  if (!words)
    return 0;
  T_command command=new_command();
  command->words=words;
  if (eat("<")) {
    command->from=p_word();
    if (!command->from)
    ERROR("missing file after <");
  }
  if (eat(">")) {
    command->to=p_word();
    if (!command->to)
    ERROR("missing file after >");
  }
  return command;
}

// Parses command and operator connected commands 
static T_pipeline p_pipeline() {
  T_command command=p_command();
  if (!command)
    return 0;
  T_pipeline pipeline=new_pipeline();
  pipeline->command=command;
  if (eat("|"))
    pipeline->pipeline=p_pipeline();
  return pipeline;
}

// Parses pipelines connected by & / ;
static T_sequence p_sequence() {
  T_pipeline pipeline=p_pipeline();
  if (!pipeline)
    return 0;
  T_sequence sequence=new_sequence();
  sequence->pipeline=pipeline;
  if (eat("&")) {
    sequence->op="&";
    sequence->sequence=p_sequence();
  }
  if (eat(";")) {
    sequence->op=";";
    sequence->sequence=p_sequence();
  }
  return sequence;
}

// Parses pipeline and following separator
static T_sequence p_pipeline_as_sequence() {
  T_pipeline pipeline=p_pipeline();
  if (!pipeline)
    return 0;
  T_sequence sequence=new_sequence();
  sequence->pipeline=pipeline;
  if (eat("&"))
    sequence->op="&";
  if (eat(";"))
    sequence->op=";";
  return sequence;
}

// Interprets a line of shell input
extern void interpret(char *line, int *eof, Jobs jobs) {
  scan=newScanner(line);
  while (1) {
    T_sequence sequence=p_pipeline_as_sequence();
    if (!sequence)
      break;
    interpretSequence(sequence,eof,jobs);
    f_sequence(sequence);
  }
  freeScanner(scan);
}

// Free word node
static void f_word(T_word t) {
  if (!t)
    return;
  if (t->s)
    free(t->s);
  free(t);
}

// Free linked list of words
static void f_words(T_words t) {
  if (!t)
    return;
  f_word(t->word);
  f_words(t->words);
  free(t);
}

// Free command node
static void f_command(T_command t) {
  if (!t)
    return;
  f_words(t->words);
  f_word(t->from);
  f_word(t->to);
  free(t);
}

// Free pipeline
static void f_pipeline(T_pipeline t) {
  if (!t)
    return;
  f_command(t->command);
  f_pipeline(t->pipeline);
  free(t);
}

// Free sequence
static void f_sequence(T_sequence t) {
  if (!t)
    return;
  f_pipeline(t->pipeline);
  f_sequence(t->sequence);
  free(t);
}

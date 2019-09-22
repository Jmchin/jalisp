#include <stdio.h>
#include <stdlib.h>

#include "mpc.h"

#ifdef _WIN32
#include <string.h>

static char buffer[2048];

char* readline(char* prompt) {
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char* cpy = malloc(strlen(buffer)+1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy)-1] = '\0';   // C strings are null-terminated
  return cpy;
}

void add_history(char* unused) {}

#else
#include <readline/readline.h>
#include <readline/history.h>
#endif

 /* lisp value  */
typedef struct {
  int type;
  long num;
  int err;
} lval;

/* possible lval types */
enum { LVAL_NUM, LVAL_ERR };

/* possible lval errors */
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

/* create new number type lval */
lval lval_num(long x) {
  lval v;
  v.type = LVAL_NUM;
  v.num = x;
  return v;
}

/* create new error type lval */
lval lval_err(int x) {
  lval v;
  v.type = LVAL_ERR;
  v.err = x;
  return v;
}

void lval_print(lval v) {
  switch(v.type) {
    case LVAL_NUM:
      printf("%li", v.num);
      break;
    case LVAL_ERR:
      if (v.err == LERR_DIV_ZERO) {
        printf("Error: Division by zero");
      }
      if (v.err == LERR_BAD_OP) {
        printf("Error: Invalid operator");
      }
      if (v.err == LERR_BAD_NUM) {
        printf("Error: Invalid number");
      }
      break;
  }
}

void lval_println(lval v) { lval_print(v); putchar('\n'); }

lval eval_op(lval x, char* op, lval y) {
  /* if error, return it */
  if (x.type == LVAL_ERR) { return x; }
  if (y.type == LVAL_ERR) { return y; }

  /* else evaluate */
  if ((strcmp(op, "+") == 0) || strcmp(op, "add") == 0)  { return lval_num(x.num + y.num); }
  if ((strcmp(op, "-") == 0) || strcmp(op, "sub") == 0)  { return lval_num(x.num - y.num); }
  if ((strcmp(op, "*") == 0) || strcmp(op, "mult") == 0) { return lval_num(x.num * y.num); }

  if ((strcmp(op, "/") == 0) || strcmp(op, "div") == 0) {
    return y.num == 0 ? lval_err(LERR_DIV_ZERO)
                      : lval_num(x.num / y.num); }

  if ((strcmp(op, "%") == 0) || strcmp(op, "mod") == 0)  { return lval_num(x.num % y.num); }
  if ((strcmp(op, "^") == 0) || strcmp(op, "exp") == 0)  { return lval_num(pow(x.num, y.num)); }

  /* if no other matches, assume bad operator */
  return lval_err(LERR_BAD_OP);
}

lval eval(mpc_ast_t* t) {
  if (strstr(t->tag, "number")) {
    /* make sure we can convert */
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lval_num(x)
                           : lval_err(LERR_BAD_NUM);
  }

  char* op = t->children[1]->contents;
  lval x = eval(t->children[2]);

  /* iterate through the rest of children */
  int i = 3;
  while(strstr(t->children[i]->tag, "expr")) {
    x = eval_op(x, op, eval(t->children[i]));
    i++;
  }

  return x;
}

int main(int argc, char** argv) {

  /* create some parsers */
  mpc_parser_t* Number = mpc_new("number");
  mpc_parser_t* Operator = mpc_new("operator");
  mpc_parser_t* Expr = mpc_new("expr");
  mpc_parser_t* Lispy = mpc_new("lispy");

  /* define parsers with following language */
  mpca_lang(MPCA_LANG_DEFAULT,
            "                                                                            \
             number   : /-?([0-9]*[.])?[0-9]+/ ;                                         \
             operator : ('+'|\"add\") | ('-'|\"sub\") | ('*'|\"mult\") | ('/'|\"div\")   \
                        | ('%'| \"mod\") | ('^'|\"exp\") ;                               \
             expr     : <number> | '(' <operator> <expr>+ ')' ;                          \
             lispy    : /^/ <operator> <expr>+ /$/ ;                                     \
            ",
            Number, Operator, Expr, Lispy);

  /* print version and exit info   */
  puts("Lispy version 0.0.0.0.1");
  puts("Press Ctl+c to exit\n");

  while(1) {

    /* output prompt and get info */
    char* input = readline("lispy> ");

    /* add input to history */
    add_history(input);

    /* attempt to parse user input */
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      /* print the result */
      lval result = eval(r.output);
      lval_println(result);
      mpc_ast_delete(r.output);
    } else {
      /* print the error */
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    /* free input buffer */
    free(input);
  }

  /* cleanup our parsers */
  mpc_cleanup(4, Number, Operator, Expr, Lispy);

  return 0;
}

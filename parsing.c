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

int main(int argc, char** argv) {

  /* create some parsers */
  mpc_parser_t* Number = mpc_new("number");
  mpc_parser_t* Operator = mpc_new("operator");
  mpc_parser_t* Expr = mpc_new("expr");
  mpc_parser_t* Lispy = mpc_new("lispy");


  /* define parsers with following language */
  mpca_lang(MPCA_LANG_DEFAULT,
            "                                                    \
             number   : /-?([0-9]*[.])?[0-9]+/ ;                             \
             operator : '+' | '-' | '*' | '/' ;                  \
             expr     : <number> | '(' <operator> <expr>+ ')' ;  \
             lispy    : /^/ <operator> <expr>+ /$/ ;             \
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
      /* print the ast */
      mpc_ast_print(r.output);
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

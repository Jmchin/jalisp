#include <stdio.h>
#include <stdlib.h>

#include "mpc.h"

#define LASSERT(args, cond, err) if (!(cond)) { lval_del(args); return lval_err(err); }

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
typedef struct lval {
  int type;
  long num;
  char* err;
  char* sym;
  int count;
  struct lval** cell;
} lval;

/* forward declarations */
void lval_print(lval* v);
lval* lval_add(lval* u, lval* v);
lval* lval_eval(lval* v);
lval* lval_pop(lval* v, int i);
lval* lval_take(lval* v, int i);
lval* builtin_op(lval* a, char* op);
lval* builtin(lval* a, char* func);

/* possible lval types */
enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR };

/* possible lval errors */
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

/* create new number type lval */
lval* lval_num(long x) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;
  return v;
}

/* create new error type lval */
lval* lval_err(char* errmsg) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_ERR;
  v->err = malloc(strlen(errmsg) + 1);
  strcpy(v->err, errmsg);
  return v;
}

lval* lval_sym(char* s) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->sym = malloc(strlen(s) + 1);
  strcpy(v->sym, s);
  return v;
}

lval* lval_sexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

lval* lval_qexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_QEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

void lval_del(lval* v) {
  switch (v->type) {
    case LVAL_NUM:
      /* nothing special to do  */
      break;
    case LVAL_ERR:
      free(v->err);
      break;
    case LVAL_SYM:
      free(v->sym);
      break;
    case LVAL_QEXPR:
      /* fall through to SEXPR */
    case LVAL_SEXPR:
      /* delete all elements in the sexpr */
      for (int i = 0; i < v->count; i++) {
        lval_del(v->cell[i]);
      }
      /* free memory for allocating the pointers */
      free(v->cell);
      break;
  }
  /* free the lval itself */
  free(v);
}

lval* lval_read_num(mpc_ast_t* t) {
  errno = 0;
  long x = strtol(t->contents, NULL, 10);
  return errno != ERANGE
      ? lval_num(x)
      : lval_err("invalid number");
}

lval* lval_read(mpc_ast_t* t) {
  /* if number of symbol, create them directly */
  if (strstr(t->tag, "number")) { return lval_read_num(t); }
  if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

  /* if root (>) or sexpr then create empty list */
  lval* x = NULL;
  if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
  if (strstr(t->tag, "sexpr"))  { x = lval_sexpr(); }
  if (strstr(t->tag, "qexpr"))  { x = lval_qexpr(); }

  for (int i = 0; i < t->children_num; i++) {
    if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
    if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
    if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }
    x = lval_add(x, lval_read(t->children[i]));
  }

  return x;
}

lval* lval_add(lval* u, lval* v) {
  u->count++;
  u->cell = realloc(u->cell, sizeof(lval*) * u->count);
  u->cell[u->count-1] = v;
  return u;
}

void lval_expr_print(lval* v, char open, char close) {
  putchar(open);
  for (int i = 0; i < v->count; i++) {
    /* print the contained value */
    lval_print(v->cell[i]);

    /* no trailing space */
    if (i != (v->count-1)) {
      putchar(' ');
    }
  }
  putchar(close);
}

void lval_print(lval* v) {
  switch (v->type) {
    case LVAL_NUM: printf("%li", v->num); break;
    case LVAL_ERR: printf("Error: %s", v->err); break;
    case LVAL_SYM: printf("%s", v->sym); break;
    case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
    case LVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
  }
}

void lval_println(lval* v) { lval_print(v); putchar('\n'); }


lval* lval_eval_sexpr(lval* v) {
  /* eval children */
  for (int i = 0; i < v->count; i++) {
    v->cell[i] = lval_eval(v->cell[i]);
  }

  /* error checking */
  for (int i = 0; i < v->count; i++) {
    if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
  }

  /* empty expr */
  if (v->count == 0) { return v; }

  /* single expr  */
  if (v->count == 1) { return lval_take(v, 0); }

  /* first element must be symbol */
  lval* f = lval_pop(v, 0);
  if (f->type != LVAL_SYM) {
    lval_del(f); lval_del(v);
    return lval_err("S-expression does not start with symbol");
  }

  /* call builtin operator */
  lval* result = builtin(v, f->sym);
  lval_del(f);
  return result;
}

lval* lval_eval(lval* v) {
  /* evaluate sexpr */
  if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(v); }
  return v;
}

lval* lval_pop(lval* v, int i) {
  lval* x = v->cell[i];

  memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*)*(v->count-i-1));

  v->count--;

  v->cell = realloc(v->cell, sizeof(lval*)*(v->count));
  return x;
}

lval* lval_take(lval* v, int i) {
  lval* x = lval_pop(v, i);
  lval_del(v);
  return x;
}

lval* builtin_op(lval* a, char* op) {
  /* all args must be nums */
  for (int i = 0; i < a->count; i++) {
    if (a->cell[i]->type != LVAL_NUM) {
      lval_del(a);
      return lval_err("Cannot operate on non-number");
    }
  }

  /* pop first element */
  lval* x = lval_pop(a, 0);

  /* if no args, sub then perform unary negation */
  if ((strcmp(op, "-") == 0) && a->count == 0) {
    x->num = -x->num;
  }

  while (a->count > 0) {
    lval* y = lval_pop(a, 0);

    if (strcmp(op, "+") == 0) { x->num += y->num; }
    if (strcmp(op, "-") == 0) { x->num -= y->num; }
    if (strcmp(op, "*") == 0) { x->num *= y->num; }
    if (strcmp(op, "/") == 0) {
      if (y->num == 0) {
        lval_del(x); lval_del(y);
        x = lval_err("Division by zero"); break;
      }
      x->num /= y->num;
    }
    lval_del(y);
  }
  lval_del(a);
  return x;
}

lval* builtin_head(lval* a) {
  LASSERT(a, a->count == 1,
          "Function 'head' passed too many arguments");
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
          "Function 'head' passed incorrect type");
  LASSERT(a, a->cell[0]->count != 0,
          "Function 'head' passed empty qexpr");

  /* take first arg */
  lval* v = lval_take(a, 0);

  /* delete everything but the first element */
  while (v->count > 1) { lval_del(lval_pop(v, 1)); }
  return v;
}

lval* builtin_tail(lval* a) {
  LASSERT(a, a->count == 1,
          "Function 'tail' passed too many arguments");
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
          "Function 'tail' passed incorrect type");
  LASSERT(a, a->cell[0]->count != 0,
          "Function 'tail' passed empty qexpr");

  /* take the first arg */
  lval* v = lval_take(a, 0);

  /* delete first element and return */
  lval_del(lval_pop(v, 0));
  return v;
}

lval* builtin_list(lval* a) {
  a->type = LVAL_QEXPR;
  return a;
}

lval* builtin_eval(lval* a) {
  LASSERT(a, a->count == 1,
          "Function 'eval' passed too many arguments");
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
          "Function 'eval' passed incorrect type");
  lval* x = lval_take(a, 0);
  x->type = LVAL_SEXPR;
  return lval_eval(x);
}

lval* lval_join(lval* x, lval* y) {
  /* while there are elements in y, pop and add them to x */
  while(y->count) {
    x = lval_add(x, lval_pop(y, 0));
  }

  /* y is empty, so deallocate it */
  lval_del(y);
  return x;
}

lval* builtin_join(lval* a) {
  /* ensure every element in a is qexpr */
  for (int i = 0; i < a->count; i++) {
    LASSERT(a, a->cell[i]->type == LVAL_QEXPR,
            "Function 'join' passed incorrect type");
  }

  lval* x = lval_pop(a, 0);

  while (a->count) {
    x = lval_join(x, lval_pop(a, 0));
  }

  lval_del(a);
  return x;
}

lval* builtin(lval* a, char* func) {
  if (strcmp("list", func) == 0) { return builtin_list(a); }
  if (strcmp("head", func) == 0) { return builtin_head(a); }
  if (strcmp("tail", func) == 0) { return builtin_tail(a); }
  if (strcmp("join", func) == 0) { return builtin_join(a); }
  if (strcmp("eval", func) == 0) { return builtin_eval(a); }
  if (strstr("+-*/", func)) { return builtin_op(a, func);  }
  lval_del(a);
  return lval_err("Unknown function");
}

int main(int argc, char** argv) {

  /* create some parsers */
  mpc_parser_t* Number = mpc_new("number");
  mpc_parser_t* Symbol = mpc_new("symbol");
  mpc_parser_t* Sexpr = mpc_new("sexpr");
  mpc_parser_t* Qexpr = mpc_new("qexpr");
  mpc_parser_t* Expr = mpc_new("expr");
  mpc_parser_t* Lispy = mpc_new("lispy");

  /* define parsers with following language */
  mpca_lang(MPCA_LANG_DEFAULT,
            "                                                                            \
             number   : /-?([0-9]*[.])?[0-9]+/ ;                                         \
             symbol   : ('+'|\"add\") | ('-'|\"sub\") | ('*'|\"mult\") | ('/'|\"div\")   \
                        | ('%'| \"mod\") | ('^'|\"exp\") | \"list\" | \"head\"           \
                        | \"tail\" | \"join\" | \"eval\" ;                               \
             sexpr    : '(' <expr>* ')' ;                                                \
             qexpr    : '{' <expr>* '}' ;                                                \
             expr     : <number> | <symbol> | <sexpr> | <qexpr> ;                        \
             lispy    : /^/ <expr>* /$/ ;                                                \
            ",
            Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

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
      // lval result = eval(r.output);
      lval* x = lval_eval(lval_read(r.output));
      lval_println(x);
      lval_del(x);
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
  mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

  return 0;
}

#include <stdio.h>
#include <stdlib.h>

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

  /* print version and exit info   */
  puts("Lispy version 0.0.0.0.1");
  puts("Press Ctl+c to exit\n");


  while(1) {

    /* output prompt and get info */
    char* input = readline("lispy> ");

    /* add input to history */
    add_history(input);

    /* echo input back to user */
    printf("No you're a %s\n", input);

    /* free input buffer */
    free(input);
  }

  return 0;

}

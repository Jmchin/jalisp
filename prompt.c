#include <stdio.h>
#include <stdlib.h>

#include <readline/readline.h>
#include <readline/history.h>

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

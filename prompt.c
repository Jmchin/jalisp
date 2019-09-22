#include <stdio.h>

/* declare a buffer for user input */
static char input[2048];

int main(int argc, char** argv) {

  /* print version and exit info   */
  puts("Lispy version 0.0.0.0.1");
  puts("Press Ctl+c to exit\n");


  while(1) {

    fputs("lispy> ", stdout);
    fgets(input, 2048, stdin);
    printf("No you're a %s", input);

  }

  return 0;

}

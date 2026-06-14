#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define BUFFER_SIZE 256

void split_string(char *s, const char *d, char *arr[]) {
  int i = 0;
  char *tok = strtok(s, d);
  while (tok != NULL) {
    arr[i++] = tok;
    tok = strtok(NULL, d);
  }

  arr[i] = NULL;
}

int main(int argc, char *argv[]) {
  char buffer[BUFFER_SIZE];

  char *path = getenv("PATH");
  char pathenv[strlen(path) + sizeof("PATH=")];
  sprintf(pathenv, "PATH=%s", path);
  char *envp[] = {pathenv, NULL};

  while (1) {
    fprintf(stdout, "> ");
    fflush(stdout);
    ssize_t n = read(STDIN_FILENO, buffer, BUFFER_SIZE - 1);
    if (n < 0) {
      perror("read");
      exit(EXIT_FAILURE);
    }

    buffer[n] = '\0';

    char *args[32];
    split_string(buffer, " \n", args);

    if (strcmp(args[0], "exit") == 0) {
      exit(EXIT_SUCCESS);
    }

    int pid = fork();
    switch (pid) {
    case -1:
      perror("fork");
      exit(EXIT_FAILURE);
    case 0:
      execvp(args[0], args);
      perror(args[0]);
      exit(EXIT_FAILURE);
    default:
      waitpid(pid, NULL, 0);
      memset(buffer, 0, BUFFER_SIZE);
    }
  }

  return 0;
}

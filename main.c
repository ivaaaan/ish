#include <stddef.h>
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

typedef int (*cmd_handler)(char **args);

struct cmd {
  const char *name;
  cmd_handler handler;
};

static int cd_handler(char **args) {
  const char *dir = args[1] ? args[1] : getenv("HOME");
  if (chdir(dir) != 0) {
    perror("cd");
  }

  return 0;
}

static int exit_handler(char **args) { exit(EXIT_SUCCESS); }

static int export_handler(char **args) {
  if (args[1] == NULL) {
    return 0;
  }

  char *eq = strchr(args[1], '=');
  if (eq == NULL) {
    return 0;
  }

  *eq = '\0';
  setenv(args[1], eq + 1, 1);

  return 0;
}

static int unset_handler(char **args) {
  if (args[1] == NULL) {
    return 0;
  }

  unsetenv(args[1]);

  return 0;
}

static const struct cmd cmds[] = {
    {"cd", cd_handler},
    {"exit", exit_handler},
    {"export", export_handler},
    {"unset", unset_handler},
};

cmd_handler find_builtin_cmd(char *name) {
  for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
    if (strcmp(name, cmds[i].name) == 0) {
      return cmds[i].handler;
    }
  }

  return NULL;
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

    if (args[0] == NULL) {
      continue;
    }

    for (size_t i = 1; args[i] != NULL; i++) {

      char *d = strchr(args[i], '$');
      if (d == NULL) {
        continue;
      }

      *d = '\0';

      char *v = getenv(d + 1);
      if (v != NULL) {
        args[i] = v;
      }
    }

    cmd_handler c = find_builtin_cmd(args[0]);
    if (c != NULL) {
      c(args);
      continue;
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

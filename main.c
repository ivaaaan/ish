#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
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

  int save_in = -1, save_out = -1;
  while (1) {
    fprintf(stdout, "> ");
    fflush(stdout);
    ssize_t n = read(STDIN_FILENO, buffer, BUFFER_SIZE - 1);
    if (n < 0) {
      perror("read");
      exit(EXIT_FAILURE);
    }

    buffer[n] = '\0';

    FILE *stdout_r = NULL, *stdin_r = NULL;

    if (save_in != -1 || save_out != -1) {
      dup2(save_in, STDIN_FILENO);
      dup2(save_out, STDOUT_FILENO);
    }

    char *stdout_redirect = strchr(buffer, '>');
    if (stdout_redirect != NULL) {
      int append = stdout_redirect[1] == '>';

      *stdout_redirect = '\0';
      char *fname = strtok(stdout_redirect + 1, " \t\n>");
      if (fname == NULL) {
        perror("stdout fname is NULL");
        continue;
      }

      save_out = dup(STDOUT_FILENO);
      close(STDOUT_FILENO);

      stdout_r = fopen(fname, append ? "a+" : "w+");
      if (stdout_r == NULL) {
        perror("fopen");
        continue;
      }
    }

    char *stdin_redirect = strchr(buffer, '<');
    if (stdin_redirect != NULL) {
      *stdin_redirect = '\0';
      char *fname = strtok(stdin_redirect + 1, " \t\n");
      if (fname == NULL) {
        perror("stdin fname is NULL");
        continue;
      }

      save_in = dup(STDIN_FILENO);
      close(STDIN_FILENO);

      stdin_r = fopen(fname, "r");
      if (stdin_r == NULL) {
        perror("fopen");
        continue;
      }
    }

    int pipefd[2];
    char *pipes = strchr(buffer, '|');
    if (pipes != NULL) {

      char *commands[32];
      split_string(buffer, "|", commands);

      if (pipe(pipefd) == -1) {
        perror("pipe");
        continue;
      }

      int left = fork();
      if (left == 0) {
        char *args[32];
        split_string(commands[0], " \n", args);

        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);

        execvp(args[0], args);
        _exit(127);
      }

      int right = fork();
      if (right == 0) {
        char *args[32];
        split_string(commands[1], " \n", args);

        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);

        execvp(args[0], args);
        _exit(127);
      }

      close(pipefd[0]);
      close(pipefd[1]);

      waitpid(left, NULL, 0);
      waitpid(right, NULL, 0);

      continue;
    }

    char *args[32];
    split_string(buffer, " \n", args);

    if (args[0] == NULL) {
      continue;
    }

    uint status = 0;
    for (size_t i = 1; args[i] != NULL; i++) {
      char *d = strchr(args[i], '$');

      if (d != NULL) {
        *d = '\0';

        char *v = getenv(d + 1);
        if (v != NULL) {
          args[i] = v;
          continue;
        }
      }
    }

    if (status < 0) {
      continue;
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
      if (stdout_r != NULL) {
        fclose(stdout_r);
        dup2(save_out, STDOUT_FILENO);
        close(save_out);
      }

      if (stdin_r != NULL) {
        fclose(stdin_r);
        dup2(save_in, STDIN_FILENO);
        close(save_in);
      }

      memset(buffer, 0, BUFFER_SIZE);
    }
  }

  return 0;
}

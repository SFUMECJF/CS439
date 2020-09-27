#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util.h"

#define STR_EQ(x, y) !strcmp(x, y)

extern char shell_paths[MAX_DIRS_IN_PATH][MAX_CHARS_IN_CMDLINE];

/* Should the UTCSH internal functions dump verbose output? */
static int utcsh_internal_verbose = 0;

void maybe_print_error() {
  if (utcsh_internal_verbose) {
    char *err = strerror(errno);
    printf("[UTCSH INTERNAL ERROR]: %s\n", err);
  }
}

static void clear_shell_paths() {
  int i;
  for (i = 0; i < MAX_DIRS_IN_PATH; ++i) {
    memset(shell_paths[i], 0, MAX_CHARS_IN_CMDLINE * sizeof(char));
  }
}

int set_shell_path(char **newPaths) {
  if (!newPaths) {
    return 0; /* Assume this was an error. To clear the shell paths, pass in a
                 pointer-to-nullpointer, not a nullpointer directly. */
  }
  clear_shell_paths();
  int i;
  for (i = 0; i < MAX_DIRS_IN_PATH && newPaths[i]; ++i) {
    if (strlen(newPaths[i]) + 1 > MAX_CHARS_IN_CMDLINE) {
      return 0; /* This path is too long. */
    }
    strcpy(shell_paths[i], newPaths[i]);
  }
  return 1;
}

int is_absolute_path(char *path) {
  if (!path) {
    return 0;
  }
  return *path == '/';
}

/* Join dirname and basename with a '/' in the caller-provided buffer `buf`.
   We use a caller-provided buffer here so that callers can use either stack
   or heap allocated arrays to view the result (depending on the needs) */
static void joinpath(const char *dirname, const char *basename, char *buf) {
  assert(dirname && "Got NULL directory name in joinpath.");
  assert(basename && "Got NULL filename in joinpath.");
  assert(buf && "Got NULL output in joinpath");
  size_t dlen = strlen(dirname);

  strcpy(buf, dirname);
  strcpy(buf + dlen + 1, basename);
  buf[dlen] = '/';
}

void Closedir(DIR *dirp) {
  if (closedir(dirp) == -1 && utcsh_internal_verbose) {
    printf("[UTCSH_INTERNAL]: Error closing directory.\n");
  }
}

char *exe_exists_in_dir(const char *dirname, const char *filename) {
  if (!dirname || !filename) {
    return NULL;
  }
  DIR *dir;
  struct dirent *dent;
  dir = opendir(dirname);
  if (!dir) {
    maybe_print_error();
    return NULL;
  }

  errno = 0; /* To distinguish EOS from error, see man 3 readdir */
  while ((dent = readdir(dir))) {
    if (STR_EQ(dent->d_name, filename)) {
      size_t buflen = strlen(dirname) + strlen(filename) + 2;
      char *buf = malloc(buflen * sizeof(char));
      if (!buf) {
        maybe_print_error();
        return NULL;
      }
      joinpath(dirname, filename, buf);
      int exec_forbidden = access(buf, X_OK);
      if (!exec_forbidden) {
        Closedir(dir);
        return buf;
      } else {
        switch (errno) {
        case EACCES:
        case ENOENT:
        case ENOTDIR:
          errno = 0;
          break; /* These are benign faults */
        case EIO:
        case EINVAL:
        case EFAULT:
        case ENOMEM:
        case ETXTBSY:
        case EROFS:
        case ENAMETOOLONG:
        case ELOOP:
          maybe_print_error(); /* User might want to know about these */
          errno = 0;
        }
        free(buf);
      }
    }
  }
  /* We have exited the loop. Why? If errno is nonzero, it's an error. */
  if (errno == EBADF) {
    maybe_print_error();
  }
  Closedir(dir);
  return NULL;
}

/*
 * testall - aggregate runner for the myMalloc test suite.
 *
 * It scans the directory of compiled test executables, runs each one as a
 * child process (with output suppressed and a watchdog timeout so a buggy
 * allocator that hangs cannot wedge the whole suite), and reports PASS/FAIL
 * per test plus an overall summary.
 *
 * A test "passes" iff its executable exits 0. Run an individual test directly
 * (e.g. ./tests/tests_executables/test_coalesce_both) to see its full output
 * including the heap dumps.
 *
 * The directory of executables is baked in at compile time via
 * -DTESTS_EXEC_DIR=... but can be overridden with argv[1].
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#ifndef TESTS_EXEC_DIR
#define TESTS_EXEC_DIR "tests/tests_executables"
#endif

#define TIMEOUT_SECONDS 60

static volatile pid_t g_child = -1;

static void on_alarm(int sig) {
  (void)sig;
  if (g_child > 0) {
    kill(g_child, SIGKILL);
  }
}

/* Result codes for a single run. */
enum { R_PASS = 0, R_FAILEXIT, R_SIGNAL, R_TIMEOUT, R_NORUN };

static int run_one(const char *path, int *info) {
  g_child = fork();
  if (g_child < 0) {
    return R_NORUN;
  }
  if (g_child == 0) {
    /* child: silence output, then exec the test */
    int devnull = open("/dev/null", O_WRONLY);
    if (devnull >= 0) {
      dup2(devnull, STDOUT_FILENO);
      dup2(devnull, STDERR_FILENO);
      if (devnull > 2) close(devnull);
    }
    execl(path, path, (char *)NULL);
    _exit(127); /* exec failed */
  }

  /* parent: wait with a watchdog */
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = on_alarm;
  sigaction(SIGALRM, &sa, NULL);

  int killed = 0;
  alarm(TIMEOUT_SECONDS);
  int status = 0;
  if (waitpid(g_child, &status, 0) < 0) {
    /* interrupted by our own alarm handler killing the child; wait again */
    waitpid(g_child, &status, 0);
    killed = 1;
  }
  unsigned remaining = alarm(0);
  if (remaining == 0) {
    killed = 1;
  }
  g_child = -1;

  if (WIFEXITED(status)) {
    *info = WEXITSTATUS(status);
    return (*info == 0) ? R_PASS : R_FAILEXIT;
  }
  if (WIFSIGNALED(status)) {
    *info = WTERMSIG(status);
    if (killed && *info == SIGKILL) {
      return R_TIMEOUT;
    }
    return R_SIGNAL;
  }
  return R_NORUN;
}

static int is_runnable(const char *dir, const char *name) {
  if (name[0] == '.') return 0;
  char path[4096];
  snprintf(path, sizeof(path), "%s/%s", dir, name);
  struct stat st;
  if (stat(path, &st) != 0) return 0;
  if (!S_ISREG(st.st_mode)) return 0;
  if (!(st.st_mode & S_IXUSR)) return 0;
  return 1;
}

int main(int argc, char **argv) {
  const char *dir = (argc > 1) ? argv[1] : TESTS_EXEC_DIR;

  struct dirent **list = NULL;
  int n = scandir(dir, &list, NULL, alphasort);
  if (n < 0) {
    fprintf(stderr, "testall: cannot scan '%s' (did you run `make tests`?)\n", dir);
    return 2;
  }

  printf("========================================================\n");
  printf(" myMalloc test suite\n");
  printf(" directory: %s\n", dir);
  printf("========================================================\n\n");

  int total = 0, passed = 0;
  for (int i = 0; i < n; i++) {
    const char *name = list[i]->d_name;
    if (is_runnable(dir, name)) {
      char path[4096];
      snprintf(path, sizeof(path), "%s/%s", dir, name);

      int info = 0;
      int r = run_one(path, &info);
      total++;

      const char *tag = "FAIL";
      char detail[64] = "";
      switch (r) {
        case R_PASS:    tag = "PASS"; passed++; break;
        case R_FAILEXIT: snprintf(detail, sizeof(detail), " (exit %d)", info); break;
        case R_SIGNAL:   snprintf(detail, sizeof(detail), " (signal %d)", info); break;
        case R_TIMEOUT:  snprintf(detail, sizeof(detail), " (TIMEOUT)"); break;
        default:         snprintf(detail, sizeof(detail), " (could not run)"); break;
      }
      printf("  [%s] %-28s%s\n", tag, name, detail);
    }
    free(list[i]);
  }
  free(list);

  printf("\n========================================================\n");
  printf(" %d/%d tests passed\n", passed, total);
  printf("========================================================\n");

  return (passed == total && total > 0) ? 0 : 1;
}

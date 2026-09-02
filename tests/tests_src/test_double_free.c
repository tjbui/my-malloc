/* test_double_free: freeing the same pointer twice must be detected and abort
 * the program. Because that aborts, we run it in a child process and treat a
 * non-clean exit as success. An alarm guards against the error path hanging on
 * the allocator mutex. */
#include "test_utils.h"

int main(void) {
  heading("test_double_free");

  pid_t pid = fork();
  if (pid == 0) {
    /* child: expected to die on the second free */
    freopen("/dev/null", "w", stderr); /* silence the diagnostic */
    alarm(5);
    void *p = malloc(64);
    /* alias through volatile so the compiler cannot flag the intentional
     * use-after-free at compile time */
    void *volatile q = p;
    free(p);
    free(q);      /* should abort here */
    _exit(0);     /* only reached if the double free went undetected */
  } else if (pid > 0) {
    int status = 0;
    waitpid(pid, &status, 0);
    int clean_exit = WIFEXITED(status) && WEXITSTATUS(status) == 0;
    CHECK(!clean_exit, "double free was detected (child did not exit cleanly)");
    if (WIFSIGNALED(status)) {
      printf("    (child terminated by signal %d, as expected)\n",
             WTERMSIG(status));
    }
  } else {
    CHECK(0, "fork() failed");
  }

  /* Sanity: a normal free still works in this (parent) process. */
  void *ok = malloc(32);
  free(ok);
  CHECK(1, "a single free still works normally");

  return test_report("test_double_free");
}

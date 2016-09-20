/*
 * cuttle/daemon.c
 */

#include <cuttle/daemon.h>


/** become_daemon()
 *    fork() and become daemon
 */
pid_t cf_become_daemon(void)
{
  return fork();
}


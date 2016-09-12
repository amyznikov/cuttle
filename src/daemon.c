/*
 * cuttle/daemon.c
 */
#define _GNU_SOURCE

#include <cuttle/daemon.h>
#include <cuttle/debug.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <execinfo.h>
#include <ucontext.h>


static void my_signal_handler(int signum, siginfo_t *si, void * context)
{
  int ignore = 0;
  int status = 0;
  const ucontext_t * uc = (ucontext_t *) context;
  void * caller_address;

#if ( __aarch64__ )
  caller_address = (void *) uc->uc_mcontext.pc;
#elif ( __arm__)
  caller_address = (void *) uc->uc_mcontext.arm_pc;
#else
  caller_address = (void *) uc->uc_mcontext.gregs[16]; // REG_RIP
#endif

  if ( signum != SIGWINCH ) {
    CF_CRITICAL("SIGNAL %d (%s)", signum, strsignal(signum));
  }

  switch ( signum ) {
    case SIGINT :
    case SIGQUIT :
    case SIGTERM :
      status = 0;
    break;

    case SIGSEGV :
    case SIGSTKFLT :
    case SIGILL :
    case SIGBUS :
    case SIGSYS :
    case SIGFPE :
    case SIGABRT :
      status = EXIT_FAILURE;
      CF_FATAL("Fault address:%p from %p", si->si_addr, caller_address);
      CF_PBT();
    break;

    default :
      ignore = 1;
    break;
  }

  if ( !ignore ) {
    exit(status);
  }
}


/**
 * setup_signal_handler()
 *    see errno on failure
 */
bool cf_setup_signal_handler(void)
{
  struct sigaction sa;
  int sig;

  memset(&sa, 0, sizeof(sa));

  sa.sa_flags = SA_SIGINFO;
  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = my_signal_handler;

  for ( sig = 1; sig <= SIGUNUSED; ++sig ) {
    /* skip unblockable signals */
    if ( sig != SIGKILL && sig != SIGSTOP && sig != SIGCONT && sigaction(sig, &sa, NULL) != 0 ) {
      return false;
    }
  }

  return true;
}


/** become_daemon()
 *    fork() and become daemon
 */
pid_t cf_become_daemon(void)
{
  return fork();
}


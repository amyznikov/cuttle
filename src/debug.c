/*
 * cuttle/debug.c
 *
 *  Created on: Aug 27, 2016
 *      Author: amyznikov
 */

#define _GNU_SOURCE

#include <cuttle/debug.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/syscall.h>
#include <errno.h>
#include <signal.h>
#include <ucontext.h>
#include <openssl/err.h>

#if __ANDROID__
  # include <android/log.h>
  # ifndef EXIT_FAILURE
    # define EXIT_FAILURE 1
  # endif
#else
  #include <execinfo.h>
#endif


/* Standard  terminal colors */
#define TCFG_GRAY     "\033[30m" //  set foreground color to gray
#define TCFG_RED      "\033[31m" //  set foreground color to red
#define TCFG_GREEN    "\033[32m" //  set foreground color to green
#define TCFG_YELLOW   "\033[33m" //  set foreground color to yellow
#define TCFG_BLUE     "\033[34m" //  set foreground color to blue
#define TCFG_PURPLE   "\033[35m" //  set foreground color to magenta (purple)
#define TCFG_CYAN     "\033[36m" //  set foreground color to cyan
#define TCFG_WHITE    "\033[37m" //  set foreground color to white

#define TCBG_GRAY     "\033[40m" //  set background color to gray
#define TCBG_RED      "\033[41m" //  set background color to red
#define TCBG_GREEN    "\033[42m" //  set background color to green
#define TCBG_YELLOW   "\033[43m" //  set background color to yellow
#define TCBG_BLUE     "\033[44m" //  set background color to blue
#define TCBG_PURPLE   "\033[45m" //  set background color to magenta (purple)
#define TCBG_CYAN     "\033[46m" //  set background color to cyan
#define TCBG_WHITE    "\033[47m" //  set background color to white

#define TC_RESET      "\033[0m"  //  reset; clears all colors and styles (to white on black)
#define TC_BOLD       "\033[1m"  //  bold on
#define TC_ITALIC     "\033[3m"  //  italics on
#define TC_UNDERLINE  "\033[4m"  //  underline on
#define TC_BLINK      "\033[5m"  //  blink on
#define TC_REVERSE    "\033[7m"  //  reverse video on
#define TC_INVISIBLE  "\033[8m"  //  nondisplayed (invisible)

//\033[x;yH moves cursor to line x, column y
//\033[xA moves cursor up x lines
//\033[xB moves cursor down x lines
//\033[xC moves cursor right x spaces
//\033[xD moves cursor left x spaces
//\033[2J clear screen and home cursor

// current time
struct ctime {
  int year;
  int month;
  int day;
  int hour;
  int min;
  int sec;
  int msec;
};

#define ctime_string() \
    getctime_string((char[32]) {0})


static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
static FILE * fplog = NULL;
static char * logfilename = NULL;
static uint32_t logmask = CF_LOG_INFO;


#if !__ANDROID__
static inline pid_t gettid() {
  return (pid_t) syscall (SYS_gettid);
}
#endif

static void getctime(struct ctime * ct)
{
  struct timespec t;
  struct tm * tm;

  clock_gettime(CLOCK_REALTIME, &t);
  tm = gmtime(&t.tv_sec);

  ct->year = tm->tm_year + 1900;
  ct->month = tm->tm_mon + 1;
  ct->day = tm->tm_mday;
  ct->hour = tm->tm_hour;
  ct->min = tm->tm_min;
  ct->sec = tm->tm_sec;
  ct->msec = t.tv_nsec / 1000000;
}

static const char * getctime_string(char buf[32])
{
  struct ctime ct;
  getctime(&ct);
  snprintf(buf, 31, "%.4d-%.2d-%.2d-%.2d:%.2d:%.2d.%.3d",
      ct.year, ct.month, ct.day, ct.hour, ct.min, ct.sec, ct.msec);
  return buf;
}

static inline void plogbegin(int pri)
{
  const char * ctrl = NULL;
  switch ( pri ) {
    case CF_LOG_FATAL:
    case CF_LOG_CRITICAL:
      ctrl = TCFG_RED TC_BOLD;
    break;
  case CF_LOG_ERROR:
    ctrl = TCFG_RED;
    break;
  case CF_LOG_WARNING:
    ctrl = TCFG_YELLOW;
    break;
  case CF_LOG_NOTICE:
      ctrl = TC_BOLD;
    break;
  //case CF_LOG_INFO:
  case CF_LOG_DEBUG:
      ctrl = TCFG_GREEN;
    break;
  default:
    break;
  }

  if ( ctrl ) {
    fputs(ctrl, fplog);
  }
}

static inline void plogend(int pri)
{
  const char * ctrl = NULL;
  switch ( pri )
  {
  case CF_LOG_FATAL:
    case CF_LOG_CRITICAL:
    case CF_LOG_ERROR:
    case CF_LOG_WARNING:
    case CF_LOG_NOTICE:
    //case CF_LOG_INFO:
    case CF_LOG_DEBUG:
    ctrl = TC_RESET;
    break;
  default:
    break;
  }

  if ( ctrl ) {
    fputs(ctrl, fplog), fflush(fplog);
  }
}


static void dump_ssl_errors(void)
{
  if ( ERR_peek_error() ) {
    ERR_print_errors_fp(fplog);
    ERR_clear_error();
  }
}



bool cf_set_logfilename(const char * fname)
{
  bool fok = false;

  free(logfilename), logfilename = NULL;

  if ( fplog && fplog != stderr && fplog != stdout ) {
    fclose(fplog), fplog = NULL;
  }

  if ( !fname ) {
    fok = true;
  }
  else if ( !(logfilename = strdup(fname)) ) {
    fprintf(stderr, "fatal error: strdup(logfilename) fails: %s\n", strerror(errno));
  }
  else if ( strcasecmp(fname, "stderr") == 0 ) {
    fplog = stderr;
  }
  else if ( strcasecmp(fname, "stdout") == 0 ) {
    fplog = stdout;
  }
  else if ( !(fplog = fopen(logfilename, "a")) ) {
    fprintf(stderr, "fatal error: strdup(logfilename) fails: %s\n", strerror(errno));
  }
  else {
    fprintf(fplog, "\n\nNEW LOG STARTED AT %s\n", ctime_string());
    fok = true;
  }

  return fok;
}

const char * cf_get_logfilename(void)
{
  return logfilename;
}

void cf_set_loglevel(uint32_t mask)
{
  logmask = mask;
}

uint32_t cf_get_loglevel(void)
{
  return logmask;
}

static char pric(int pri)
{
  char ch;

  switch ( pri ) {
    case CF_LOG_FATAL :
      ch = 'F';
    break;
    case CF_LOG_CRITICAL :
      ch = 'C';
    break;
    case CF_LOG_ERROR :
      ch = 'E';
    break;
    case CF_LOG_WARNING :
      ch = 'W';
    break;
    case CF_LOG_NOTICE :
      ch = 'N';
    break;
    case CF_LOG_INFO :
      ch = 'I';
    break;
    case CF_LOG_DEBUG :
      ch = 'D';
    break;
    default :
      ch = 'U';
    break;
  }
  return ch;
}

static void do_plogv(int pri, const char * func, int line, const char * format, va_list arglist)
{
  pthread_mutex_lock(&mtx);

#if __ANDROID__
  if ( fplog == stderr || fplog == stdout ) {
    char buf[1024] = "";
    int n;
    n = snprintf(buf, sizeof(buf) - 1, "|%c|%6d|%s| %-28s(): %4d :", pric(pri), gettid(), ctime_string(), func, line);
    if ( n < (int) (sizeof(buf) - 1) ) {
      vsnprintf(buf + n, sizeof(buf) - 1 - n, format, arglist);
    }
    __android_log_write(ANDROID_LOG_DEBUG, "cuttle", buf);
  }
  else
#endif
  {
    plogbegin(pri & 0x07);
    fprintf(fplog, "|%c|%6d|%s| %-28s(): %4d :", pric(pri), gettid(), ctime_string(), func, line);
    vfprintf(fplog, format, arglist);
    fputc('\n',fplog);
    dump_ssl_errors();
    plogend(pri & 0x07);
  }

  pthread_mutex_unlock(&mtx);

}

void cf_plogv(int pri, const char * func, int line, const char * format, va_list arglist)
{
  if ( fplog && (pri & 0x07) <= (logmask & 0x07) ) {
    do_plogv(pri, func, line, format, arglist);
  }
}

void cf_plog(int pri, const char * func, int line, const char * format, ...)
{
  if ( fplog && (pri & 0x07) <= (logmask & 0x07) ) {
    va_list arglist;
    va_start(arglist, format);
    do_plogv(pri, func, line, format, arglist);
    va_end(arglist);
  }
}


void cf_pbt(void)
{
#if !__ANDROID__

  int size;
  void * array[256];
  char ** messages = NULL;

  pthread_mutex_lock(&mtx);

  if ( fplog ) {

    size = backtrace(array, sizeof(array) / sizeof(array[0]));
    messages = backtrace_symbols(array, size);

    for ( int i = 0; i < size; ++i ) {
#if __ANDROID__
#else
      fprintf(fplog, "[bt]: (%d) %p %s\n", i, array[i], messages[i]);
#endif
    }
  }

  pthread_mutex_unlock(&mtx);

  if ( messages ) {
    free(messages);
  }
#endif
}





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

#ifndef  SIGUNUSED
#define SIGUNUSED 31
#endif

  for ( sig = 1; sig <= SIGUNUSED; ++sig ) {
    /* skip unblockable signals */
    if ( sig != SIGKILL && sig != SIGSTOP && sig != SIGCONT && sigaction(sig, &sa, NULL) != 0 ) {
      return false;
    }
  }

  return true;
}


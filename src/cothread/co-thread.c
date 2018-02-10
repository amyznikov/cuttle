/*
 * co-thread.c
 *
 *  Created on: Sep 24, 2016
 *      Author: amyznikov
 */

#include <cuttle/debug.h>
#include <cuttle/cothread/cothread.h>
#include <pthread.h>
#include <ucontext.h>
#include <stdlib.h>
#include <signal.h>

/*
 * The following value must be power of two (N^2).
 */
#define CO_STK_ALIGN      256
#define CO_STK_COROSIZE   ((sizeof(coroutine) + CO_STK_ALIGN - 1) & ~(CO_STK_ALIGN - 1))
#define CO_MIN_SIZE       SIGSTKSZ


#if defined(__ANDROID__)
extern int getcontext(ucontext_t * uctx);
extern int swapcontext(ucontext_t * ouctx, const ucontext_t * uctx);
extern void makecontext(ucontext_t * uctx, void (*func)(), int argc, ...);
#endif


typedef
struct s_co_ctx {
  ucontext_t cc;
} co_ctx_t;

typedef
struct s_coroutine {
  co_ctx_t ctx;
  size_t alloc_size;
  struct s_coroutine *caller;
  struct s_coroutine *restarget;
  void (*func)(void *);
  void *data;
  void *sheduler_data;
} coroutine;

typedef
struct s_cothread_ctx {
  coroutine co_main;
  coroutine *co_curr;
  coroutine *co_dhelper;
  coroutine *dchelper;
  char stk[CO_MIN_SIZE + CO_STK_COROSIZE];
} cothread_ctx;




static int valid_key;
static pthread_key_t key;
static pthread_once_t once_control = PTHREAD_ONCE_INIT;


static void * (*cothread_alloc_mem_proc)(size_t size) = NULL;
static void (*cothread_free_mem_proc)(void * address, size_t size) = NULL;



static void co_once_init(void)
{
  int status;
  if ( (status = pthread_key_create(&key, free)) == 0 ) {
    ++valid_key;
  }
  else {
    errno = status;
    CF_FATAL("pthread_key_create() fails: %s", strerror(status));
  }
}

static cothread_ctx * co_get_global_ctx(void)
{
  static cothread_ctx ctx;
  if ( ctx.co_curr == NULL )
    ctx.co_curr = &ctx.co_main;
  return &ctx;
}

static cothread_ctx * co_get_thread_ctx(void)
{
  cothread_ctx * ctx = (cothread_ctx *) (valid_key ? pthread_getspecific(key) : NULL);
  return ctx ? ctx : co_get_global_ctx();
}


static void * cothread_alloc_mem(size_t size)
{
  return cothread_alloc_mem_proc ? cothread_alloc_mem_proc(size): malloc(size);
}

static void cothread_free_mem(void * address, size_t size)
{
  return cothread_free_mem_proc ? cothread_free_mem_proc(address, size) : free(address);
}

static void co_runner(void)
{
  cothread_ctx * ctx = co_get_thread_ctx();
  coroutine * co = ctx->co_curr;

  co->restarget = co->caller;
  co->func(co->data);
  co_exit();
}

static void co_del_helper(void * data)
{
  (void)(data);

  cothread_ctx * ctx;
  coroutine * cdh;

  for ( ;; ) {
    ctx = co_get_thread_ctx();
    cdh = ctx->co_dhelper;
    ctx->co_dhelper = NULL;
    co_delete(ctx->co_curr->caller);
    co_call((coroutine_t) cdh);
    if ( !ctx->co_dhelper ) {
      CF_FATAL("Resume to delete helper coroutine: curr=%p caller=%p", ctx->co_curr, ctx->co_curr->caller);
      // exit(1);
    }
  }
}

static bool co_set_context(co_ctx_t * ctx, void (*func)(void), char * stkbase, size_t stksiz)
{
  if ( getcontext(&ctx->cc) ) {
    CF_FATAL("getcontext() fails: %s", strerror(errno));
    return false;
  }

  ctx->cc.uc_link = NULL;
  ctx->cc.uc_stack.ss_sp = stkbase;
  ctx->cc.uc_stack.ss_size = stksiz - sizeof(size_t);
  ctx->cc.uc_stack.ss_flags = 0;
  makecontext(&ctx->cc, func, 0);

  return true;
}

static void co_switch_context(co_ctx_t * octx, co_ctx_t * nctx)
{
  cothread_ctx * ctx = co_get_thread_ctx();
  if ( swapcontext(&octx->cc, &nctx->cc) < 0 ) {
    CF_FATAL("swapcontext() fails: curr=%p %s", ctx->co_curr, strerror(errno));
    exit(1);
  }
}



bool co_thread_init(void)
{
  cothread_ctx * ctx = NULL;
  int status;

  if ( (status = pthread_once(&once_control, co_once_init)) ) {
    errno = status;
    CF_FATAL("pthread_once() fails: %s", strerror(status));
  }
  else if ( !valid_key ) {
    status = -1;
    CF_FATAL("co_once_init() fails: %s", strerror(errno));
  }
  else if ( !(ctx = (cothread_ctx *) calloc(1, sizeof(cothread_ctx))) ) {
    CF_FATAL("calloc(cothread_ctx) fails: %s", strerror(errno));
    status = -1;
  }
  else {
    ctx->co_curr = &ctx->co_main;
    if ( (status = pthread_setspecific(key, ctx)) ) {
      CF_FATAL("pthread_setspecific() fails: %s", strerror(status));
      errno = status;
    }
  }

  if ( status ) {
    free(ctx);
  }

  return status == 0;
}


void co_thread_cleanup(void)
{
}


coroutine_t co_create(void (*func)(void *), void * data, void * stack, size_t size)
{
  size_t alloc_size = 0;
  coroutine * co = NULL;

  if ( stack != NULL ) {
    if ( (size &= ~(sizeof(size_t) - 1)) < CO_MIN_SIZE + CO_STK_COROSIZE ) {
      errno = EINVAL;
      goto end;
    }
  }
  else if ( (size &= ~(sizeof(size_t) - 1)) < CO_MIN_SIZE ) {
    errno = EINVAL;
    goto end;
  }
  else if ( !(stack = cothread_alloc_mem(size = size + CO_STK_COROSIZE)) ) {
    goto end;
  }
  else {
    alloc_size = size;
  }

  co = stack;
  stack = (uint8_t *) stack + CO_STK_COROSIZE;
  co->alloc_size = alloc_size;
  co->func = func;
  co->data = data;
  co->sheduler_data = NULL;

  if ( !co_set_context(&co->ctx, co_runner, stack, size - CO_STK_COROSIZE) ) {
    CF_FATAL("co_set_context() fails: %s", strerror(errno));
    if ( alloc_size ) {
      cothread_free_mem(co, alloc_size);
    }
    co = NULL;
  }

end:

  // CF_DEBUG("cothread created: co=%p", co);
  return (coroutine_t) co;
}

void co_delete(coroutine_t coro)
{
  cothread_ctx * ctx = co_get_thread_ctx();
  coroutine * co = (coroutine *) coro;

  if ( co == ctx->co_curr ) {
    CF_FATAL("FATAL BUG: Cannot delete itself: curr=%p", ctx->co_curr);
    exit(1);
  }
  else if ( co->alloc_size ) {
    cothread_free_mem(co, co->alloc_size);
  }
}

void co_call(coroutine_t coro)
{
  cothread_ctx * ctx = co_get_thread_ctx();
  coroutine * co = (coroutine *) coro, * oldco = ctx->co_curr;

  co->caller = ctx->co_curr;
  ctx->co_curr = co;

  co_switch_context(&oldco->ctx, &co->ctx);
}

void co_resume(void)
{
  cothread_ctx * ctx = co_get_thread_ctx();
  co_call(ctx->co_curr->restarget);
  ctx->co_curr->restarget = ctx->co_curr->caller;
}

void co_exit_to(coroutine_t coro)
{
  cothread_ctx * ctx = co_get_thread_ctx();
  coroutine * co = (coroutine *) coro;

  if ( !ctx->dchelper && !(ctx->dchelper = co_create(co_del_helper, NULL, ctx->stk, sizeof(ctx->stk))) ) {
    CF_FATAL("FATAL ERROR: co_create(co_del_helper) fails: curr=%p %s",ctx->co_curr, strerror(errno));
    exit(1);
  }

  ctx->co_dhelper = co;
  co_call((coroutine_t) ctx->dchelper);

  CF_FATAL("FATAL BUG: Stale coroutine called: curr=%p  exitto=%p  caller=%p",ctx->co_curr, co, ctx->co_curr->caller);
  exit(1);
}

void co_exit(void)
{
  co_exit_to((coroutine_t) (co_get_thread_ctx())->co_curr->restarget);
}

coroutine_t co_current(void)
{
  return (coroutine_t) (co_get_thread_ctx())->co_curr;
}

void * co_get_data(coroutine_t coro)
{
  return ((coroutine *) coro)->data;
}

void * co_set_data(coroutine_t coro, void *data)
{
  coroutine * co = (coroutine *) coro;
  void * olddata = co->data;
  co->data = data;
  return olddata;
}

void * co_get_scheduler_data(coroutine_t coro)
{
  return ((coroutine *) coro)->sheduler_data;
}

void *co_set_scheduler_data(coroutine_t coro, void *data)
{
  coroutine * co = (coroutine *) coro;
  void * olddata = co->sheduler_data;
  co->sheduler_data = data;
  return olddata;
}

int co_get_min_stack_size(void)
{
  return CO_MIN_SIZE + CO_STK_COROSIZE;
}

void co_set_mem_allocator(void * (*alloc)(size_t), void (*free)(void *, size_t) )
{
  cothread_alloc_mem_proc = alloc;
  cothread_free_mem_proc = free;
}


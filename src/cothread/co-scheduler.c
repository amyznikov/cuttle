/*
 * co-scheduler.c
 *
 *  Created on: Sep 8, 2016
 *      Author: amyznikov
 */

#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <sched.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/eventfd.h>
#include <sys/syscall.h>
#include <pcl.h>
#include <stdio.h>

#include <cuttle/debug.h>
#include <cuttle/sockopt.h>
#include <cuttle/time.h>
#include <cuttle/cclist.h>
#include <cuttle/cothread/scheduler.h>

#include "pthread_wait.h"



#define UNMASKED_EVENTS (EPOLLERR|EPOLLHUP|EPOLLRDHUP)
#define CREQ_LISTENER_STACK_SIZE   (128*1024)

#define MTX_WAKEUP_WAITING  0x01
#define MTX_WAKEUP_EVENT    0x02
#define CO_YIELD            0x04



#define co_current_time_ms()  cf_get_monotic_ms()

//////////////////////////////////////////////////////////////////////////////////
// epoll listener

static struct {
  pthread_wait_t lock;
  int eso;
} epoll_listener = {
  .lock = PTHREAD_WAIT_INITIALIZER,
  .eso = -1,
};

struct io_waiter {
  int64_t tmo;
  struct io_waiter *prev, *next;
  coroutine_t co;
  uint32_t mask;
  uint32_t events;
  uint32_t revents;
  uint32_t flags;
};

enum {
  iowait_io,
  iowait_eventfd
};

struct iorq {
  struct io_waiter *head, *tail;
  int so;
  int type;
};



static bool set_non_blocking(int so, bool optval)
{
  int flags;
  int status;

  if ( (flags = fcntl(so, F_GETFL, 0)) < 0 ) {
    status = -1;
  }
  else if ( optval ) {
    status = fcntl(so, F_SETFL, flags | O_NONBLOCK);
  }
  else {
    status = fcntl(so, F_SETFL, flags & ~O_NONBLOCK);
  }

  return status == 0;
}

static inline void epoll_listener_lock(void)
{
  pthread_wait_lock(&epoll_listener.lock);
}

static inline void epoll_listener_unlock(void)
{
  pthread_wait_unlock(&epoll_listener.lock);
}

static inline int epoll_listener_wait(int tmo)
{
  return pthread_wait(&epoll_listener.lock, tmo);
}

static inline void epoll_listener_signal(void)
{
  pthread_wait_broadcast(&epoll_listener.lock);
}

static inline bool epoll_add(struct iorq * e, uint32_t events)
{
  int status;

  status = epoll_ctl(epoll_listener.eso, EPOLL_CTL_ADD, e->so,
      &(struct epoll_event ) {
            .data.ptr = e,
            .events = (events | ((events & EPOLLONESHOT) ? 0 : EPOLLET))
            });

  return status == 0;
}

static inline bool epoll_remove(int so)
{
  int status;
  epoll_listener_lock();
  status = epoll_ctl(epoll_listener.eso, EPOLL_CTL_DEL, so, NULL);
  epoll_listener_unlock();
  return status == 0;
}

static inline int epoll_wait_events(struct epoll_event events[], int nmax)
{
  int n;
  while ( (n = epoll_wait(epoll_listener.eso, events, nmax, -1)) < 0 && errno == EINTR )
    {}
  return n;
}

static inline void epoll_queue(struct iorq * e, struct io_waiter * w)
{
  if ( w ) {

    epoll_listener_lock();

    w->next = NULL;

    if ( !e->tail ) {
      w->prev = NULL;
    }
    else {
      w->prev = e->tail;
      e->tail->next = w;
    }

    e->tail = w;

    if ( !e->head ) {
      e->head = w;
    }

    epoll_listener_unlock();
  }
}

static inline void epoll_dequeue(struct iorq * e, struct io_waiter * w)
{
  if ( w ) {

    epoll_listener_lock();

    if ( e->tail == w ) {
      e->tail = w->prev;
    }

    if ( w->prev ) {
      w->prev->next = w->next;
    }

    if ( w->next ) {
      w->next->prev = w->prev;
    }

    if ( e->head == w ) {
      e->head = w->next;
    }

    epoll_listener_unlock();
  }
}


static void * epoll_listener_thread(void * arg)
{
  (void) (arg);

  const int MAX_EPOLL_EVENTS = 5000;
  struct epoll_event events[MAX_EPOLL_EVENTS];

  struct iorq * e;
  struct io_waiter * w;

  int i, n, c;

  pthread_detach(pthread_self());


  while ( (n = epoll_wait_events(events, MAX_EPOLL_EVENTS)) >= 0 ) {

    epoll_listener_lock();

    for ( i = 0, c = 0; i < n; ++i ) {

      e = events[i].data.ptr;

      if ( e->type == iowait_eventfd ) {
        eventfd_t x;
        while ( eventfd_read(e->so, &x) == 0 ) {}
      }

      for ( w = e->head; w; w = w->next ) {
        if ( ((w->events |= events[i].events) & w->mask) && w->co ) {
          ++c;
        }
      }
    }

    if ( c ) {
      epoll_listener_signal();
    }

    epoll_listener_unlock();
  }

  return NULL;
}

static bool epoll_listener_init(void)
{
  static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

  pthread_t pid;
  int status = 0;

  pthread_mutex_lock(&mtx);

  if ( epoll_listener.eso == -1 ) {

    if ( (epoll_listener.eso = epoll_create1(0)) == -1 ) {
      CF_CRITICAL("epoll_create1() fails: %s", strerror(errno));
      status = -1;
    }
    else if ( (status = pthread_create(&pid, NULL, epoll_listener_thread, NULL)) ) {
      CF_CRITICAL("pthread_create() fails: %s", strerror(errno = status));
      close(epoll_listener.eso), epoll_listener.eso = -1;
    }
  }

  pthread_mutex_unlock(&mtx);

  return status == 0;
}



//////////////////////////////////////////////////////////////////////////////////
// cothread scheduler

struct co_scheduler_context {
  coroutine_t main;
  ccfifo queue;
  cclist waiters;
  int cs[2];
  pthread_spinlock_t lock;
  bool started :1, csbusy : 1;
};


struct schedule_request {
  union {
    struct {
      int (*callback)(void *, uint32_t);
      void * callback_arg;
      size_t stack_size;
      uint32_t flags;
      int so;
    } io;

    struct {
      void (*func)(void*);
      void * arg;
      size_t stack_size;
    } thread;
  };

  enum {
    creq_schedule_io = 1,
    creq_start_cothread = 2,
  } req;
};


struct iocb {
  struct iorq e;
  struct cclist_node * node;
  int (*fn)(void * cookie, uint32_t events);
  void * cookie;
};



static struct co_scheduler_context
  ** g_sched_array = NULL;

static int g_ncpu = 0;

static __thread struct co_scheduler_context
  * current_core = NULL;


bool cf_in_co_thread(void)
{
  return current_core != NULL;
}

static inline struct cclist_node * add_waiter(struct co_scheduler_context * core, struct io_waiter * w)
{
  w->mask |= UNMASKED_EVENTS;
  return cclist_push_back(&core->waiters, w);
}

static inline void remove_waiter(struct co_scheduler_context * core, struct cclist_node * node)
{
  cclist_erase(&core->waiters, node);
}


static void iocb_handler(void * arg)
{
  struct iocb * cb = arg;

  while ( cb->fn(cb->cookie, cb->e.head->revents) == 0 ) {
    co_call(current_core->main);
  }

  epoll_remove(cb->e.so);
  remove_waiter(current_core, cb->node);

  free(cb);
}


static inline void co_scheduler_lock(struct co_scheduler_context * core)
{
  pthread_spin_lock(&core->lock);
  while ( core->csbusy ) {
    pthread_spin_unlock(&core->lock);
    co_yield();
    pthread_spin_lock(&core->lock);
  }

  core->csbusy = true;
  pthread_spin_unlock(&core->lock);
}

static inline void co_scheduler_unlock(struct co_scheduler_context * core)
{
  pthread_spin_lock(&core->lock);
  core->csbusy = false;
  pthread_spin_unlock(&core->lock);
}

static inline int send_schedule_request(struct co_scheduler_context * core, const struct schedule_request * rq)
{
  int32_t status = 0;

  if ( !cf_in_co_thread() ) {
    if ( send(core->cs[0], rq, sizeof(*rq), MSG_NOSIGNAL) != (ssize_t) sizeof(*rq) ) {
      status = errno;
    }
    else if ( recv(core->cs[0], &status, sizeof(status), MSG_NOSIGNAL) != (ssize_t) sizeof(status) ) {
      status = errno;
    }
  }
  else {

    co_scheduler_lock(core);

    if ( co_send(core->cs[0], rq, sizeof(*rq), 0) != (ssize_t) sizeof(*rq) ) {
      status = errno;
    }
    else if ( co_recv(core->cs[0], &status, sizeof(status), 0) != (ssize_t) sizeof(status) ) {
      status = errno;
    }

    co_scheduler_unlock(core);
  }

  return status;
}


static void schedule_request_handler(void * arg)
{
  (void)(arg);
  coroutine_t co;
  struct iocb * cb;
  struct schedule_request creq;
  ssize_t size;
  int32_t status;

  while ( (size = co_recv(current_core->cs[1], &creq, sizeof(creq), 0)) == (ssize_t) sizeof(creq) ) {

    errno = 0;

    if ( creq.req == creq_start_cothread ) {


      if ( ccfifo_is_full(&current_core->queue) ) {
        status = EBUSY;
      }
      else if ( !(co = co_create(creq.thread.func, creq.thread.arg, NULL, creq.thread.stack_size)) ) {
        status = errno ? errno : ENOMEM;
      }
      else {
        epoll_listener_lock();
        ccfifo_ppush(&current_core->queue, co);
        epoll_listener_signal();
        epoll_listener_unlock();

        status  = 0;
      }

    }
    else if ( creq.req == creq_schedule_io ) {

      if ( ccfifo_is_full(&current_core->queue) ) {
        status = EBUSY;
      }
      else if ( !(cb = calloc(1, sizeof(*cb))) ) {
        status = ENOMEM;
      }
      else if ( !(co = co_create(iocb_handler, cb, NULL, creq.io.stack_size)) ) {
        status = errno ? errno : ENOMEM;
      }
      else {
        cb->fn = creq.io.callback;
        cb->cookie = creq.io.callback_arg;
        cb->e.so = creq.io.so;
        cb->e.type = iowait_io;
        cb->e.head = cclist_peek(cb->node =
            add_waiter(current_core, &(struct io_waiter ) {
                  .co = co,
                  .mask = creq.io.flags,
                  .tmo = -1,
                }));

        if ( !epoll_add(&cb->e, creq.io.flags) ) {
          CF_FATAL("BUG: epoll_add(so=%d) fails: %s", creq.io.so, strerror(errno));
          exit(1);
        }
        status = 0;
      }
    }


    if ( co_send(current_core->cs[1], &status, sizeof(status), 0) != sizeof(status) ) {
      CF_FATAL("FATAL: co_send(status) fails: %s", strerror(errno));
      exit(1);
    }

  }

  CF_FATAL("LEAVE: co_recv(schedule_request) fails: %s\n", strerror(errno));
  exit(1);
}


static inline int add_signaled(struct io_waiter * w, coroutine_t cc[], int n)
{
  w->revents = w->events;
  w->events &= ~w->mask;

  // check if already added
  if ( !co_get_scheduler_data(w->co)  ) {
    co_set_scheduler_data(w->co, (void*)(1));
    cc[n++] = w->co;
  }
  return n;

//  for ( int i = 0; i < n; ++i ) {
//    if ( w->co == cc[i] ) {
//      return n;
//    }
//  }
//  cc[n] = w->co;
//  return n + 1;
}

static int walk_waiters_list(int64_t ct, coroutine_t cc[], int ccmax, int64_t * wtmo)
{
  struct cclist_node * node;
  struct io_waiter * w;
  int64_t dt, tmo;
  int n;

  for ( n = 0, tmo = INT_MAX, node = cclist_head(&current_core->waiters); node; node = node->next ) {

    if ( !(w = cclist_peek(node))->co ) {
      continue;
    }

    if ( w->flags & CO_YIELD ) {
      w->flags &= ~CO_YIELD;
      tmo = 0;
      continue;
    }


    if ( (w->events & w->mask) || (w->flags & MTX_WAKEUP_EVENT) ) {
      if ( (n = add_signaled(w, cc, n)) == ccmax ) {
        break;
      }
    }
    else if ( w->tmo > 0 ) {
      if ( ct >= w->tmo ) {
        if ( (n = add_signaled(w, cc, n)) == ccmax ) {
          break;
        }
      }
      else if ( (dt = (w->tmo - ct)) < tmo ) {
        tmo = dt;
      }
    }
    else if ( w->tmo == 0 ) {
      CF_FATAL("APP BUG: w->tmo==0");
      exit(1);
    }
  }

  for ( int i = 0; i < n; ++i ) {
    co_set_scheduler_data(cc[i], NULL);
  }

  *wtmo = tmo;

  return n;
}



static void * pclthread(void * arg)
{
  const int ccmax = 1000;
  coroutine_t cc[ccmax];
  coroutine_t co;

  int64_t t0, tmo;
  int n;

  pthread_detach(pthread_self());

  current_core = arg;

  pthread_spin_init(&current_core->lock, 0);

  if ( co_thread_init() != 0 ) {
    CF_FATAL("FATAL: co_thread_init() fails");
    exit(1);
  }

  if ( !(current_core->main = co_current()) ) {
    CF_FATAL("FATAL: co_current() fails");
    exit(1);
  }

  if ( !(co = co_create(schedule_request_handler, NULL, NULL, CREQ_LISTENER_STACK_SIZE)) ) {
    CF_FATAL("FATAL: co_create(schedule_request_handler) fails");
    exit(1);
  }

  epoll_listener_lock();
  ccfifo_ppush(&current_core->queue, co);

  current_core->started = true;

  while ( 42 ) {

    while ( ccfifo_pop(&current_core->queue, &co) ) {
      epoll_listener_unlock();
      co_call(co);
      epoll_listener_lock();
    }

    t0 = co_current_time_ms();

    if ( !(n = walk_waiters_list(t0, cc, ccmax, &tmo)) ) {
      epoll_listener_wait(tmo);
    }
    else {
      epoll_listener_unlock();
      for ( int i = 0; i < n; ++i ) {
        co_call(cc[i]);
      }
      epoll_listener_lock();
    }
  }

  epoll_listener_unlock();


  co_thread_cleanup();
  pthread_spin_destroy(&current_core->lock);

  return NULL;
}


static pthread_t new_pcl_thread(void)
{
  struct co_scheduler_context * ctx = NULL;
  pthread_t pid = 0;

  int status;

  if ( !(ctx = calloc(1, sizeof(*ctx))) ) {
    goto end;
  }

  ctx->cs[0] = ctx->cs[1] = -1;

  if ( socketpair(AF_LOCAL, SOCK_STREAM, 0, ctx->cs) != 0 ) {
    goto end;
  }

  if ( !set_non_blocking(ctx->cs[1], true) ) {
    goto end;
  }

  if ( !ccfifo_init(&ctx->queue, 1000, sizeof(coroutine_t)) ) {
    goto end;
  }

  if ( !cclist_init(&ctx->waiters, 10000, sizeof(struct io_waiter)) ) {
    goto end;
  }

  g_sched_array[g_ncpu++] = ctx;

  if ( (status = pthread_create(&pid, NULL, pclthread, ctx)) ) {
    g_sched_array[--g_ncpu] = NULL;
    errno = status;
    goto end;
  }

  while ( !ctx->started ) {
    usleep(20 * 1000);
  }

end:

  if ( !pid && ctx ) {
    cclist_cleanup(&ctx->waiters);
    ccfifo_cleanup(&ctx->queue);

    for ( int i = 0; i < 2; ++i ) {
      if ( ctx->cs[i] != -1 ) {
        close(ctx->cs[i]);
      }
    }

    free(ctx);
  }

  return pid;
}

bool co_scheduler_init(int ncpu)
{
  bool fok = false;

  if ( ncpu < 1 ) {
    ncpu = 1;
  }

  if ( !epoll_listener_init() ) {
    goto end;
  }

  if ( !(g_sched_array = calloc(ncpu, sizeof(struct co_scheduler_context*))) ) {
    goto end;
  }


  while ( ncpu > 0 && new_pcl_thread() ) {
    --ncpu;
  }

  fok = (ncpu == 0);

end:
  return fok;
}


bool co_schedule(void (*func)(void*), void * arg, size_t stack_size)
{
  struct co_scheduler_context * core;
  int status;

  core = g_sched_array[rand() % g_ncpu];

  status = send_schedule_request(core,
      &(struct schedule_request) {
        .req = creq_start_cothread,
        .thread.func = func,
        .thread.arg = arg,
        .thread.stack_size = stack_size
      });

  if ( status ) {
    errno = status;
  }

  return status == 0;
}

bool co_schedule_io(int so, uint32_t events, int (*callback)(void * arg, uint32_t events), void * arg,
    size_t stack_size)
{
  struct co_scheduler_context * core;
  int status;

  core = g_sched_array[rand() % g_ncpu];

  status = send_schedule_request(core,
      &(struct schedule_request) {
        .req = creq_schedule_io,
        .io.so = so,
        .io.flags = events,
        .io.callback = callback,
        .io.callback_arg = arg,
        .io.stack_size = stack_size
      });


  if ( status ) {
    errno = status;
  }

  return status == 0;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// co thread management

void co_sleep(uint32_t msec)
{
  if ( cf_in_co_thread() ) {

    struct cclist_node * node =
        add_waiter(current_core,
            &(struct io_waiter ) {
                  .co = co_current(),
                  .tmo = co_current_time_ms() + msec,
                  .flags = CO_YIELD
                });

    co_call(current_core->main);

    remove_waiter(current_core, node);
  }
  else if ( msec == 0 ) {
    sched_yield();
  }
  else {
    nanosleep(&(struct timespec ) {
          .tv_sec = msec / 1000,
          .tv_nsec = (msec - msec % 1000) * 1000000,
        }, NULL);
  }
}

void co_yield(void)
{
  co_sleep(0);
}




//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


struct co_thread_lock_t {
  struct iorq e;
  coroutine_t co;
};

static pthread_mutex_t co_thread_lock_global_mtx
  = PTHREAD_MUTEX_INITIALIZER;


static inline void co_thread_global_lock(void)
{
  pthread_mutex_lock(&co_thread_lock_global_mtx);
}

static inline void co_thread_global_unlock(void)
{
  pthread_mutex_unlock(&co_thread_lock_global_mtx);
}

static co_thread_lock_t * co_thread_check(co_thread_lock_t ** objp)
{
  co_thread_lock_t * obj;

  if ( !(obj = *objp) ) {
    CF_FATAL("BUG: obj = NULL");
    errno = EINVAL;
    raise(SIGINT);
  }
  else if ( obj->co != co_current() ) {
    CF_FATAL("BUG: obj->co=%p != co_current()=%p."
        " Forgot to call co_thread_lock()?",
        obj->co, co_current());
    errno = EBUSY;
    //raise(SIGINT);
    obj = NULL;
  }

  return obj;
}

static co_thread_lock_t * co_thread_lock_init_internal(co_thread_lock_t ** objp, bool lock)
{
  co_thread_lock_t * obj = NULL;
  bool fok = false;

  if ( lock ) {
    co_thread_global_lock();
  }

  if ( !(obj = calloc(1, sizeof(struct co_thread_lock_t))) ) {
    goto end;
  }

  obj->e.type = iowait_eventfd;

  if ( !(obj->e.so = eventfd(0, 0)) ) {
    goto end;
  }
  if ( !set_non_blocking(obj->e.so, true) ) {
    goto end;
  }
  if ( !epoll_add(&obj->e, EPOLLIN) ) {
    goto end;
  }

  fok = true;

end :

  if ( !fok && obj ) {
    if ( obj->e.so != -1 ) {
      epoll_remove(obj->e.so);
      close(obj->e.so);
    }
    free(obj);
    obj = NULL;
  }

  *objp = obj;

  if ( lock ) {
    co_thread_global_unlock();
  }

  return obj;
}

bool co_thread_lock_init(co_thread_lock_t ** objp)
{
  return co_thread_lock_init_internal(objp, true) != NULL;
}

void co_thread_lock_destroy(co_thread_lock_t ** objp)
{
  co_thread_global_lock();

  if ( objp && *objp ) {
    if ( (*objp)->e.so != -1 ) {
      epoll_remove((*objp)->e.so);
      close((*objp)->e.so);
    }
    free(*objp);
    *objp = NULL;
  }

  co_thread_global_unlock();
}

// must be globally locked
static void co_thread_lock_internal(co_thread_lock_t * obj)
{
  struct cclist_node * node =
      add_waiter(current_core,
          &(struct io_waiter ) {
                .co = co_current(),
                .tmo = -1,
                .mask = EPOLLIN
              });

  epoll_queue(&obj->e, cclist_peek(node));

  while ( obj->co ) {
    co_thread_global_unlock();

      co_call(current_core->main);

    co_thread_global_lock();
  }
  obj->co = co_current();


  epoll_dequeue(&obj->e, cclist_peek(node));
  remove_waiter(current_core, node);
}

// must be globally locked
static void co_thread_unlock_internal(co_thread_lock_t * obj)
{
  obj->co = NULL;
  eventfd_write(obj->e.so, 1);
}

bool co_thread_lock(co_thread_lock_t ** objp)
{
  co_thread_lock_t * obj;

  co_thread_global_lock();
  if ( (obj = *objp) || (obj = co_thread_lock_init_internal(objp, false)) ) {
    co_thread_lock_internal(obj);
  }
  co_thread_global_unlock();

  return obj != NULL;
}

bool co_thread_unlock(co_thread_lock_t ** objp)
{
  struct co_thread_lock_t * obj;

  co_thread_global_lock();

  if ( (obj = co_thread_check(objp)) ) {
    co_thread_unlock_internal(obj);
  }

  co_thread_global_unlock();

  if ( obj ) {
    co_yield();
  }

  return obj != NULL;
}

static int co_thread_signal_internal(co_thread_lock_t ** objp, bool bc)
{
  co_thread_lock_t * obj;
  int nb_signalled = -1;

  co_thread_global_lock();

  if ( (obj = co_thread_check(objp)) ) {

    nb_signalled = 0;

    for ( struct io_waiter * iow = obj->e.head; iow != NULL; iow = iow->next ) {

      if ( iow->flags & MTX_WAKEUP_WAITING ) {

        if ( !(iow->flags & MTX_WAKEUP_EVENT) ) {
          iow->flags |= MTX_WAKEUP_EVENT;
        }

        ++nb_signalled;
        if ( !bc ) {
          break;
        }
      }
    }
  }

  co_thread_global_unlock();

  if ( nb_signalled > 0 ) {
    eventfd_write(obj->e.so, 1);
    co_yield();
  }

  return nb_signalled;
}

int co_thread_signal(co_thread_lock_t ** objp)
{
  return co_thread_signal_internal(objp, false);
}

int co_thread_broadcast(co_thread_lock_t ** objp)
{
  return co_thread_signal_internal(objp, true);
}

int co_thread_wait(co_thread_lock_t ** objp, int tmo)
{
  struct co_thread_lock_t * obj;
  struct cclist_node * node;
  struct io_waiter * iow;
  int status = -1;

  co_thread_global_lock();

  if ( (obj = co_thread_check(objp)) ) {

    node = add_waiter(current_core,
        &(struct io_waiter ) {
              .co = obj->co,
              .tmo = tmo >= 0 ? co_current_time_ms() + tmo : -1,
              .mask = EPOLLIN,
              .flags = MTX_WAKEUP_WAITING
            });

    epoll_queue(&obj->e, iow = cclist_peek(node));

    co_thread_unlock_internal(obj);

    while ( !(status = ((iow->flags & MTX_WAKEUP_EVENT) != 0)) && (iow->tmo < 0 || co_current_time_ms() < iow->tmo) ) {
      co_thread_global_unlock();

        co_call(current_core->main);

      co_thread_global_lock();
    }

    co_thread_lock_internal(obj);

    epoll_dequeue(&obj->e, cclist_peek(node));
    remove_waiter(current_core, node);
  }

  co_thread_global_unlock();

  if ( status == 0 ) {
    errno = ETIME;
  }

  return status;
}





//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


struct co_socket {
  struct iorq e;
  int recvtmo, sendtmo;
};

co_socket * co_socket_attach(int so)
{
  co_socket * cc = NULL;

  if ( (cc = calloc(1, sizeof(*cc))) ) {
    cc->e.so = so;
    cc->e.type = iowait_io;
    cc->e.head = NULL;
    cc->recvtmo = cc->sendtmo = -1;
    if ( !so_set_non_blocking(so, 1) || !epoll_add(&cc->e, EPOLLIN | EPOLLOUT) ) {
      free(cc), cc = NULL;
    }
  }

  return cc;
}

co_socket * co_socket_new(int af, int sock_type, int proto)
{
  co_socket * cc = NULL;
  int so = -1;

  if ( !af ) {
    af = AF_INET;
  }

  if ( !sock_type ) {
    sock_type = SOCK_STREAM;
  }

  if ( !proto ) {
    proto = IPPROTO_TCP;
  }

  if ( (so = socket(af, sock_type, proto)) != -1 && !(cc = co_socket_attach(so)) ) {
    close(so);
  }

  return cc;
}

void co_socket_free(co_socket ** cc)
{
  if ( cc && *cc ) {
    free(*cc);
    *cc = NULL;
  }
}


void co_socket_close(co_socket ** cc, bool abort_conn)
{
  if ( cc && *cc ) {
    if ( (*cc)->e.so != -1 ) {
      epoll_remove((*cc)->e.so);
      so_close((*cc)->e.so, abort_conn);
    }
    co_socket_free(cc);
  }
}

co_socket * co_socket_attach_and_connect(int so, const struct sockaddr *address, socklen_t addrslen, int tmo_ms)
{
  co_socket * cc = NULL;

  if ( (cc = co_socket_attach(so)) && !co_socket_connect(cc, address, addrslen, tmo_ms) ) {
    co_socket_free(&cc);
  }
  return cc;
}

bool co_socket_connect(co_socket * cc, const struct sockaddr *address, socklen_t addrslen, int tmo_ms)
{
  struct io_waiter * w;
  int status;

  struct cclist_node * node =
      add_waiter(current_core, &(struct io_waiter ) {
        .co = co_current(),
        .tmo = tmo_ms >= 0 ? co_current_time_ms() + tmo_ms: -1,
        .mask = EPOLLOUT
      });

  epoll_queue(&cc->e, w = cclist_peek(node));

  errno = 0;
  if ( (status = connect(cc->e.so, address, addrslen)) == -1 && errno == EINPROGRESS ) {
    do {
      co_call(current_core->main);
    } while ( !(w->revents & (EPOLLOUT | EPOLLERR)) && (w->tmo == -1 || co_current_time_ms() <= w->tmo) );

    if ( !(w->revents & EPOLLERR) ) {
      status = 0;
    }
    else {
      errno = so_get_error(cc->e.so);
      status = -1;
    }
  }

  epoll_dequeue(&cc->e, w);
  remove_waiter(current_core, node);

  return status == 0;
}


int co_socket_fd(const co_socket * cc)
{
  return cc->e.so;
}

bool co_socket_set_send_tmout(co_socket * cc, int msec)
{
  if ( cc ) {
    cc->sendtmo = msec;
  }
  else {
    errno = EBADF;
  }
  return cc != NULL;
}

bool co_socket_set_recv_tmout(co_socket * cc, int msec)
{
  if ( cc ) {
    cc->recvtmo = msec;
  }
  else {
    errno = EBADF;
  }
  return cc != NULL;
}

bool co_socket_set_sndrcv_tmouts(co_socket * cc, int snd_tmout_msec, int rcv_tmout_msec)
{
  if ( !cc ) {
    errno = EBADF;
  }
  else {
    cc->sendtmo = snd_tmout_msec;
    cc->recvtmo = rcv_tmout_msec;
  }
  return cc != NULL;
}

ssize_t co_socket_send(co_socket * cc, const void * buf, size_t buf_size, int flags)
{
  const uint8_t * pb = buf;
  ssize_t size, sent = -1;

  if ( !cc ) {
    errno = EBADF;
  }
  else if ( !pb ) {
    errno = EINVAL;
  }
  else {

    struct cclist_node * node =
        add_waiter(current_core,
            &(struct io_waiter ) {
              .co = co_current(),
              .tmo = cc->sendtmo >= 0 ? co_current_time_ms() + cc->sendtmo: -1,
              .mask = EPOLLOUT
              });

    struct io_waiter * w  =
          cclist_peek(node);

    epoll_queue(&cc->e, w);

    sent = 0;
    while ( sent < (ssize_t) buf_size ) {

      if ( (size = send(cc->e.so, pb + sent, buf_size - sent, flags | MSG_NOSIGNAL | MSG_DONTWAIT)) > 0 ) {
        sent += size;
        if ( cc->sendtmo >= 0 ) {
          w->tmo = co_current_time_ms() + cc->sendtmo * 1000;
        }
      }
      else if ( errno == EAGAIN ) {
        co_call(current_core->main);
        if ( !(w->revents & EPOLLOUT) ) {
          break;
        }
      }
      else {
        sent = size;
        break;
      }
    }

    epoll_dequeue(&cc->e, w);
    remove_waiter(current_core, node);
  }

  return sent;
}

ssize_t co_socket_recv(co_socket * cc, void * buf, size_t buf_size, int flags)
{
  struct io_waiter * w;
  ssize_t size = -1;

  if ( !cc ) {
    errno = EBADF;
  }
  else if ( !buf ) {
    errno = EINVAL;
  }
  else {

    struct cclist_node * node =
        add_waiter(current_core, &(struct io_waiter ) {
          .co = co_current(),
          .tmo = cc->recvtmo >= 0 ? co_current_time_ms() + cc->recvtmo : -1,
          .mask = EPOLLIN
        });


    epoll_queue(&cc->e, w = cclist_peek(node));

    size = 0;
    while ( (size = recv(cc->e.so, buf, buf_size, flags | MSG_DONTWAIT | MSG_NOSIGNAL)) < 0 && errno == EAGAIN ) {
      if ( w->tmo != -1 && co_current_time_ms() >= w->tmo ) {
        errno = ETIME;
        break;
      }
      co_call(current_core->main);
    }

    epoll_dequeue(&cc->e, w);
    remove_waiter(current_core, node);

    if ( size == 0 ) {
      errno = ECONNRESET;
    }
  }

  return size;
}


co_socket * co_socket_accept_new(co_socket * cc, struct sockaddr * addrs, socklen_t * addrslen)
{
  int so = -1;
  co_socket * cc2 = NULL;

  if ( !cc ) {
    errno = EBADF;
  }
  else {

    struct io_waiter * w;

    struct cclist_node * node =
        add_waiter(current_core, &(struct io_waiter ) {
          .co = co_current(),
          .tmo = cc->recvtmo >= 0 ? co_current_time_ms() + cc->recvtmo : -1,
          .mask = EPOLLIN
        });

    epoll_queue(&cc->e, w = cclist_peek(node));

    while ( (so = accept(cc->e.so, addrs, addrslen)) == -1 && errno == EAGAIN ) {
      if ( w->tmo != -1 && co_current_time_ms() >= w->tmo ) {
        break;
      }
      co_call(current_core->main);
    }

    epoll_dequeue(&cc->e, w);
    remove_waiter(current_core, node);

    if ( so != -1 ) {
      cc2 = co_socket_attach(so);
    }
  }

  return cc2;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// co io

uint32_t co_io_wait(int so, uint32_t events, int msec)
{
  struct cclist_node * node;
  uint32_t revents = 0;

  struct iorq e = {
    .so = so,
    .type = iowait_io,
    .head = cclist_peek(node =
        add_waiter(current_core, &(struct io_waiter ) {
          .co = co_current(),
          .tmo = msec < 0 ? -1 : co_current_time_ms() + msec,
          .mask = events,
        })),
  };

  if ( !epoll_add(&e, events) ) {
    CF_CRITICAL("emgr_add(so=%d) fails: %s", so, strerror(errno));
    raise(SIGINT);// for gdb
    exit(1);
  }

  co_call(current_core->main);

  revents = e.head->revents;

  epoll_remove(so);
  remove_waiter(current_core, node);

  return revents;
}


ssize_t co_send(int so, const void * buf, size_t buf_size, int flags)
{
  const uint8_t * pb = buf;
  ssize_t size, sent = 0;

  while ( sent < (ssize_t) buf_size ) {
    if ( (size = send(so, pb + sent, buf_size - sent, flags | MSG_DONTWAIT)) > 0 ) {
      sent += size;
    }
    else if ( (errno != EAGAIN) || !(co_io_wait(so, EPOLLOUT, -1) & EPOLLOUT) ) {
      break;
    }
  }

  return sent;
}


ssize_t co_recv(int so, void * buf, size_t buf_size, int flags)
{
  ssize_t size;
  while ( (size = recv(so, buf, buf_size, flags | MSG_DONTWAIT)) < 0 && errno == EAGAIN ) {
    if ( !(co_io_wait(so, EPOLLIN, -1) & EPOLLIN) ) {
      break;
    }
  }
  if ( size == 0 ) {
    errno = ECONNRESET;
  }
  else if ( size < 0 ) {
    errno = so_get_error(so);
  }
  return size;
}


ssize_t co_read(int fd, void * buf, size_t buf_size)
{
  ssize_t size;
  while ( (size = read(fd, buf, buf_size)) < 0 && errno == EAGAIN ) {
    if ( !(co_io_wait(fd, EPOLLIN, -1) & EPOLLIN) ) {
      break;
    }
  }
  return size;
}


ssize_t co_write(int fd, const void *buf, size_t buf_size)
{
  const uint8_t * pb = buf;
  ssize_t size, sent = 0;

  while ( sent < (ssize_t) buf_size ) {
    if ( (size = write(fd, pb + sent, buf_size - sent)) > 0 ) {
      sent += size;
    }
    else if ( errno == EAGAIN ) {
      co_io_wait(fd, EPOLLOUT, -1);
    }
    else {
      break;
    }
  }

  return sent;
}


int co_connect(int so, const struct sockaddr *address, socklen_t address_len)
{
  uint32_t revents;
  int status;
  errno = 0;
  if ( (status = connect(so, address, address_len)) == -1 && errno == EINPROGRESS ) {
    do { // wait for EPOLLOUT
    } while ( !((revents = co_io_wait(so, EPOLLOUT, -1)) & (EPOLLOUT | EPOLLERR)) );

    if ( !(revents & EPOLLERR) ) {
      status = 0;
    }
    else {
      errno = so_get_error(so);
      status = -1;
    }
  }

  return status;
}


int co_accept(int sso, struct sockaddr * restrict address, socklen_t * restrict address_len)
{
  int so;
  while ( (so = accept(sso, address, address_len)) == -1 && errno == EAGAIN ) {
    co_io_wait(sso, EPOLLIN, -1);
  }
  if ( so != -1 ) {
    set_non_blocking(so, true);
  }
  return so;
}

int co_poll(struct pollfd *__fds, nfds_t __nfds, int __timeout_ms)
{
  struct {
    struct iorq e;
    struct cclist_node * node;
  } c[__nfds];

  int64_t tmo = __timeout_ms >= 0 ? co_current_time_ms() + __timeout_ms : -1;
  coroutine_t co = co_current();
  uint32_t event_mask;

  int n = 0;


  for ( nfds_t i = 0; i < __nfds; ++i ) {

    event_mask = ((__fds[i].events & POLLIN) ? EPOLLIN : 0) | ((__fds[i].events & POLLOUT) ? EPOLLOUT : 0);
    __fds[i].revents = 0;

    c[i].e.so = __fds[i].fd;
    c[i].e.type = iowait_io;
    c[i].e.head = cclist_peek(c[i].node =
        add_waiter(current_core, &(struct io_waiter ) {
          .co = co,
          .tmo = tmo,
          .mask = event_mask,
        }));


    if ( !c[i].node ) {
      CF_FATAL("add_waiter() fails: %s", strerror(errno));
      exit(1);
    }

    if ( !epoll_add(&c[i].e, event_mask | EPOLLONESHOT) ) {
      CF_FATAL("emgr_add() fails: %s", strerror(errno));
      exit(1);
    }
  }

  co_call(current_core->main);

  for ( nfds_t i = 0; i < __nfds; ++i ) {

    epoll_remove(c[i].e.so);

    if ( (__fds[i].events & POLLIN) && (c[i].e.head->revents & EPOLLIN) ) {
      __fds[i].revents |= POLLIN;
    }

    if ( (__fds[i].events & POLLOUT) && (c[i].e.head->revents & EPOLLOUT) ) {
      __fds[i].revents |= POLLOUT;
    }

    if ( (c[i].e.head->revents & EPOLLERR) ) {
      __fds[i].revents |= POLLERR;
    }

    if ( __fds[i].revents ) {
      ++n;
    }

    remove_waiter(current_core, c[i].node);
  }

  return n;
}

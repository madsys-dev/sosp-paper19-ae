#ifndef HL_FRED_H
#define HL_FRED_H

/// A thread-wrapper of childlike simplicity :).

#include <pthread.h>
#include <unistd.h>

typedef void * (*ThreadFunctionType) (void *);

namespace HL {

class Fred {
public:

  Fred() {
    pthread_attr_init (&attr);
    pthread_attr_setscope (&attr, PTHREAD_SCOPE_SYSTEM);
  }

  ~Fred() {
    pthread_attr_destroy (&attr);
  }

  void create (ThreadFunctionType function, void * arg) {
    pthread_create (&t, &attr, function, arg);
  }

  void join (void) {
    pthread_join (t, NULL);
  }

  static void yield (void) {
    sched_yield();
  }


  static void setConcurrency (int n) {
    pthread_setconcurrency (n);
  }


private:
  typedef pthread_t FredType;
  pthread_attr_t attr;

  FredType t;
};

}


#endif
/*
 * Cross Platform Thread/Mutex abstraction
 * Copyright(C) 2007 Michael Jerris
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so.
 *
 * This work is provided under this license on an "as is" basis, without
 * warranty of any kind, either expressed or implied, including, without
 * limitation, warranties that the covered code is free of defects,
 * merchantable, fit for a particular purpose or non-infringing. The entire risk
 * as to the quality and performance of the covered code is with you. Should any
 * covered code prove defective in any respect, you (not the initial developer
 * or any other contributor) assume the cost of any necessary servicing, repair
 * or correction. This disclaimer of warranty constitutes an essential part of
 * this license. No use of any covered code is authorized hereunder except under
 * this disclaimer.
 *
 */

#include "esl_threadmutex.h"
#include "esl.h"

#include <pthread.h>

struct esl_mutex {
  pthread_mutex_t mutex;
};

struct esl_thread {
  pthread_t handle;
  void *private_data;
  esl_thread_function_t function;
  size_t stack_size;
  pthread_attr_t attribute;
};

size_t thread_default_stacksize = 240 * 1024;

void esl_thread_override_default_stacksize(size_t size) {
  thread_default_stacksize = size;
}

static void *thread_launch(void *args) {
  auto *thread = (esl_thread_t *)args;
  void *exit_val = thread->function(thread, thread->private_data);
  pthread_attr_destroy(&thread->attribute);
  free(thread);

  return exit_val;
}

ESL_DECLARE(esl_status_t)
esl_thread_create_detached(esl_thread_function_t func, void *data) {
  return esl_thread_create_detached_ex(func, data, thread_default_stacksize);
}

esl_status_t esl_thread_create_detached_ex(esl_thread_function_t func,
                                           void *data, size_t stack_size) {
  esl_thread_t *thread = nullptr;
  esl_status_t status = ESL_FAIL;

  if (!func) {
    goto done;
  }

  thread = (esl_thread_t *)calloc(1, sizeof(esl_thread_t));
  if (thread == nullptr) {
    goto done;
  }

  thread->private_data = data;
  thread->function = func;
  thread->stack_size = stack_size;

  if (pthread_attr_init(&thread->attribute) != 0) {
    goto fail;
  }

  if (pthread_attr_setdetachstate(&thread->attribute,
                                  PTHREAD_CREATE_DETACHED) != 0) {
    goto failpthread;
  }

  if (thread->stack_size &&
      pthread_attr_setstacksize(&thread->attribute, thread->stack_size) != 0) {
    goto failpthread;
  }

  if (pthread_create(&thread->handle, &thread->attribute, thread_launch,
                     thread) != 0) {
    goto failpthread;
  }

  status = ESL_SUCCESS;
  goto done;

failpthread:

  pthread_attr_destroy(&thread->attribute);

fail:
  free(thread);
done:
  return status;
}

ESL_DECLARE(esl_status_t) esl_mutex_create(esl_mutex_t **mutex) {
  esl_status_t status = ESL_FAIL;
  pthread_mutexattr_t attr;
  esl_mutex_t *check = nullptr;

  check = (esl_mutex_t *)calloc(1, sizeof(**mutex));
  if (!check) {
    goto done;
  }

  if (pthread_mutexattr_init(&attr)) {
    free(check);
    goto done;
  }

  if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE)) {
    goto fail;
  }

  if (pthread_mutex_init(&check->mutex, &attr)) {
    goto fail;
  }

  goto success;

fail:
  pthread_mutexattr_destroy(&attr);
  free(check);
  goto done;

success:
  *mutex = check;
  status = ESL_SUCCESS;

done:
  return status;
}

ESL_DECLARE(esl_status_t) esl_mutex_destroy(esl_mutex_t **mutex) {
  esl_mutex_t *mp = *mutex;
  *mutex = nullptr;
  if (!mp) {
    return ESL_FAIL;
  }
  if (pthread_mutex_destroy(&mp->mutex)) {
    return ESL_FAIL;
  }
  free(mp);
  return ESL_SUCCESS;
}

ESL_DECLARE(esl_status_t) esl_mutex_lock(esl_mutex_t *mutex) {
  if (pthread_mutex_lock(&mutex->mutex)) {
    return ESL_FAIL;
  }
  return ESL_SUCCESS;
}

ESL_DECLARE(esl_status_t) esl_mutex_trylock(esl_mutex_t *mutex) {
  if (pthread_mutex_trylock(&mutex->mutex)) {
    return ESL_FAIL;
  }
  return ESL_SUCCESS;
}

ESL_DECLARE(esl_status_t) esl_mutex_unlock(esl_mutex_t *mutex) {
  if (pthread_mutex_unlock(&mutex->mutex)) {
    return ESL_FAIL;
  }
  return ESL_SUCCESS;
}

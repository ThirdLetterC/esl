/*
 * Fundamental ESL type and macro definitions extracted to break cyclic
 * header inclusions.
 */
#pragma once

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifndef ESL_DECLARE
#define ESL_DECLARE(type) type
#define ESL_DECLARE_NONSTD(type) type
#define ESL_DECLARE_DATA
#endif

typedef int esl_socket_t;
typedef ssize_t esl_ssize_t;
typedef int esl_filehandle_t;
typedef int16_t esl_port_t;
typedef size_t esl_size_t;

typedef enum {
  ESL_SUCCESS,
  ESL_FAIL,
  ESL_BREAK,
  ESL_DISCONNECTED,
  ESL_GENERR
} esl_status_t;

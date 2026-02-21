/*
 * Copyright (c) 2007-2014, Anthony Minessale II
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <assert.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "esl/esl_base.h"
#include "esl_buffer.h"

static inline int esl_copy_string(char *x, const char *y, size_t z) {
  if (x == nullptr || z == 0) {
    return -1;
  }
  if (y == nullptr) {
    x[0] = '\0';
    return 0;
  }
  return snprintf(x, z, "%s", y);
}
static inline int esl_set_string_impl(char *x, size_t z, const char *y) {
  return esl_copy_string(x, y, z);
}
#define esl_set_string(_x, _y) esl_set_string_impl((_x), sizeof(_x), (_y))
#define ESL_VA_NONE "%s", ""

typedef struct esl_event_header esl_event_header_t;
typedef struct esl_event esl_event_t;
typedef struct esl_mutex esl_mutex_t;

typedef enum {
  ESL_POLL_READ = (1 << 0),
  ESL_POLL_WRITE = (1 << 1),
  ESL_POLL_ERROR = (1 << 2)
} esl_poll_t;

typedef enum {
  ESL_EVENT_TYPE_PLAIN,
  ESL_EVENT_TYPE_XML,
  ESL_EVENT_TYPE_JSON
} esl_event_type_t;

static const char ESL_SEQ_ESC[] = "\033[";
/* Ansi Control character suffixes */
static constexpr char ESL_SEQ_HOME_CHAR = 'H';
static const char ESL_SEQ_HOME_CHAR_STR[] = "H";
static constexpr char ESL_SEQ_CLEARLINE_CHAR = '1';
static const char ESL_SEQ_CLEARLINE_CHAR_STR[] = "1";
static const char ESL_SEQ_CLEARLINEEND_CHAR[] = "K";
static constexpr char ESL_SEQ_CLEARSCR_CHAR0 = '2';
static constexpr char ESL_SEQ_CLEARSCR_CHAR1 = 'J';
static const char ESL_SEQ_CLEARSCR_CHAR[] = "2J";
static const char ESL_SEQ_DEFAULT_COLOR[] =
    "\033[m"; /* Reset to default fg/bg */
static const char ESL_SEQ_AND_COLOR[] =
    ";"; /* To add multiple color definitions */
static const char ESL_SEQ_END_COLOR[] = "m"; /* To end color definitions */
/* Foreground colors values */
static const char ESL_SEQ_F_BLACK[] = "30";
static const char ESL_SEQ_F_RED[] = "31";
static const char ESL_SEQ_F_GREEN[] = "32";
static const char ESL_SEQ_F_YELLOW[] = "33";
static const char ESL_SEQ_F_BLUE[] = "34";
static const char ESL_SEQ_F_MAGEN[] = "35";
static const char ESL_SEQ_F_CYAN[] = "36";
static const char ESL_SEQ_F_WHITE[] = "37";
/* Background colors values */
static const char ESL_SEQ_B_BLACK[] = "40";
static const char ESL_SEQ_B_RED[] = "41";
static const char ESL_SEQ_B_GREEN[] = "42";
static const char ESL_SEQ_B_YELLOW[] = "43";
static const char ESL_SEQ_B_BLUE[] = "44";
static const char ESL_SEQ_B_MAGEN[] = "45";
static const char ESL_SEQ_B_CYAN[] = "46";
static const char ESL_SEQ_B_WHITE[] = "47";
/* Preset escape sequences - Change foreground colors only */
static const char ESL_SEQ_FBLACK[] = "\033[30m";
static const char ESL_SEQ_FRED[] = "\033[31m";
static const char ESL_SEQ_FGREEN[] = "\033[32m";
static const char ESL_SEQ_FYELLOW[] = "\033[33m";
static const char ESL_SEQ_FBLUE[] = "\033[34m";
static const char ESL_SEQ_FMAGEN[] = "\033[35m";
static const char ESL_SEQ_FCYAN[] = "\033[36m";
static const char ESL_SEQ_FWHITE[] = "\033[37m";
static const char ESL_SEQ_BBLACK[] = "\033[40m";
static const char ESL_SEQ_BRED[] = "\033[41m";
static const char ESL_SEQ_BGREEN[] = "\033[42m";
static const char ESL_SEQ_BYELLOW[] = "\033[43m";
static const char ESL_SEQ_BBLUE[] = "\033[44m";
static const char ESL_SEQ_BMAGEN[] = "\033[45m";
static const char ESL_SEQ_BCYAN[] = "\033[46m";
static const char ESL_SEQ_BWHITE[] = "\033[47m";
/* Preset escape sequences */
static const char ESL_SEQ_HOME[] = "\033[H";
static const char ESL_SEQ_CLEARLINE[] = "\033[1";
static const char ESL_SEQ_CLEARLINEEND[] = "\033[K";
static const char ESL_SEQ_CLEARSCR[] = "\033[2J\033[H";

#define esl_assert(_x) assert(_x)

static inline void esl_safe_free_ptr(void **ptr) {
  if (ptr && *ptr) {
    free(*ptr);
    *ptr = nullptr;
  }
}
#define esl_safe_free(_x) esl_safe_free_ptr((void **)&(_x))

static inline bool esl_strlen_zero(const char *s) {
  return (s == nullptr) || (*s == '\0');
}

static inline bool esl_strlen_zero_buf(const char *s) { return (*s == '\0'); }

static inline char esl_end_of(const char *s) {
  return *(*s == '\0' ? s : s + strlen(s) - 1);
}
#define end_of(_s) esl_end_of(_s)

constexpr esl_socket_t ESL_SOCK_INVALID = -1;

constexpr esl_size_t BUF_CHUNK = 65'536 * 50;
constexpr esl_size_t BUF_START = 65'536 * 100;

/*! \brief A handle that will hold the socket information and
           different events received. */
typedef struct {
  struct sockaddr_storage sockaddr;
  struct hostent hostent;
  char hostbuf[256];
  esl_socket_t sock;
  /*! In case of socket error, this will hold the error description as reported
   * by the OS */
  char err[256];
  /*! The error number reported by the OS */
  int errnum;
  /*! The inner contents received by the socket. Used only internally. */
  esl_buffer_t *packet_buf;
  char socket_buf[65536];
  /*! Last command reply */
  char last_reply[1024];
  /*! Last command reply when called with esl_send_recv */
  char last_sr_reply[1024];
  /*! Last event received. Only populated when **save_event is nullptr */
  esl_event_t *last_event;
  /*! Last event received when called by esl_send_recv */
  esl_event_t *last_sr_event;
  /*! This will hold already processed events queued by esl_recv_event */
  esl_event_t *race_event;
  /*! Events that have content-type == text/plain and a body */
  esl_event_t *last_ievent;
  /*! For outbound socket. Will hold reply information when connect\n\n is sent
   */
  esl_event_t *info_event;
  /*! Socket is connected or not */
  int connected;
  struct sockaddr_in addr;
  /*! Internal mutex */
  esl_mutex_t *mutex;
  int async_execute;
  int event_lock;
  int destroyed;
} esl_handle_t;

#define esl_test_flag(obj, flag) ((obj)->flags & (flag))
#define esl_set_flag(obj, flag) (obj)->flags |= (flag)
#define esl_clear_flag(obj, flag) (obj)->flags &= ~(flag)

#ifndef __FUNCTION__
#define __FUNCTION__ (const char *)__func__
#endif

typedef enum {
  ESL_LOG_LEVEL_EMERG = 0,
  ESL_LOG_LEVEL_ALERT = 1,
  ESL_LOG_LEVEL_CRIT = 2,
  ESL_LOG_LEVEL_ERROR = 3,
  ESL_LOG_LEVEL_WARNING = 4,
  ESL_LOG_LEVEL_NOTICE = 5,
  ESL_LOG_LEVEL_INFO = 6,
  ESL_LOG_LEVEL_DEBUG = 7
} esl_log_level_t;

#define ESL_PRE __FILE__, __FUNCTION__, __LINE__

#define ESL_LOG_DEBUG ESL_PRE, ESL_LOG_LEVEL_DEBUG
#define ESL_LOG_INFO ESL_PRE, ESL_LOG_LEVEL_INFO
#define ESL_LOG_NOTICE ESL_PRE, ESL_LOG_LEVEL_NOTICE
#define ESL_LOG_WARNING ESL_PRE, ESL_LOG_LEVEL_WARNING
#define ESL_LOG_ERROR ESL_PRE, ESL_LOG_LEVEL_ERROR
#define ESL_LOG_CRIT ESL_PRE, ESL_LOG_LEVEL_CRIT
#define ESL_LOG_ALERT ESL_PRE, ESL_LOG_LEVEL_ALERT
#define ESL_LOG_EMERG ESL_PRE, ESL_LOG_LEVEL_EMERG
typedef void (*esl_logger_t)(const char *file, const char *func, int line,
                             int level, const char *fmt, ...);

[[nodiscard]] ESL_DECLARE(int)
    esl_vasprintf(char **ret, const char *fmt, va_list ap);

ESL_DECLARE_DATA extern esl_logger_t esl_log;

/*! Sets the logger for libesl. Default is the null_logger */
ESL_DECLARE(void) esl_global_set_logger(esl_logger_t logger);
/*! Sets the default log level for libesl */
ESL_DECLARE(void) esl_global_set_default_logger(int level);

ESL_DECLARE(size_t) esl_url_encode(const char *url, char *buf, size_t len);
ESL_DECLARE(char *) esl_url_decode(char *s);
ESL_DECLARE(const char *) esl_stristr(const char *instr, const char *str);
ESL_DECLARE(int) esl_toupper(int c);
ESL_DECLARE(int) esl_tolower(int c);
ESL_DECLARE(int) esl_snprintf(char *buffer, size_t count, const char *fmt, ...);

typedef void (*esl_listen_callback_t)(esl_socket_t server_sock,
                                      esl_socket_t client_sock,
                                      struct sockaddr_in *addr,
                                      void *user_data);
/*!
    \brief Attach a handle to an established socket connection
    \param handle Handle to be attached
    \param socket Socket to which the handle will be attached
    \param addr Structure that will contain the connection descritption (look up
   your os info)
*/
[[nodiscard]] ESL_DECLARE(esl_status_t)
    esl_attach_handle(esl_handle_t *handle, esl_socket_t socket,
                      struct sockaddr_in *addr);
/*!
    \brief Will bind to host and callback when event is received. Used for
   outbound socket.
    \param host Host to bind to
    \param port Port to bind to
    \param callback Callback that will be called upon data received
*/

[[nodiscard]] ESL_DECLARE(esl_status_t)
    esl_listen(const char *host, esl_port_t port,
               esl_listen_callback_t callback, void *user_data,
               esl_socket_t *server_sockP);
[[nodiscard]] ESL_DECLARE(esl_status_t)
    esl_listen_threaded(const char *host, esl_port_t port,
                        esl_listen_callback_t callback, void *user_data,
                        int max);
/*!
    \brief Executes application with sendmsg to a specific UUID. Used for
   outbound socket.
    \param handle Handle that the msg will be sent
    \param app Application to execute
    \param arg Application arguments
    \param uuid Target UUID for the application
*/
[[nodiscard]] ESL_DECLARE(esl_status_t)
    esl_execute(esl_handle_t *handle, const char *app, const char *arg,
                const char *uuid);
/*!
    \brief Send an event
    \param handle Handle to which the event should be sent
    \param event Event to be sent
*/
[[nodiscard]] ESL_DECLARE(esl_status_t)
    esl_sendevent(esl_handle_t *handle, esl_event_t *event);

/*!
    \brief Send an event as a message to be parsed
    \param handle Handle to which the event should be sent
    \param event Event to be sent
        \param uuid a specific uuid if not the default
*/
[[nodiscard]] ESL_DECLARE(esl_status_t)
    esl_sendmsg(esl_handle_t *handle, esl_event_t *event, const char *uuid);

/*!
    \brief Connect a handle to a host/port with a specific password. This will
   also authenticate against the server
    \param handle Handle to connect
    \param host Host to be connected
    \param port Port to be connected
    \param password FreeSWITCH server username (optional)
    \param password FreeSWITCH server password
        \param timeout Connection timeout, in miliseconds
*/
[[nodiscard]] ESL_DECLARE(esl_status_t)
    esl_connect_timeout(esl_handle_t *handle, const char *host, esl_port_t port,
                        const char *user, const char *password,
                        uint32_t timeout);

[[nodiscard]] static inline esl_status_t
esl_connect(esl_handle_t *handle, const char *host, esl_port_t port,
            const char *user, const char *password) {
  return esl_connect_timeout(handle, host, port, user, password, 0);
}

/*!
    \brief Disconnect a handle
    \param handle Handle to be disconnected
*/
[[nodiscard]] ESL_DECLARE(esl_status_t) esl_disconnect(esl_handle_t *handle);
/*!
    \brief Send a raw command using specific handle
    \param handle Handle to send the command to
    \param cmd Command to send
*/
[[nodiscard]] ESL_DECLARE(esl_status_t)
    esl_send(esl_handle_t *handle, const char *cmd);
/*!
    \brief Poll the handle's socket until an event is received or a connection
   error occurs
    \param handle Handle to poll
    \param check_q If set to 1, will check the handle queue (handle->race_event)
   and return the last event from it
    \param[out] save_event If this is not nullptr, will return the event
   received
*/
[[nodiscard]] ESL_DECLARE(esl_status_t)
    esl_recv_event(esl_handle_t *handle, int check_q, esl_event_t **save_event);
/*!
    \brief Poll the handle's socket until an event is received, a connection
   error occurs or ms expires
    \param handle Handle to poll
    \param ms Maximum time to poll
    \param check_q If set to 1, will check the handle queue (handle->race_event)
   and return the last event from it
    \param[out] save_event If this is not nullptr, will return the event
   received
*/
[[nodiscard]] ESL_DECLARE(esl_status_t)
    esl_recv_event_timed(esl_handle_t *handle, uint32_t ms, int check_q,
                         esl_event_t **save_event);
/*!
    \brief This will send a command and place its response event on
   handle->last_sr_event and handle->last_sr_reply
    \param handle Handle to be used
    \param cmd Raw command to send
*/
[[nodiscard]] ESL_DECLARE(esl_status_t)
    esl_send_recv_timed(esl_handle_t *handle, const char *cmd, uint32_t ms);
[[nodiscard]] static inline esl_status_t esl_send_recv(esl_handle_t *handle,
                                                       const char *cmd) {
  return esl_send_recv_timed(handle, cmd, 0);
}
/*!
    \brief Applies a filter to received events
    \param handle Handle to apply the filter to
    \param header Header that the filter will be based on
    \param value The value of the header to filter
*/
[[nodiscard]] ESL_DECLARE(esl_status_t)
    esl_filter(esl_handle_t *handle, const char *header, const char *value);
/*!
    \brief Will subscribe to events on the server
    \param handle Handle to which we will subscribe to events
    \param etype Event type to subscribe
    \param value Which event to subscribe to
*/
[[nodiscard]] ESL_DECLARE(esl_status_t)
    esl_events(esl_handle_t *handle, esl_event_type_t etype, const char *value);

[[nodiscard]] ESL_DECLARE(int)
    esl_wait_sock(esl_socket_t sock, uint32_t ms, esl_poll_t flags);

ESL_DECLARE(unsigned int)
esl_separate_string_string(char *buf, const char *delim, char **array,
                           unsigned int arraylen);

[[nodiscard]] static inline esl_status_t esl_recv(esl_handle_t *handle) {
  return esl_recv_event(handle, 0, nullptr);
}

[[nodiscard]] static inline esl_status_t esl_recv_timed(esl_handle_t *handle,
                                                        uint32_t ms) {
  return esl_recv_event_timed(handle, ms, 0, nullptr);
}

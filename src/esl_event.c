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

#include "esl/esl_event.h"
#include "esl/esl.h"
#include "esl/esl_json.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>

constexpr size_t ESL_EVENT_MAX_BODY_LENGTH = 16'777'216;
constexpr size_t ESL_EVENT_JSON_MAX_LENGTH = 16'777'216;
constexpr size_t ESL_EVENT_JSON_MAX_HEADERS = 4'096;
constexpr size_t ESL_EVENT_JSON_MAX_ARRAY_ITEMS = 4'096;
constexpr size_t ESL_EVENT_JSON_MAX_HEADER_NAME_LENGTH = 1'024;

[[nodiscard]] static bool esl_string_len_within_limit(const char *s,
                                                      size_t limit,
                                                      size_t *out_len) {
  size_t i = 0;

  if (s == nullptr) {
    return false;
  }

  for (i = 0; i <= limit; i++) {
    if (s[i] == '\0') {
      if (out_len != nullptr) {
        *out_len = i;
      }
      return true;
    }
  }

  return false;
}

static char *my_dup(const char *s) {
  if (s == nullptr) {
    return nullptr;
  }
  size_t len = strlen(s) + 1;
  void *new = calloc(len, sizeof(char));
  if (new == nullptr) {
    return nullptr;
  }

  return (char *)memcpy(new, s, len);
}

#ifndef ALLOC
#define ALLOC(size) calloc(1, (size))
#endif
#ifndef DUP
#define DUP(str) my_dup(str)
#endif
#ifndef FREE
#define FREE(ptr) esl_safe_free(ptr)
#endif

static void free_header(esl_event_header_t **header);

/* make sure this is synced with the esl_event_types_t enum in esl_types.h
   also never put any new ones before EVENT_ALL
*/
static const char *EVENT_NAMES[] = {"CUSTOM",
                                    "CLONE",
                                    "CHANNEL_CREATE",
                                    "CHANNEL_DESTROY",
                                    "CHANNEL_STATE",
                                    "CHANNEL_CALLSTATE",
                                    "CHANNEL_ANSWER",
                                    "CHANNEL_HANGUP",
                                    "CHANNEL_HANGUP_COMPLETE",
                                    "CHANNEL_EXECUTE",
                                    "CHANNEL_EXECUTE_COMPLETE",
                                    "CHANNEL_HOLD",
                                    "CHANNEL_UNHOLD",
                                    "CHANNEL_BRIDGE",
                                    "CHANNEL_UNBRIDGE",
                                    "CHANNEL_PROGRESS",
                                    "CHANNEL_PROGRESS_MEDIA",
                                    "CHANNEL_OUTGOING",
                                    "CHANNEL_PARK",
                                    "CHANNEL_UNPARK",
                                    "CHANNEL_APPLICATION",
                                    "CHANNEL_ORIGINATE",
                                    "CHANNEL_UUID",
                                    "API",
                                    "LOG",
                                    "INBOUND_CHAN",
                                    "OUTBOUND_CHAN",
                                    "STARTUP",
                                    "SHUTDOWN",
                                    "PUBLISH",
                                    "UNPUBLISH",
                                    "TALK",
                                    "NOTALK",
                                    "SESSION_CRASH",
                                    "MODULE_LOAD",
                                    "MODULE_UNLOAD",
                                    "DTMF",
                                    "MESSAGE",
                                    "PRESENCE_IN",
                                    "NOTIFY_IN",
                                    "PRESENCE_OUT",
                                    "PRESENCE_PROBE",
                                    "MESSAGE_WAITING",
                                    "MESSAGE_QUERY",
                                    "ROSTER",
                                    "CODEC",
                                    "BACKGROUND_JOB",
                                    "DETECTED_SPEECH",
                                    "DETECTED_TONE",
                                    "PRIVATE_COMMAND",
                                    "HEARTBEAT",
                                    "TRAP",
                                    "ADD_SCHEDULE",
                                    "DEL_SCHEDULE",
                                    "EXE_SCHEDULE",
                                    "RE_SCHEDULE",
                                    "RELOADXML",
                                    "NOTIFY",
                                    "PHONE_FEATURE",
                                    "PHONE_FEATURE_SUBSCRIBE",
                                    "SEND_MESSAGE",
                                    "RECV_MESSAGE",
                                    "REQUEST_PARAMS",
                                    "CHANNEL_DATA",
                                    "GENERAL",
                                    "COMMAND",
                                    "SESSION_HEARTBEAT",
                                    "CLIENT_DISCONNECTED",
                                    "SERVER_DISCONNECTED",
                                    "SEND_INFO",
                                    "RECV_INFO",
                                    "RECV_RTCP_MESSAGE",
                                    "SEND_RTCP_MESSAGE",
                                    "CALL_SECURE",
                                    "NAT",
                                    "RECORD_START",
                                    "RECORD_STOP",
                                    "PLAYBACK_START",
                                    "PLAYBACK_STOP",
                                    "CALL_UPDATE",
                                    "FAILURE",
                                    "SOCKET_DATA",
                                    "MEDIA_BUG_START",
                                    "MEDIA_BUG_STOP",
                                    "CONFERENCE_DATA_QUERY",
                                    "CONFERENCE_DATA",
                                    "CALL_SETUP_REQ",
                                    "CALL_SETUP_RESULT",
                                    "CALL_DETAIL",
                                    "DEVICE_STATE",
                                    "TEXT",
                                    "SHUTDOWN_REQUESTED",
                                    "ALL"};

ESL_DECLARE(const char *) esl_event_name(esl_event_types_t event) {
  if (event < ESL_EVENT_CUSTOM || event > ESL_EVENT_ALL) {
    return "INVALID";
  }
  return EVENT_NAMES[event];
}

ESL_DECLARE(esl_status_t)
esl_name_event(const char *name, esl_event_types_t *type) {
  esl_event_types_t x;

  if (name == nullptr || type == nullptr) {
    return ESL_FAIL;
  }

  for (x = 0; x <= ESL_EVENT_ALL; x++) {
    if ((strlen(name) > 13 && !strcasecmp(name + 13, EVENT_NAMES[x])) ||
        !strcasecmp(name, EVENT_NAMES[x])) {
      *type = x;
      return ESL_SUCCESS;
    }
  }

  return ESL_FAIL;
}

ESL_DECLARE(esl_status_t)
esl_event_create_subclass(esl_event_t **event, esl_event_types_t event_id,
                          const char *subclass_name) {
  if (event == nullptr) {
    return ESL_FAIL;
  }

  *event = nullptr;

  if ((event_id != ESL_EVENT_CLONE && event_id != ESL_EVENT_CUSTOM) &&
      subclass_name) {
    return ESL_FAIL;
  }

  *event = ALLOC(sizeof(esl_event_t));
  if (*event == nullptr) {
    return ESL_FAIL;
  }

  memset(*event, 0, sizeof(esl_event_t));

  if (event_id != ESL_EVENT_CLONE) {
    (*event)->event_id = event_id;
    if (esl_event_add_header_string(*event, ESL_STACK_BOTTOM, "Event-Name",
                                    esl_event_name((*event)->event_id)) !=
        ESL_SUCCESS) {
      esl_event_destroy(event);
      return ESL_FAIL;
    }
  }

  if (subclass_name) {
    (*event)->subclass_name = DUP(subclass_name);
    if ((*event)->subclass_name == nullptr) {
      esl_event_destroy(event);
      return ESL_FAIL;
    }
    if (esl_event_add_header_string(*event, ESL_STACK_BOTTOM, "Event-Subclass",
                                    subclass_name) != ESL_SUCCESS) {
      esl_event_destroy(event);
      return ESL_FAIL;
    }
  }

  return ESL_SUCCESS;
}

ESL_DECLARE(const char *) esl_priority_name(esl_priority_t priority) {
  switch (priority) { /*lol */
  case ESL_PRIORITY_NORMAL:
    return "NORMAL";
  case ESL_PRIORITY_LOW:
    return "LOW";
  case ESL_PRIORITY_HIGH:
    return "HIGH";
  default:
    return "INVALID";
  }
}

ESL_DECLARE(esl_status_t)
esl_event_set_priority(esl_event_t *event, esl_priority_t priority) {
  if (event == nullptr) {
    return ESL_FAIL;
  }

  event->priority = priority;
  return esl_event_add_header_string(event, ESL_STACK_TOP, "priority",
                                     esl_priority_name(priority));
}

constexpr esl_ssize_t ESL_HASH_KEY_STRING = -1;
constexpr int ESL_EVENT_HEADER_INDEX_MAX = 4'000;

[[nodiscard]] static bool esl_parse_event_header_index(const char *expr,
                                                       int *out_index) {
  char *endptr = nullptr;
  long parsed = 0;

  if (expr == nullptr || out_index == nullptr) {
    return false;
  }

  errno = 0;
  parsed = strtol(expr, &endptr, 10);
  if (errno == ERANGE || endptr == expr || endptr == nullptr ||
      *endptr != ']' || *(endptr + 1) != '\0') {
    return false;
  }

  if (parsed < 0 || parsed > ESL_EVENT_HEADER_INDEX_MAX) {
    return false;
  }

  *out_index = (int)parsed;
  return true;
}

static unsigned int esl_ci_hashfunc_default(const char *char_key,
                                            esl_ssize_t *klen)

{
  unsigned int hash = 0;
  const unsigned char *key = (const unsigned char *)char_key;
  const unsigned char *p;
  esl_ssize_t i;

  if (*klen == ESL_HASH_KEY_STRING) {
    for (p = key; *p; p++) {
      hash = hash * 33 + tolower(*p);
    }
    *klen = p - key;
  } else {
    for (p = key, i = *klen; i; i--, p++) {
      hash = hash * 33 + tolower(*p);
    }
  }

  return hash;
}

ESL_DECLARE(esl_event_header_t *)
esl_event_get_header_ptr(esl_event_t *event, const char *header_name) {
  esl_event_header_t *hp;
  esl_ssize_t hlen = -1;
  unsigned long hash = 0;

  if (event == nullptr || header_name == nullptr)
    return nullptr;

  hash = esl_ci_hashfunc_default(header_name, &hlen);

  for (hp = event->headers; hp; hp = hp->next) {
    if ((!hp->hash || hash == hp->hash) && !strcasecmp(hp->name, header_name)) {
      return hp;
    }
  }
  return nullptr;
}

ESL_DECLARE(char *)
esl_event_get_header_idx(esl_event_t *event, const char *header_name, int idx) {
  esl_event_header_t *hp;

  if (event == nullptr) {
    return nullptr;
  }

  if ((hp = esl_event_get_header_ptr(event, header_name))) {
    if (idx > -1) {
      if (idx < hp->idx) {
        return hp->array[idx];
      } else {
        return nullptr;
      }
    }

    return hp->value;
  } else if (header_name && !strcmp(header_name, "_body")) {
    return event->body;
  }

  return nullptr;
}

ESL_DECLARE(char *) esl_event_get_body(esl_event_t *event) {
  return (event ? event->body : nullptr);
}

ESL_DECLARE(esl_status_t)
esl_event_del_header_val(esl_event_t *event, const char *header_name,
                         const char *val) {
  esl_event_header_t *hp, *lp = nullptr, *tp;
  esl_status_t status = (esl_status_t) false;
  int x = 0;
  esl_ssize_t hlen = -1;
  unsigned long hash = 0;

  if (event == nullptr || header_name == nullptr) {
    return ESL_FAIL;
  }

  tp = event->headers;
  while (tp) {
    hp = tp;
    tp = tp->next;

    x++;
    esl_assert(x < 1000000);
    hash = esl_ci_hashfunc_default(header_name, &hlen);

    if ((!hp->hash || hash == hp->hash) &&
        (hp->name && !strcasecmp(header_name, hp->name)) &&
        (esl_strlen_zero(val) || (hp->value && !strcmp(hp->value, val)))) {
      if (lp) {
        lp->next = hp->next;
      } else {
        event->headers = hp->next;
      }
      if (hp == event->last_header || !hp->next) {
        event->last_header = lp;
      }

      free_header(&hp);
      status = ESL_SUCCESS;
    } else {
      lp = hp;
    }
  }

  return status;
}

static esl_event_header_t *new_header(const char *header_name) {
  esl_event_header_t *header;

#ifdef ESL_EVENT_RECYCLE
  void *pop;
  if (esl_queue_trypop(EVENT_HEADER_RECYCLE_QUEUE, &pop) == ESL_SUCCESS) {
    header = (esl_event_header_t *)pop;
  } else {
#endif
    header = ALLOC(sizeof(*header));
#ifdef ESL_EVENT_RECYCLE
  }
#endif

  if (header == nullptr) {
    return nullptr;
  }
  memset(header, 0, sizeof(*header));
  header->name = DUP(header_name);
  if (header->name == nullptr) {
    FREE(header);
    return nullptr;
  }

  return header;
}

static void free_header(esl_event_header_t **header) {
  assert(header);

  if (*header) {
    FREE((*header)->name);

    if ((*header)->idx) {
      int i = 0;

      for (i = 0; i < (*header)->idx; i++) {
        FREE((*header)->array[i]);
      }
      FREE((*header)->array);
    }

    FREE((*header)->value);

#ifdef ESL_EVENT_RECYCLE
    if (esl_queue_trypush(EVENT_HEADER_RECYCLE_QUEUE, *header) != ESL_SUCCESS) {
      FREE(*header);
    }
#else
    FREE(*header);
#endif
  }
}

ESL_DECLARE(int)
esl_event_add_array(esl_event_t *event, const char *var, const char *val) {
  char *data;
  char **array;
  size_t max = 0;
  unsigned int count = 0;
  const char *p;
  unsigned int i;

  if (event == nullptr || var == nullptr || val == nullptr ||
      strlen(val) < 8) {
    return -1;
  }

  p = val + 7;

  max = 1;

  while ((p = strstr(p, "|:"))) {
    if (max == UINT_MAX) {
      return -1;
    }
    max++;
    p += 2;
  }

  data = strdup(val + 7);
  if (data == nullptr) {
    return -1;
  }

  array = calloc((size_t)max, sizeof(char *));
  if (array == nullptr) {
    free(data);
    return -1;
  }

  count = esl_separate_string_string(data, "|:", array, (unsigned int)max);

  for (i = 0; i < count; i++) {
    if (esl_event_add_header_string(event, ESL_STACK_PUSH, var, array[i]) !=
        ESL_SUCCESS) {
      free(array);
      free(data);
      return -1;
    }
  }

  free(array);
  free(data);

  return 0;
}

static esl_status_t esl_event_base_add_header(esl_event_t *event,
                                              esl_stack_t stack,
                                              const char *header_name,
                                              char *data) {
  esl_event_header_t *header = nullptr;
  esl_ssize_t hlen = -1;
  int exists = 0, fly = 0;
  char *index_ptr;
  int index = 0;
  char *real_header_name = nullptr;
  esl_event_header_t *owned_new_header = nullptr;

  if (event == nullptr || header_name == nullptr || data == nullptr) {
    FREE(data);
    return ESL_FAIL;
  }

  if (!strcmp(header_name, "_body")) {
    const esl_status_t body_status = esl_event_set_body(event, data);
    FREE(data);
    return body_status;
  }

  if ((index_ptr = strchr(header_name, '['))) {
    index_ptr++;
    if (!esl_parse_event_header_index(index_ptr, &index)) {
      FREE(data);
      return ESL_FAIL;
    }
    real_header_name = DUP(header_name);
    if (real_header_name == nullptr) {
      FREE(data);
      return ESL_FAIL;
    }
    if ((index_ptr = strchr(real_header_name, '['))) {
      *index_ptr++ = '\0';
    }
    header_name = real_header_name;
  }

  if (index_ptr || (stack & ESL_STACK_PUSH) || (stack & ESL_STACK_UNSHIFT)) {
    esl_event_header_t *tmp_header = nullptr;

    if (!(header = esl_event_get_header_ptr(event, header_name)) && index_ptr) {

      tmp_header = header = new_header(header_name);
      if (header == nullptr) {
        goto fail;
      }
      owned_new_header = tmp_header;

      if (esl_test_flag(event, ESL_EF_UNIQ_HEADERS)) {
        esl_event_del_header(event, header_name);
      }

      fly++;
    }

    if (header || (header = esl_event_get_header_ptr(event, header_name))) {

      if (index_ptr) {
        if (index > -1 && index <= ESL_EVENT_HEADER_INDEX_MAX) {
          if (index < header->idx) {
            char *replacement = DUP(data);
            if (replacement == nullptr) {
              goto fail;
            }
            FREE(header->array[index]);
            header->array[index] = replacement;
          } else {
            int i;
            char **m;
            const auto target_index = (size_t)index + 1;

            if (target_index > (SIZE_MAX / sizeof(char *))) {
              goto fail;
            }
            m = realloc(header->array, sizeof(char *) * target_index);
            if (m == nullptr) {
              goto fail;
            }
            header->array = m;
            for (i = header->idx; i < index; i++) {
              m[i] = DUP("");
              if (m[i] == nullptr) {
                int j;
                for (j = header->idx; j < i; j++) {
                  FREE(m[j]);
                }
                goto fail;
              }
            }
            m[index] = DUP(data);
            if (m[index] == nullptr) {
              int j;
              for (j = header->idx; j < index; j++) {
                FREE(m[j]);
              }
              goto fail;
            }
            header->idx = index + 1;
            if (!fly) {
              exists = 1;
            }

            FREE(data);
            data = nullptr;
            goto redraw;
          }
        } else if (tmp_header) {
          free_header(&tmp_header);
          owned_new_header = nullptr;
        }

        FREE(data);
        data = nullptr;
        goto end;
      } else {
        if ((stack & ESL_STACK_PUSH) || (stack & ESL_STACK_UNSHIFT)) {
          exists++;
          stack &= ~(ESL_STACK_TOP | ESL_STACK_BOTTOM);
        } else {
          header = nullptr;
        }
      }
    }
  }

  if (!header) {

    if (esl_strlen_zero(data)) {
      esl_event_del_header(event, header_name);
      FREE(data);
      goto end;
    }

    if (esl_test_flag(event, ESL_EF_UNIQ_HEADERS)) {
      esl_event_del_header(event, header_name);
    }

    if (strstr(data, "ARRAY::")) {
      if (esl_event_add_array(event, header_name, data) != 0) {
        goto fail;
      }
      FREE(data);
      data = nullptr;
      goto end;
    }

    header = new_header(header_name);
    if (header == nullptr) {
      goto fail;
    }
    owned_new_header = header;
  }

  if ((stack & ESL_STACK_PUSH) || (stack & ESL_STACK_UNSHIFT)) {
    char **m = nullptr;
    esl_size_t len = 0;
    char *hv;
    int i = 0, j = 0;

    if (header->value && !header->idx) {
      m = calloc(1, sizeof(char *));
      if (m == nullptr) {
        goto fail;
      }
      m[0] = header->value;
      header->value = nullptr;
      header->array = m;
      header->idx++;
    }

    if (header->idx == INT_MAX) {
      goto fail;
    }
    i = header->idx + 1;
    if ((size_t)i > (SIZE_MAX / sizeof(char *))) {
      goto fail;
    }
    m = realloc(header->array, sizeof(char *) * (size_t)i);
    if (m == nullptr) {
      goto fail;
    }

    if ((stack & ESL_STACK_PUSH)) {
      m[header->idx] = data;
      data = nullptr;
    } else if ((stack & ESL_STACK_UNSHIFT)) {
      for (j = header->idx; j > 0; j--) {
        m[j] = m[j - 1];
      }
      m[0] = data;
      data = nullptr;
    }

    header->idx++;
    header->array = m;

  redraw:
    len = 0;
    for (j = 0; j < header->idx; j++) {
      if (header->array[j] == nullptr) {
        goto fail;
      }
      const auto value_len = strlen(header->array[j]);
      if (value_len > (SIZE_MAX - len - 2)) {
        goto fail;
      }
      len += value_len + 2;
    }

    if (len) {
      if (len > (SIZE_MAX - 8)) {
        goto fail;
      }
      len += 8;
      hv = realloc(header->value, len);
      if (hv == nullptr) {
        goto fail;
      }
      header->value = hv;

      if (header->idx > 1) {
        esl_snprintf(header->value, len, "ARRAY::");
      } else {
        *header->value = '\0';
      }

      for (j = 0; j < header->idx; j++) {
        esl_snprintf(header->value + strlen(header->value),
                     len - strlen(header->value), "%s%s",
                     j == 0 ? "" : "|:", header->array[j]);
      }
    }

  } else {
    header->value = data;
    data = nullptr;
  }

  if (!exists) {
    header->hash = esl_ci_hashfunc_default(header->name, &hlen);

    if ((stack & ESL_STACK_TOP)) {
      header->next = event->headers;
      event->headers = header;
      if (!event->last_header) {
        event->last_header = header;
      }
    } else {
      if (event->last_header) {
        event->last_header->next = header;
      } else {
        event->headers = header;
        header->next = nullptr;
      }
      event->last_header = header;
    }
    owned_new_header = nullptr;
  }

end:

  esl_safe_free(real_header_name);

  return ESL_SUCCESS;

fail:
  if (owned_new_header != nullptr) {
    free_header(&owned_new_header);
  }
  FREE(data);
  esl_safe_free(real_header_name);
  return ESL_FAIL;
}

ESL_DECLARE(esl_status_t)
esl_event_add_header(esl_event_t *event, esl_stack_t stack,
                     const char *header_name, const char *fmt, ...) {
  int ret = 0;
  char *data;
  va_list ap;

  va_start(ap, fmt);
  ret = esl_vasprintf(&data, fmt, ap);
  va_end(ap);

  if (ret == -1) {
    return ESL_FAIL;
  }

  return esl_event_base_add_header(event, stack, header_name, data);
}

ESL_DECLARE(esl_status_t)
esl_event_add_header_string(esl_event_t *event, esl_stack_t stack,
                            const char *header_name, const char *data) {
  if (data) {
    char *copy = DUP(data);
    if (copy == nullptr) {
      return ESL_FAIL;
    }
    return esl_event_base_add_header(event, stack, header_name, copy);
  }
  return ESL_FAIL;
}

ESL_DECLARE(esl_status_t)
esl_event_set_body(esl_event_t *event, const char *body) {
  size_t body_len = 0;

  if (event == nullptr) {
    return ESL_FAIL;
  }

  if (body != nullptr &&
      !esl_string_len_within_limit(body, ESL_EVENT_MAX_BODY_LENGTH,
                                   &body_len)) {
    return ESL_FAIL;
  }

  esl_safe_free(event->body);

  if (body) {
    event->body = DUP(body);
    if (event->body == nullptr) {
      return ESL_FAIL;
    }
  }

  return ESL_SUCCESS;
}

ESL_DECLARE(esl_status_t)
esl_event_add_body(esl_event_t *event, const char *fmt, ...) {
  int ret = 0;
  char *data = nullptr;
  size_t data_len = 0;

  va_list ap;
  if (event == nullptr || fmt == nullptr) {
    return ESL_FAIL;
  }

  va_start(ap, fmt);
  ret = esl_vasprintf(&data, fmt, ap);
  va_end(ap);

  if (ret < 0 || data == nullptr) {
    return ESL_FAIL;
  }
  if (!esl_string_len_within_limit(data, ESL_EVENT_MAX_BODY_LENGTH, &data_len)) {
    free(data);
    return ESL_FAIL;
  }

  esl_safe_free(event->body);
  event->body = data;
  return ESL_SUCCESS;
}

ESL_DECLARE(void) esl_event_destroy(esl_event_t **event) {
  esl_event_t *ep = nullptr;
  esl_event_header_t *hp, *this;

  if (event == nullptr) {
    return;
  }

  ep = *event;

  if (ep) {
    for (hp = ep->headers; hp;) {
      this = hp;
      hp = hp->next;
      free_header(&this);
    }
    FREE(ep->body);
    FREE(ep->subclass_name);
#ifdef ESL_EVENT_RECYCLE
    if (esl_queue_trypush(EVENT_RECYCLE_QUEUE, ep) != ESL_SUCCESS) {
      FREE(ep);
    }
#else
    FREE(ep);
#endif
  }
  *event = nullptr;
}

ESL_DECLARE(void) esl_event_merge(esl_event_t *event, esl_event_t *tomerge) {
  esl_event_header_t *hp;

  esl_assert(tomerge && event);

  for (hp = tomerge->headers; hp; hp = hp->next) {
    if (hp->idx) {
      int i;

      for (i = 0; i < hp->idx; i++) {
        esl_event_add_header_string(event, ESL_STACK_PUSH, hp->name,
                                    hp->array[i]);
      }
    } else {
      esl_event_add_header_string(event, ESL_STACK_BOTTOM, hp->name, hp->value);
    }
  }
}

ESL_DECLARE(esl_status_t)
esl_event_dup(esl_event_t **event, esl_event_t *todup) {
  esl_event_header_t *hp;

  if (esl_event_create_subclass(event, ESL_EVENT_CLONE, todup->subclass_name) !=
      ESL_SUCCESS) {
    return ESL_GENERR;
  }

  (*event)->event_id = todup->event_id;
  (*event)->event_user_data = todup->event_user_data;
  (*event)->bind_user_data = todup->bind_user_data;
  (*event)->flags = todup->flags;
  for (hp = todup->headers; hp; hp = hp->next) {
    if (todup->subclass_name && !strcmp(hp->name, "Event-Subclass")) {
      continue;
    }

    if (hp->idx) {
      int i;
      for (i = 0; i < hp->idx; i++) {
        if (esl_event_add_header_string(*event, ESL_STACK_PUSH, hp->name,
                                        hp->array[i]) != ESL_SUCCESS) {
          esl_event_destroy(event);
          return ESL_FAIL;
        }
      }
    } else {
      if (esl_event_add_header_string(*event, ESL_STACK_BOTTOM, hp->name,
                                      hp->value) != ESL_SUCCESS) {
        esl_event_destroy(event);
        return ESL_FAIL;
      }
    }
  }

  if (todup->body) {
    (*event)->body = DUP(todup->body);
    if ((*event)->body == nullptr) {
      esl_event_destroy(event);
      return ESL_FAIL;
    }
  }

  (*event)->key = todup->key;

  return ESL_SUCCESS;
}

ESL_DECLARE(esl_status_t)
esl_event_serialize(esl_event_t *event, char **str, bool encode) {
  esl_size_t len = 0;
  esl_event_header_t *hp;
  esl_size_t llen = 0, dlen = 0, blocksize = 512, encode_len = 1536,
             new_len = 0;
  char *buf = nullptr;
  char *encode_buf = nullptr; /* used for url encoding of variables to make sure
                              unsafe things stay out of the serialized copy */

  if (event == nullptr || str == nullptr) {
    return ESL_FAIL;
  }
  *str = nullptr;

  dlen = blocksize * 2;

  if ((buf = calloc(dlen, sizeof(char))) == nullptr) {
    goto fail;
  }

  /* go ahead and give ourselves some space to work with, should save a few
   * reallocs */
  if ((encode_buf = calloc(encode_len, sizeof(char))) == nullptr) {
    goto fail;
  }

  /* esl_log_printf(ESL_CHANNEL_LOG, ESL_LOG_INFO, "hit serialized!.\n"); */
  for (hp = event->headers; hp; hp = hp->next) {
    /*
     * grab enough memory to store 3x the string (url encode takes one char and
     * turns it into %XX) so we could end up with a string that is 3 times the
     * originals length, unlikely but rather be safe than destroy the string,
     * also add one for the null.  And try to be smart about using the memory,
     * allocate and only reallocate if we need more.  This avoids an alloc, free
     * CPU destroying loop.
     */

    if (hp->idx) {
      int i;
      new_len = 0;
      for (i = 0; i < hp->idx; i++) {
        if (hp->array[i] == nullptr) {
          goto fail;
        }
        const auto value_len = strlen(hp->array[i]);
        if (value_len > ((SIZE_MAX - 1 - new_len) / 3)) {
          goto fail;
        }
        new_len += (value_len * 3) + 1;
      }
    } else {
      if (hp->value == nullptr) {
        goto fail;
      }
      const auto value_len = strlen(hp->value);
      if (value_len > ((SIZE_MAX - 1) / 3)) {
        goto fail;
      }
      new_len = (value_len * 3) + 1;
    }

    if (encode_len < new_len) {
      char *tmp;

      /* keep track of the size of our allocation */
      encode_len = new_len;

      if ((tmp = realloc(encode_buf, encode_len)) == nullptr) {
        goto fail;
      }

      encode_buf = tmp;
    }

    /* handle any bad things in the string like newlines : etc that screw up the
     * serialized format */

    if (hp->value == nullptr) {
      goto fail;
    }

    if (encode) {
      esl_url_encode(hp->value, encode_buf, encode_len);
    } else {
      esl_snprintf(encode_buf, encode_len, "%s", hp->value);
    }

    if (hp->name == nullptr) {
      goto fail;
    }
    if (strlen(hp->name) > (SIZE_MAX - strlen(encode_buf) - 8)) {
      goto fail;
    }
    llen = strlen(hp->name) + strlen(encode_buf) + 8;

    if ((len + llen) > dlen) {
      char *m;
      if (len > (SIZE_MAX - llen)) {
        goto fail;
      }
      if (blocksize > (SIZE_MAX - (len + llen))) {
        goto fail;
      }
      if (dlen > (SIZE_MAX - (blocksize + (len + llen)))) {
        goto fail;
      }
      dlen += (blocksize + (len + llen));
      if ((m = realloc(buf, dlen))) {
        buf = m;
      } else {
        goto fail;
      }
    }

    esl_snprintf(buf + len, dlen - len, "%s: %s\n", hp->name,
                 *encode_buf == '\0' ? "_undef_" : encode_buf);
    len = strlen(buf);
  }

  /* we are done with the memory we used for encoding, give it back */
  esl_safe_free(encode_buf);

  if (event->body) {
    const size_t blen = strlen(event->body);
    llen = blen;

    if (blen) {
      llen += 25;
    } else {
      llen += 5;
    }

    if ((len + llen) > dlen) {
      char *m;
      if (len > (SIZE_MAX - llen)) {
        goto fail;
      }
      if (blocksize > (SIZE_MAX - (len + llen))) {
        goto fail;
      }
      if (dlen > (SIZE_MAX - (blocksize + (len + llen)))) {
        goto fail;
      }
      dlen += (blocksize + (len + llen));
      if ((m = realloc(buf, dlen))) {
        buf = m;
      } else {
        goto fail;
      }
    }

    if (blen) {
      esl_snprintf(buf + len, dlen - len, "Content-Length: %zu\n\n%s", blen,
                   event->body);
    } else {
      esl_snprintf(buf + len, dlen - len, "\n");
    }
  } else {
    esl_snprintf(buf + len, dlen - len, "\n");
  }

  *str = buf;

  return ESL_SUCCESS;

fail:
  FREE(encode_buf);
  FREE(buf);
  *str = nullptr;
  return ESL_FAIL;
}

ESL_DECLARE(esl_status_t)
esl_event_create_json(esl_event_t **event, const char *json) {
  if (event == nullptr || json == nullptr) {
    return ESL_FAIL;
  }

  *event = nullptr;

  if (!esl_string_len_within_limit(json, ESL_EVENT_JSON_MAX_LENGTH, nullptr)) {
    return ESL_FAIL;
  }

  cJSON *cj = cJSON_Parse(json);
  if (cj == nullptr) {
    return (esl_status_t) false;
  }

  JSON_Object *root = cjson_get_object(cj);
  if (root == nullptr) {
    cJSON_Delete(cj);
    return (esl_status_t) false;
  }

  esl_event_t *new_event = nullptr;
  if (esl_event_create(&new_event, ESL_EVENT_CLONE) != ESL_SUCCESS) {
    cJSON_Delete(cj);
    return (esl_status_t) false;
  }

  const size_t count = json_object_get_count(root);
  if (count > ESL_EVENT_JSON_MAX_HEADERS) {
    esl_event_destroy(&new_event);
    cJSON_Delete(cj);
    return ESL_FAIL;
  }

  for (size_t i = 0; i < count; i++) {
    const char *name = json_object_get_name(root, i);
    JSON_Value *value = json_object_get_value_at(root, i);

    if (name == nullptr || value == nullptr) {
      continue;
    }
    if (!esl_string_len_within_limit(name, ESL_EVENT_JSON_MAX_HEADER_NAME_LENGTH,
                                     nullptr)) {
      esl_event_destroy(&new_event);
      cJSON_Delete(cj);
      return ESL_FAIL;
    }

    switch (json_value_get_type(value)) {
    case JSONString: {
      const char *text = json_value_get_string(value);
      if (text == nullptr) {
        break;
      }
      if (!esl_string_len_within_limit(text, ESL_EVENT_JSON_MAX_LENGTH,
                                       nullptr)) {
        esl_event_destroy(&new_event);
        cJSON_Delete(cj);
        return ESL_FAIL;
      }

      if (!strcasecmp(name, "_body")) {
        if (esl_event_set_body(new_event, text) != ESL_SUCCESS) {
          esl_event_destroy(&new_event);
          cJSON_Delete(cj);
          return ESL_FAIL;
        }
      } else {
        if (!strcasecmp(name, "event-name")) {
          (void)esl_event_del_header(new_event, "event-name");
          if (esl_name_event(text, &new_event->event_id) != ESL_SUCCESS) {
            esl_event_destroy(&new_event);
            cJSON_Delete(cj);
            return ESL_FAIL;
          }
        }

        if (esl_event_add_header_string(new_event, ESL_STACK_BOTTOM, name,
                                        text) != ESL_SUCCESS) {
          esl_event_destroy(&new_event);
          cJSON_Delete(cj);
          return ESL_FAIL;
        }
      }
      break;
    }
    case JSONArray: {
      JSON_Array *array = json_value_get_array(value);
      if (array == nullptr) {
        break;
      }

      const size_t array_size = json_array_get_count(array);
      if (array_size > ESL_EVENT_JSON_MAX_ARRAY_ITEMS) {
        esl_event_destroy(&new_event);
        cJSON_Delete(cj);
        return ESL_FAIL;
      }
      for (size_t j = 0; j < array_size; j++) {
        const char *element = json_array_get_string(array, j);
        if (element == nullptr ||
            !esl_string_len_within_limit(element, ESL_EVENT_JSON_MAX_LENGTH,
                                         nullptr)) {
          esl_event_destroy(&new_event);
          cJSON_Delete(cj);
          return ESL_FAIL;
        }
        if (esl_event_add_header_string(new_event, ESL_STACK_PUSH, name,
                                        element) != ESL_SUCCESS) {
          esl_event_destroy(&new_event);
          cJSON_Delete(cj);
          return ESL_FAIL;
        }
      }
      break;
    }
    default:
      break;
    }
  }

  cJSON_Delete(cj);
  *event = new_event;
  return ESL_SUCCESS;
}

ESL_DECLARE(esl_status_t)
esl_event_serialize_json(esl_event_t *event, char **str) {
  esl_event_header_t *hp;
  cJSON *cj;
  JSON_Object *obj;

  if (event == nullptr || str == nullptr) {
    return ESL_FAIL;
  }

  *str = nullptr;

  cj = cJSON_CreateObject();
  if (cj == nullptr) {
    return ESL_FAIL;
  }
  obj = cjson_get_object(cj);
  if (obj == nullptr) {
    cJSON_Delete(cj);
    return ESL_FAIL;
  }

  for (hp = event->headers; hp; hp = hp->next) {
    if (hp->name == nullptr ||
        !esl_string_len_within_limit(hp->name, ESL_EVENT_JSON_MAX_HEADER_NAME_LENGTH,
                                     nullptr)) {
      goto fail;
    }

    if (hp->idx) {
      JSON_Array *array = nullptr;
      cJSON *array_value = cJSON_CreateArray();
      int i;

      if (array_value == nullptr) {
        goto fail;
      }
      array = cjson_get_array(array_value);
      if (array == nullptr) {
        cJSON_Delete(array_value);
        goto fail;
      }

      for (i = 0; i < hp->idx; i++) {
        if (hp->array[i] == nullptr ||
            !esl_string_len_within_limit(hp->array[i], ESL_EVENT_JSON_MAX_LENGTH,
                                         nullptr) ||
            json_array_append_string(array, hp->array[i]) != JSONSuccess) {
          cJSON_Delete(array_value);
          goto fail;
        }
      }

      if (json_object_set_value(obj, hp->name, array_value) != JSONSuccess) {
        cJSON_Delete(array_value);
        goto fail;
      }
    } else {
      if (hp->value == nullptr ||
          !esl_string_len_within_limit(hp->value, ESL_EVENT_JSON_MAX_LENGTH,
                                       nullptr) ||
          json_object_set_string(obj, hp->name, hp->value) != JSONSuccess) {
        goto fail;
      }
    }
  }

  if (event->body) {
    const auto blen = strlen(event->body);
    char tmp[32];

    if (blen > ESL_EVENT_MAX_BODY_LENGTH) {
      goto fail;
    }

    esl_snprintf(tmp, sizeof(tmp), "%zu", blen);

    if (json_object_set_string(obj, "Content-Length", tmp) != JSONSuccess ||
        json_object_set_string(obj, "_body", event->body) != JSONSuccess) {
      goto fail;
    }
  }

  *str = cJSON_Print(cj);
  cJSON_Delete(cj);

  if (*str == nullptr) {
    return ESL_FAIL;
  }

  return ESL_SUCCESS;

fail:
  cJSON_Delete(cj);
  *str = nullptr;
  return ESL_FAIL;
}

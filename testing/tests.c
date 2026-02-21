#include "esl/esl.h"
#include "esl/esl_buffer.h"
#include "esl/esl_config.h"
#include "esl/esl_event.h"
#include "esl/esl_json.h"
#include "esl/esl_threadmutex.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <unistd.h>

typedef struct {
  _Atomic int value;
} test_thread_signal_t;

static void *test_thread_mark_done([[maybe_unused]] esl_thread_t *thread,
                                   void *data) {
  if (data != nullptr) {
    test_thread_signal_t *signal = (test_thread_signal_t *)data;
    atomic_store_explicit(&signal->value, 1, memory_order_release);
  }

  return nullptr;
}

[[nodiscard]] static bool run_test_url_encode_decode() {
  const char *raw = "A B+C%";
  char encoded[128] = {0};

  if (esl_url_encode(raw, encoded, sizeof(encoded)) == 0) {
    return false;
  }

  if (strcmp(encoded, "A%20B%2BC%25") != 0) {
    return false;
  }

  esl_url_decode(encoded);
  return strcmp(encoded, raw) == 0;
}

[[nodiscard]] static bool run_test_url_encode_truncation() {
  char buf[5] = {0};
  const auto written = esl_url_encode("abcdef", buf, sizeof(buf));

  return written == 4 && strcmp(buf, "abcd") == 0;
}

[[nodiscard]] static bool run_test_url_decode_invalid_sequences() {
  char malformed_percent[] = "%";
  char malformed_hex[] = "%2G";
  char mixed[] = "A%2GB%41";

  if (esl_url_decode(nullptr) != nullptr) {
    return false;
  }

  if (esl_url_decode(malformed_percent) != malformed_percent ||
      strcmp(malformed_percent, "%") != 0) {
    return false;
  }

  if (esl_url_decode(malformed_hex) != malformed_hex ||
      strcmp(malformed_hex, "%2G") != 0) {
    return false;
  }

  if (esl_url_decode(mixed) != mixed || strcmp(mixed, "A%2GBA") != 0) {
    return false;
  }

  return true;
}

[[nodiscard]] static bool run_test_stristr_case_insensitive() {
  const char *found = esl_stristr("bEtA", "AlphaBetaGamma");

  if (found == nullptr || strcmp(found, "BetaGamma") != 0) {
    return false;
  }

  return esl_stristr("delta", "AlphaBetaGamma") == nullptr;
}

[[nodiscard]] static bool run_test_snprintf_bounds() {
  char buf[5] = {0};
  const auto ret = esl_snprintf(buf, sizeof(buf), "%s", "abcdef");

  if (ret != 6) {
    return false;
  }
  if (strcmp(buf, "abcd") != 0) {
    return false;
  }

  return esl_snprintf(nullptr, sizeof(buf), "%s", "x") == -1;
}

[[nodiscard]] static bool run_test_buffer_write_read() {
  esl_buffer_t *buffer = nullptr;
  bool ok = false;

  if (esl_buffer_create(&buffer, 4, 4, 0) != ESL_SUCCESS || buffer == nullptr) {
    goto done;
  }

  if (esl_buffer_write(buffer, "abcdef", 6) != 6) {
    goto done;
  }
  if (esl_buffer_inuse(buffer) != 6 || esl_buffer_len(buffer) < 6) {
    goto done;
  }

  {
    char out[4] = {0};
    if (esl_buffer_read(buffer, out, 3) != 3 || memcmp(out, "abc", 3) != 0) {
      goto done;
    }
  }

  if (esl_buffer_inuse(buffer) != 3) {
    goto done;
  }
  if (esl_buffer_toss(buffer, 2) != 1) {
    goto done;
  }
  if (esl_buffer_inuse(buffer) != 1) {
    goto done;
  }

  esl_buffer_zero(buffer);
  if (esl_buffer_inuse(buffer) != 0) {
    goto done;
  }

  ok = true;

done:
  esl_buffer_destroy(&buffer);
  return ok && buffer == nullptr;
}

[[nodiscard]] static bool run_test_buffer_max_len_enforced() {
  esl_buffer_t *buffer = nullptr;
  bool ok = false;

  if (esl_buffer_create(&buffer, 4, 4, 5) != ESL_SUCCESS || buffer == nullptr) {
    goto done;
  }

  if (esl_buffer_write(buffer, "123456", 6) != 0) {
    goto done;
  }
  if (esl_buffer_write(buffer, "12345", 5) != 5) {
    goto done;
  }
  if (esl_buffer_freespace(buffer) != 0) {
    goto done;
  }
  if (esl_buffer_write(buffer, "6", 1) != 0) {
    goto done;
  }

  ok = true;

done:
  esl_buffer_destroy(&buffer);
  return ok && buffer == nullptr;
}

[[nodiscard]] static bool run_test_buffer_destroy_null_safe() {
  esl_buffer_t *buffer = nullptr;

  esl_buffer_destroy(nullptr);
  esl_buffer_destroy(&buffer);

  return buffer == nullptr;
}

[[nodiscard]] static bool run_test_buffer_seek_packets_and_looping() {
  static constexpr char framed_packets[] = "one\n\ntwo\n\npartial";
  static constexpr char long_packet[] = "12345\n\n";
  esl_buffer_t *buffer = nullptr;
  bool ok = false;
  char out[32] = {0};

  if (esl_buffer_create(&buffer, 2, 2, 0) != ESL_SUCCESS || buffer == nullptr) {
    goto done;
  }

  if (esl_buffer_write(buffer, "abcdef", 6) != 6) {
    goto done;
  }
  if (esl_buffer_seek(buffer, 2) != 2) {
    goto done;
  }
  if (esl_buffer_read(buffer, out, 4) != 4 || memcmp(out, "cdef", 4) != 0) {
    goto done;
  }

  esl_buffer_zero(buffer);
  if (esl_buffer_seek(buffer, 1) != 0) {
    goto done;
  }

  if (esl_buffer_write(buffer, framed_packets, sizeof(framed_packets) - 1) !=
      sizeof(framed_packets) - 1) {
    goto done;
  }
  if (esl_buffer_packet_count(buffer) != 2) {
    goto done;
  }

  memset(out, 0, sizeof(out));
  if (esl_buffer_read_packet(buffer, out, sizeof(out)) == 0 ||
      memcmp(out, "one\n", 4) != 0) {
    goto done;
  }
  if (esl_buffer_packet_count(buffer) != 1) {
    goto done;
  }

  esl_buffer_zero(buffer);
  if (esl_buffer_write(buffer, long_packet, sizeof(long_packet) - 1) !=
      sizeof(long_packet) - 1) {
    goto done;
  }

  memset(out, 0, sizeof(out));
  if (esl_buffer_read_packet(buffer, out, 3) != 3 || memcmp(out, "123", 3) != 0) {
    goto done;
  }

  esl_buffer_zero(buffer);
  if (esl_buffer_write(buffer, "abc", 3) != 3) {
    goto done;
  }
  esl_buffer_set_loops(buffer, 0);
  memset(out, 0, sizeof(out));
  if (esl_buffer_read_loop(buffer, out, 5) != 3 || memcmp(out, "abc", 3) != 0) {
    goto done;
  }

  esl_buffer_zero(buffer);
  if (esl_buffer_write(buffer, "abc", 3) != 3) {
    goto done;
  }
  esl_buffer_set_loops(buffer, 1);
  memset(out, 0, sizeof(out));
  if (esl_buffer_read_loop(buffer, out, 5) != 2 || memcmp(out, "abcab", 5) != 0) {
    goto done;
  }

  if (esl_buffer_read_loop(nullptr, out, 1) != 0) {
    goto done;
  }
  if (esl_buffer_read_loop(buffer, nullptr, 1) != 0) {
    goto done;
  }
  if (esl_buffer_read_loop(buffer, out, 0) != 0) {
    goto done;
  }

  ok = true;

done:
  esl_buffer_destroy(&buffer);
  return ok && buffer == nullptr;
}

[[nodiscard]] static bool run_test_json_helpers() {
  cJSON *root = nullptr;
  cJSON *parsed = nullptr;
  char *serialized = nullptr;
  bool ok = false;

  root = cJSON_CreateObject();
  if (root == nullptr) {
    goto done;
  }

  if (cJSON_AddStringToObject(root, "name", "esl") == nullptr) {
    goto done;
  }
  if (cJSON_AddNumberToObject(root, "version", 1.0) == nullptr) {
    goto done;
  }

  {
    const auto arr = esl_json_add_child_array(root, "items");
    if (arr == nullptr || !cJSON_IsArray(arr)) {
      goto done;
    }
    cJSON_AddItemToArray(arr, cJSON_CreateString("alpha"));
    if (cJSON_GetArraySize(arr) != 1) {
      goto done;
    }
    {
      const auto first = cJSON_GetArrayItem(arr, 0);
      const auto value = cJSON_GetStringValue(first);
      if (value == nullptr || strcmp(value, "alpha") != 0) {
        goto done;
      }
    }
  }

  {
    const auto name = esl_json_object_get_cstr(root, "name");
    if (name == nullptr || strcmp(name, "esl") != 0) {
      goto done;
    }
  }

  serialized = cJSON_PrintUnformatted(root);
  if (serialized == nullptr) {
    goto done;
  }

  parsed = cJSON_Parse(serialized);
  if (parsed == nullptr) {
    goto done;
  }

  {
    const auto name = esl_json_object_get_cstr(parsed, "name");
    if (name == nullptr || strcmp(name, "esl") != 0) {
      goto done;
    }
  }

  ok = true;

done:
  cJSON_Delete(parsed);
  cJSON_free(serialized);
  cJSON_Delete(root);
  return ok;
}

[[nodiscard]] static bool run_test_event_create_add_serialize() {
  esl_event_t *event = nullptr;
  char *wire = nullptr;
  bool ok = false;

  if (esl_event_create_subclass(&event, ESL_EVENT_CUSTOM, "unit::suite") !=
          ESL_SUCCESS ||
      event == nullptr) {
    goto done;
  }

  {
    const auto event_name = esl_event_get_header(event, "Event-Name");
    const auto subclass = esl_event_get_header(event, "Event-Subclass");
    if (event_name == nullptr || strcmp(event_name, "CUSTOM") != 0 ||
        subclass == nullptr || strcmp(subclass, "unit::suite") != 0) {
      goto done;
    }
  }

  if (esl_event_add_header_string(event, ESL_STACK_BOTTOM, "X-Test", "true") !=
      ESL_SUCCESS) {
    goto done;
  }
  if (esl_event_add_body(event, "body-%d", 42) != ESL_SUCCESS) {
    goto done;
  }

  if (esl_event_get_body(event) == nullptr ||
      strcmp(esl_event_get_body(event), "body-42") != 0) {
    goto done;
  }

  if (esl_event_serialize(event, &wire, false) != ESL_SUCCESS || wire == nullptr) {
    goto done;
  }
  if (strstr(wire, "X-Test: true\n") == nullptr) {
    goto done;
  }
  if (strstr(wire, "Content-Length: 7\n\nbody-42") == nullptr) {
    goto done;
  }

  if (esl_event_del_header(event, "X-Test") != ESL_SUCCESS) {
    goto done;
  }
  if (esl_event_get_header(event, "X-Test") != nullptr) {
    goto done;
  }

  ok = true;

done:
  free(wire);
  esl_event_destroy(&event);
  return ok && event == nullptr;
}

[[nodiscard]] static bool run_test_event_json_roundtrip() {
  esl_event_t *event = nullptr;
  esl_event_t *parsed = nullptr;
  char *json = nullptr;
  bool ok = false;

  if (esl_event_create(&event, ESL_EVENT_API) != ESL_SUCCESS || event == nullptr) {
    goto done;
  }
  if (esl_event_add_header_string(event, ESL_STACK_PUSH, "X-List", "first") !=
          ESL_SUCCESS ||
      esl_event_add_header_string(event, ESL_STACK_PUSH, "X-List", "second") !=
          ESL_SUCCESS) {
    goto done;
  }
  if (esl_event_set_body(event, "payload") != ESL_SUCCESS) {
    goto done;
  }

  if (esl_event_serialize_json(event, &json) != ESL_SUCCESS || json == nullptr) {
    goto done;
  }
  if (esl_event_create_json(&parsed, json) != ESL_SUCCESS || parsed == nullptr) {
    goto done;
  }

  {
    const auto event_name = esl_event_get_header(parsed, "Event-Name");
    const auto body = esl_event_get_header(parsed, "_body");
    const auto list_0 = esl_event_get_header_idx(parsed, "X-List", 0);
    const auto list_1 = esl_event_get_header_idx(parsed, "X-List", 1);

    if (event_name == nullptr || strcmp(event_name, "API") != 0 ||
        body == nullptr || strcmp(body, "payload") != 0 || list_0 == nullptr ||
        strcmp(list_0, "first") != 0 || list_1 == nullptr ||
        strcmp(list_1, "second") != 0) {
      goto done;
    }
  }

  ok = true;

done:
  cJSON_free(json);
  esl_event_destroy(&parsed);
  esl_event_destroy(&event);
  return ok;
}

[[nodiscard]] static bool run_test_event_validation_guards() {
  esl_event_t *event = nullptr;
  esl_event_t *dup = nullptr;

  if (esl_event_add_header(nullptr, ESL_STACK_BOTTOM, "X", "%s", "Y") !=
      ESL_FAIL) {
    return false;
  }
  if (esl_event_add_header(nullptr, ESL_STACK_BOTTOM, "X", nullptr) !=
      ESL_FAIL) {
    return false;
  }

  if (esl_event_dup(nullptr, nullptr) != ESL_FAIL) {
    return false;
  }
  if (esl_event_dup(&dup, nullptr) != ESL_FAIL) {
    return false;
  }
  if (dup != nullptr) {
    return false;
  }

  esl_event_merge(nullptr, nullptr);
  esl_event_merge(nullptr, event);

  return true;
}

[[nodiscard]] static bool run_test_event_priority_index_and_body_header() {
  esl_event_t *event = nullptr;
  bool ok = false;

  if (esl_event_set_priority(nullptr, ESL_PRIORITY_HIGH) != ESL_FAIL) {
    return false;
  }
  if (strcmp(esl_priority_name(ESL_PRIORITY_HIGH), "HIGH") != 0 ||
      strcmp(esl_priority_name((esl_priority_t)99), "INVALID") != 0) {
    return false;
  }

  if (esl_event_create(&event, ESL_EVENT_API) != ESL_SUCCESS || event == nullptr) {
    goto done;
  }

  if (esl_event_set_priority(event, ESL_PRIORITY_LOW) != ESL_SUCCESS) {
    goto done;
  }
  {
    const auto priority = esl_event_get_header(event, "priority");
    if (priority == nullptr || strcmp(priority, "LOW") != 0) {
      goto done;
    }
  }

  if (esl_event_add_header_string(event, ESL_STACK_BOTTOM, "X-Indexed[1]",
                                  "beta") != ESL_SUCCESS) {
    goto done;
  }
  if (esl_event_add_header_string(event, ESL_STACK_BOTTOM, "X-Indexed[1]",
                                  "gamma") != ESL_SUCCESS) {
    goto done;
  }
  if (esl_event_add_header_string(event, ESL_STACK_BOTTOM, "X-Indexed[-1]",
                                  "bad") != ESL_FAIL) {
    goto done;
  }
  if (esl_event_add_header_string(event, ESL_STACK_BOTTOM, "X-Indexed[4001]",
                                  "bad") != ESL_FAIL) {
    goto done;
  }

  {
    const auto idx0 = esl_event_get_header_idx(event, "X-Indexed", 0);
    const auto idx1 = esl_event_get_header_idx(event, "X-Indexed", 1);
    if (idx0 == nullptr || idx1 == nullptr || strcmp(idx0, "") != 0 ||
        strcmp(idx1, "gamma") != 0) {
      goto done;
    }
  }

  if (esl_event_add_header_string(event, ESL_STACK_BOTTOM, "_body",
                                  "from-header") != ESL_SUCCESS) {
    goto done;
  }
  if (esl_event_get_body(event) == nullptr ||
      strcmp(esl_event_get_body(event), "from-header") != 0) {
    goto done;
  }
  if (esl_event_get_header(event, "_body") == nullptr ||
      strcmp(esl_event_get_header(event, "_body"), "from-header") != 0) {
    goto done;
  }

  ok = true;

done:
  esl_event_destroy(&event);
  return ok && event == nullptr;
}

[[nodiscard]] static bool run_test_config_file_parse() {
  char path[] = "/tmp/esl_cfg_XXXXXX";
  static const char cfg_text[] =
      "[general]\n"
      "foo => bar\n"
      "baz = qux\n";
  esl_config_t cfg;
  bool opened = false;
  bool saw_foo = false;
  bool saw_baz = false;
  bool ok = false;
  int fd = -1;

  fd = mkstemp(path);
  if (fd < 0) {
    goto done;
  }

  if (write(fd, cfg_text, strlen(cfg_text)) != (ssize_t)strlen(cfg_text)) {
    goto done;
  }

  if (close(fd) != 0) {
    fd = -1;
    goto done;
  }
  fd = -1;

  if (!esl_config_open_file(&cfg, path)) {
    goto done;
  }
  opened = true;

  for (;;) {
    char *var = nullptr;
    char *val = nullptr;
    const auto rc = esl_config_next_pair(&cfg, &var, &val);

    if (rc <= 0) {
      break;
    }

    if (var != nullptr && val != nullptr) {
      if (strcmp(var, "foo") == 0 && strcmp(val, "bar") == 0) {
        saw_foo = true;
      } else if (strcmp(var, "baz") == 0 && strcmp(val, "qux") == 0) {
        saw_baz = true;
      }
    }
  }

  ok = saw_foo && saw_baz;

done:
  if (fd >= 0) {
    close(fd);
  }
  if (opened) {
    esl_config_close_file(&cfg);
  }
  unlink(path);
  return ok;
}

[[nodiscard]] static bool run_test_config_cas_bits() {
  unsigned char bits = 0;
  char ok_pattern[] = "sig:1010";
  char bad_pattern[] = "sig:10A0";

  if (esl_config_get_cas_bits(ok_pattern, &bits) != 0) {
    return false;
  }
  if (bits != 0b1010) {
    return false;
  }

  return esl_config_get_cas_bits(bad_pattern, &bits) == -1;
}

[[nodiscard]] static bool run_test_config_sections_and_syntax_errors() {
  char path[] = "/tmp/esl_cfg_sections_XXXXXX";
  static const char cfg_text[] =
      "[general]\n"
      "[+alpha]\n"
      "foo = bar\n"
      "[+beta]\n"
      "baz = qux\n"
      "__END__\n";
  char invalid_path[] = "/tmp/esl_cfg_invalid_XXXXXX";
  static const char invalid_text[] =
      "[general]\n"
      "invalid_line_without_equals\n"
      "__END__\n";
  esl_config_t cfg;
  char *var = nullptr;
  char *val = nullptr;
  bool opened = false;
  bool invalid_opened = false;
  bool ok = false;
  int fd = -1;
  int invalid_fd = -1;

  fd = mkstemp(path);
  if (fd < 0) {
    goto done;
  }
  if (write(fd, cfg_text, strlen(cfg_text)) != (ssize_t)strlen(cfg_text)) {
    goto done;
  }
  if (close(fd) != 0) {
    fd = -1;
    goto done;
  }
  fd = -1;

  if (!esl_config_open_file(&cfg, path)) {
    goto done;
  }
  opened = true;

  if (esl_config_next_pair(&cfg, &var, &val) != 1 || strcmp(cfg.section, "alpha") != 0 ||
      strcmp(var, "") != 0 || strcmp(val, "") != 0) {
    goto done;
  }

  cfg.lockto = cfg.sectno;
  if (esl_config_next_pair(&cfg, &var, &val) != 1 || strcmp(var, "foo") != 0 ||
      strcmp(val, "bar") != 0) {
    goto done;
  }
  if (esl_config_next_pair(&cfg, &var, &val) != 0) {
    goto done;
  }

  esl_config_close_file(&cfg);
  opened = false;

  invalid_fd = mkstemp(invalid_path);
  if (invalid_fd < 0) {
    goto done;
  }
  if (write(invalid_fd, invalid_text, strlen(invalid_text)) !=
      (ssize_t)strlen(invalid_text)) {
    goto done;
  }
  if (close(invalid_fd) != 0) {
    invalid_fd = -1;
    goto done;
  }
  invalid_fd = -1;

  if (!esl_config_open_file(&cfg, invalid_path)) {
    goto done;
  }
  invalid_opened = true;

  if (esl_config_next_pair(&cfg, &var, &val) != -1) {
    goto done;
  }

  ok = true;

done:
  if (fd >= 0) {
    close(fd);
  }
  if (invalid_fd >= 0) {
    close(invalid_fd);
  }
  if (opened) {
    esl_config_close_file(&cfg);
  }
  if (invalid_opened) {
    esl_config_close_file(&cfg);
  }
  unlink(path);
  unlink(invalid_path);
  return ok;
}

[[nodiscard]] static bool run_test_threadmutex_lifecycle() {
  esl_mutex_t *mutex = nullptr;

  if (esl_mutex_create(&mutex) != ESL_SUCCESS || mutex == nullptr) {
    return false;
  }
  if (esl_mutex_lock(mutex) != ESL_SUCCESS) {
    esl_mutex_destroy(&mutex);
    return false;
  }
  if (esl_mutex_trylock(mutex) != ESL_SUCCESS) {
    esl_mutex_unlock(mutex);
    esl_mutex_destroy(&mutex);
    return false;
  }
  if (esl_mutex_unlock(mutex) != ESL_SUCCESS ||
      esl_mutex_unlock(mutex) != ESL_SUCCESS) {
    esl_mutex_destroy(&mutex);
    return false;
  }
  if (esl_mutex_destroy(&mutex) != ESL_SUCCESS || mutex != nullptr) {
    return false;
  }

  return esl_mutex_destroy(&mutex) == ESL_FAIL;
}

[[nodiscard]] static bool run_test_thread_detached_variants() {
  static constexpr size_t DEFAULT_STACK_SIZE = 240 * 1024;
  test_thread_signal_t signal = {.value = 0};
  int attempts = 500;

  if (esl_thread_create_detached(nullptr, nullptr) != ESL_FAIL) {
    return false;
  }
  if (esl_thread_create_detached_ex(test_thread_mark_done, &signal, 1) !=
      ESL_FAIL) {
    return false;
  }

  esl_thread_override_default_stacksize(0);
  if (esl_thread_create_detached(test_thread_mark_done, &signal) != ESL_SUCCESS) {
    esl_thread_override_default_stacksize(DEFAULT_STACK_SIZE);
    return false;
  }

  while (attempts-- > 0) {
    if (atomic_load_explicit(&signal.value, memory_order_acquire) == 1) {
      esl_thread_override_default_stacksize(DEFAULT_STACK_SIZE);
      return true;
    }
    {
      const struct timespec delay = {.tv_sec = 0, .tv_nsec = 1'000'000};
      nanosleep(&delay, nullptr);
    }
  }

  esl_thread_override_default_stacksize(DEFAULT_STACK_SIZE);
  return false;
}

[[nodiscard]] static bool run_test_separate_string_string() {
  char input[] = "alpha|:beta|:gamma";
  char *parts[4] = {nullptr};
  const auto count = esl_separate_string_string(input, "|:", parts, 4);

  return count == 3 && parts[0] != nullptr && parts[1] != nullptr &&
         parts[2] != nullptr && strcmp(parts[0], "alpha") == 0 &&
         strcmp(parts[1], "beta") == 0 && strcmp(parts[2], "gamma") == 0;
}

[[nodiscard]] static bool run_test_esl_guard_paths_and_wait_sock() {
  esl_handle_t handle = {0};
  char *parts[2] = {nullptr};
  int sockets[2] = {-1, -1};
  char byte = 'x';
  bool ok = false;

  handle.sock = ESL_SOCK_INVALID;

  if (esl_toupper('a') != 'A' || esl_tolower('A') != 'a' ||
      esl_toupper(-2) != -2 || esl_tolower(-2) != -2) {
    return false;
  }

  if (esl_wait_sock(ESL_SOCK_INVALID, 1, ESL_POLL_READ) != ESL_SOCK_INVALID) {
    return false;
  }

  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) {
    return false;
  }

  if ((esl_wait_sock(sockets[0], 50, ESL_POLL_WRITE) & ESL_POLL_WRITE) == 0) {
    goto done;
  }
  if (esl_wait_sock(sockets[0], 1, ESL_POLL_READ) != 0) {
    goto done;
  }
  if (write(sockets[1], &byte, sizeof(byte)) != (ssize_t)sizeof(byte)) {
    goto done;
  }
  if ((esl_wait_sock(sockets[0], 50, ESL_POLL_READ) & ESL_POLL_READ) == 0) {
    goto done;
  }
  if (read(sockets[0], &byte, sizeof(byte)) != (ssize_t)sizeof(byte)) {
    goto done;
  }

  if (esl_attach_handle(nullptr, ESL_SOCK_INVALID, nullptr) != ESL_FAIL) {
    goto done;
  }
  if (esl_sendevent(nullptr, nullptr) != ESL_FAIL) {
    goto done;
  }
  if (esl_execute(nullptr, "app", "arg", "uuid") != ESL_FAIL) {
    goto done;
  }
  if (esl_sendmsg(nullptr, nullptr, nullptr) != ESL_FAIL) {
    goto done;
  }
  if (esl_filter(nullptr, "header", "value") != ESL_FAIL ||
      esl_filter(&handle, "header", "value") != ESL_FAIL) {
    goto done;
  }
  if (esl_events(nullptr, ESL_EVENT_TYPE_PLAIN, "all") != ESL_FAIL ||
      esl_events(&handle, ESL_EVENT_TYPE_JSON, "all") != ESL_FAIL) {
    goto done;
  }
  if (esl_connect_timeout(nullptr, "127.0.0.1", 8021, nullptr, "password", 0) !=
      ESL_FAIL) {
    goto done;
  }
  if (esl_disconnect(nullptr) != ESL_FAIL) {
    goto done;
  }
  if (esl_recv_event(nullptr, 0, nullptr) != ESL_FAIL ||
      esl_recv_event_timed(nullptr, 50, 0, nullptr) != ESL_FAIL) {
    goto done;
  }
  if (esl_send(nullptr, "api status") != ESL_FAIL ||
      esl_send_recv_timed(nullptr, "api status", 50) != ESL_FAIL) {
    goto done;
  }
  {
    char mutable_input[] = "a|b";
    char mutable_empty_delim[] = "a|b";

    if (esl_separate_string_string(nullptr, "|", parts, 2) != 0 ||
        esl_separate_string_string(mutable_input, "|", parts, 0) != 0 ||
        esl_separate_string_string(mutable_empty_delim, "", parts, 2) != 0 ||
        esl_separate_string_string("a|b", "|", nullptr, 2) != 0) {
      goto done;
    }
  }

  esl_global_set_logger(nullptr);
  esl_global_set_default_logger(42);

  ok = true;

done:
  if (sockets[0] >= 0) {
    close(sockets[0]);
  }
  if (sockets[1] >= 0) {
    close(sockets[1]);
  }
  return ok;
}

#define TEST(name)                                                             \
  do {                                                                         \
    printf("Running: %s\n", #name);                                            \
    if (run_test_##name()) {                                                   \
      printf("  ✓ PASSED\n");                                                  \
      tests_passed++;                                                          \
    } else {                                                                   \
      printf("  ✗ FAILED\n");                                                  \
      tests_failed++;                                                          \
    }                                                                          \
  } while (0)

int main() {
  int tests_passed = 0;
  int tests_failed = 0;

  TEST(url_encode_decode);
  TEST(url_encode_truncation);
  TEST(url_decode_invalid_sequences);
  TEST(stristr_case_insensitive);
  TEST(snprintf_bounds);
  TEST(buffer_write_read);
  TEST(buffer_max_len_enforced);
  TEST(buffer_destroy_null_safe);
  TEST(buffer_seek_packets_and_looping);
  TEST(json_helpers);
  TEST(event_create_add_serialize);
  TEST(event_json_roundtrip);
  TEST(event_validation_guards);
  TEST(event_priority_index_and_body_header);
  TEST(config_file_parse);
  TEST(config_cas_bits);
  TEST(config_sections_and_syntax_errors);
  TEST(threadmutex_lifecycle);
  TEST(thread_detached_variants);
  TEST(separate_string_string);
  TEST(esl_guard_paths_and_wait_sock);

  printf("\nResult: %d passed, %d failed\n", tests_passed, tests_failed);
  return tests_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

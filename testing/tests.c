#include "esl/esl.h"
#include "esl/esl_buffer.h"
#include "esl/esl_config.h"
#include "esl/esl_event.h"
#include "esl/esl_json.h"
#include "esl/esl_threadmutex.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

[[nodiscard]] static bool run_test_separate_string_string() {
  char input[] = "alpha|:beta|:gamma";
  char *parts[4] = {nullptr};
  const auto count = esl_separate_string_string(input, "|:", parts, 4);

  return count == 3 && parts[0] != nullptr && parts[1] != nullptr &&
         parts[2] != nullptr && strcmp(parts[0], "alpha") == 0 &&
         strcmp(parts[1], "beta") == 0 && strcmp(parts[2], "gamma") == 0;
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
  TEST(stristr_case_insensitive);
  TEST(snprintf_bounds);
  TEST(buffer_write_read);
  TEST(buffer_max_len_enforced);
  TEST(json_helpers);
  TEST(event_create_add_serialize);
  TEST(event_json_roundtrip);
  TEST(config_file_parse);
  TEST(config_cas_bits);
  TEST(threadmutex_lifecycle);
  TEST(separate_string_string);

  printf("\nResult: %d passed, %d failed\n", tests_passed, tests_failed);
  return tests_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

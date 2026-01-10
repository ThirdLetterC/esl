#include "esl_json.h"

[[nodiscard]] ESL_DECLARE(const char *)
esl_json_object_get_cstr(const cJSON *value, const char *name) {
  if (value == nullptr || name == nullptr) {
    return nullptr;
  }

  const auto object = cjson_get_object_const(value);
  return object != nullptr ? json_object_get_string(object, name) : nullptr;
}

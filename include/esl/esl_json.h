/*
  Copyright (c) 2009 Dave Gamble

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/
#ifndef ESL_JSON__h
#define ESL_JSON__h

#include <assert.h>

#include "esl/parson.h"

#ifndef esl_assert
#define esl_assert(_x) assert(_x)
#endif

#ifndef ESL_DECLARE
#define ESL_DECLARE(type) type
#define ESL_DECLARE_NONSTD(type) type
#define ESL_DECLARE_DATA
#endif

typedef JSON_Value cJSON;

[[nodiscard]] static inline bool cjson_is_type(const cJSON *value,
                                               JSON_Value_Type expected) {
  return value != nullptr && json_value_get_type(value) == expected;
}

[[nodiscard]] static inline JSON_Object *cjson_get_object(cJSON *value) {
  return cjson_is_type(value, JSONObject) ? json_value_get_object(value)
                                          : nullptr;
}

[[nodiscard]] static inline const JSON_Object *
cjson_get_object_const(const cJSON *value) {
  return cjson_is_type(value, JSONObject) ? json_value_get_object(value)
                                          : nullptr;
}

[[nodiscard]] static inline JSON_Array *cjson_get_array(cJSON *value) {
  return cjson_is_type(value, JSONArray) ? json_value_get_array(value)
                                         : nullptr;
}

[[nodiscard]] static inline const JSON_Array *
cjson_get_array_const(const cJSON *value) {
  return cjson_is_type(value, JSONArray) ? json_value_get_array(value)
                                         : nullptr;
}

[[nodiscard]] static inline cJSON *cJSON_Parse(const char *text) {
  return text != nullptr ? json_parse_string(text) : nullptr;
}

[[nodiscard]] static inline cJSON *cJSON_CreateObject() {
  return json_value_init_object();
}

[[nodiscard]] static inline cJSON *cJSON_CreateArray() {
  return json_value_init_array();
}

[[nodiscard]] static inline cJSON *cJSON_CreateString(const char *string) {
  return string != nullptr ? json_value_init_string(string) : nullptr;
}

[[nodiscard]] static inline cJSON *cJSON_CreateNumber(double number) {
  return json_value_init_number(number);
}

[[nodiscard]] static inline cJSON *
cJSON_AddStringToObject(cJSON *object, const char *name, const char *string) {
  const auto obj = cjson_get_object(object);
  if (obj == nullptr || name == nullptr || string == nullptr ||
      json_object_set_string(obj, name, string) != JSONSuccess) {
    return nullptr;
  }
  return json_object_get_value(obj, name);
}

[[nodiscard]] static inline cJSON *cJSON_GetObjectItem(const cJSON *object,
                                                       const char *name) {
  const auto obj = cjson_get_object_const(object);
  return obj != nullptr && name != nullptr ? json_object_get_value(obj, name)
                                           : nullptr;
}

[[nodiscard]] static inline cJSON *
cJSON_AddNumberToObject(cJSON *object, const char *name, double number) {
  const auto obj = cjson_get_object(object);
  if (obj == nullptr || name == nullptr ||
      json_object_set_number(obj, name, number) != JSONSuccess) {
    return nullptr;
  }
  return json_object_get_value(obj, name);
}

[[nodiscard]] static inline cJSON *
cJSON_AddBoolToObject(cJSON *object, const char *name, bool boolean) {
  const auto obj = cjson_get_object(object);
  if (obj == nullptr || name == nullptr ||
      json_object_set_boolean(obj, name, boolean) != JSONSuccess) {
    return nullptr;
  }
  return json_object_get_value(obj, name);
}

[[nodiscard]] static inline cJSON *cJSON_AddNullToObject(cJSON *object,
                                                         const char *name) {
  const auto obj = cjson_get_object(object);
  if (obj == nullptr || name == nullptr ||
      json_object_set_null(obj, name) != JSONSuccess) {
    return nullptr;
  }
  return json_object_get_value(obj, name);
}

static inline cJSON *cjson_add_value_to_object(cJSON *object, const char *name,
                                               cJSON *item) {
  const auto obj = cjson_get_object(object);
  if (obj == nullptr || name == nullptr || item == nullptr) {
    json_value_free(item);
    return nullptr;
  }

  if (json_object_set_value(obj, name, item) != JSONSuccess) {
    json_value_free(item);
    return nullptr;
  }

  return json_object_get_value(obj, name);
}

static inline void cJSON_AddItemToArray(cJSON *array, cJSON *item) {
  auto arr = cjson_get_array(array);
  if (arr == nullptr || item == nullptr) {
    json_value_free(item);
    return;
  }
  if (json_array_append_value(arr, item) != JSONSuccess) {
    json_value_free(item);
  }
}

static inline void cJSON_AddItemToObject(cJSON *object, const char *name,
                                         cJSON *item) {
  (void)cjson_add_value_to_object(object, name, item);
}

[[nodiscard]] static inline int cJSON_GetArraySize(const cJSON *array) {
  const auto arr = cjson_get_array_const(array);
  return arr != nullptr ? (int)json_array_get_count(arr) : 0;
}

[[nodiscard]] static inline cJSON *cJSON_GetArrayItem(const cJSON *array,
                                                      int index) {
  if (index < 0) {
    return nullptr;
  }
  const auto arr = cjson_get_array_const(array);
  return arr != nullptr ? json_array_get_value(arr, (size_t)index) : nullptr;
}

[[nodiscard]] static inline char *cJSON_PrintUnformatted(const cJSON *item) {
  return item != nullptr ? json_serialize_to_string(item) : nullptr;
}

[[nodiscard]] static inline char *cJSON_Print(const cJSON *item) {
  return item != nullptr ? json_serialize_to_string_pretty(item) : nullptr;
}

[[nodiscard]] static inline bool cJSON_IsArray(const cJSON *value) {
  return cjson_is_type(value, JSONArray);
}

[[nodiscard]] static inline bool cJSON_IsObject(const cJSON *value) {
  return cjson_is_type(value, JSONObject);
}

[[nodiscard]] static inline bool cJSON_IsString(const cJSON *value) {
  return cjson_is_type(value, JSONString);
}

[[nodiscard]] static inline const char *
cJSON_GetStringValue(const cJSON *item) {
  return cJSON_IsString(item) ? json_value_get_string(item) : nullptr;
}

static inline void cJSON_free(void *ptr) {
  if (ptr != nullptr) {
    json_free_serialized_string((char *)ptr);
  }
}

static inline void cJSON_Delete(cJSON *item) { json_value_free(item); }

[[nodiscard]] ESL_DECLARE(const char *)
    esl_json_object_get_cstr(const cJSON *value, const char *name);

[[nodiscard]] static inline cJSON *
esl_json_add_child_obj(cJSON *parent, const char *name, cJSON *value) {
  esl_assert(parent != nullptr);
  esl_assert(name != nullptr);

  auto child_value = value != nullptr ? value : json_value_init_object();
  if (child_value == nullptr) {
    return nullptr;
  }

  return cjson_add_value_to_object(parent, name, child_value);
}

[[nodiscard]] static inline cJSON *esl_json_add_child_array(cJSON *parent,
                                                            const char *name) {
  esl_assert(parent != nullptr);
  esl_assert(name != nullptr);

  auto child_value = json_value_init_array();
  if (child_value == nullptr) {
    return nullptr;
  }

  return cjson_add_value_to_object(parent, name, child_value);
}

[[nodiscard]] static inline cJSON *
esl_json_add_child_string(cJSON *parent, const char *name, const char *val) {
  esl_assert(parent != nullptr);
  esl_assert(name != nullptr);

  const auto obj = cjson_get_object(parent);
  if (obj == nullptr || val == nullptr ||
      json_object_set_string(obj, name, val) != JSONSuccess) {
    return nullptr;
  }

  return json_object_get_value(obj, name);
}

#endif

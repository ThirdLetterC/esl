#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "esl/esl.h"
#include "esl/esl_event.h" // required to inspect esl_event_t fields

[[nodiscard]] static const char *env_or_default(const char *name,
                                                const char *fallback) {
  const auto value = getenv(name);

  if (value == nullptr || *value == '\0') {
    return fallback;
  }

  return value;
}

[[nodiscard]] static bool parse_port(const char *text, esl_port_t *out_port) {
  char *endptr = nullptr;
  long parsed = 0;

  if (text == nullptr || out_port == nullptr) {
    return false;
  }

  errno = 0;
  parsed = strtol(text, &endptr, 10);
  if (errno != 0 || endptr == text || endptr == nullptr || *endptr != '\0' ||
      parsed < 0 || parsed > UINT16_MAX) {
    return false;
  }

  *out_port = (esl_port_t)parsed;
  return true;
}

int main(int argc, char **argv) {
  esl_handle_t handle = {0};
  const char *host = "localhost";
  const char *user = nullptr;
  const char *password = nullptr;
  esl_port_t port = 8021;

  if (argc > 1) {
    host = argv[1];
  } else {
    host = env_or_default("ESL_HOST", "localhost");
  }

  if (argc > 2) {
    if (!parse_port(argv[2], &port)) {
      fprintf(stderr, "Invalid port: %s\n", argv[2]);
      return EXIT_FAILURE;
    }
  } else {
    const auto env_port = getenv("ESL_PORT");
    if (env_port != nullptr && *env_port != '\0' &&
        !parse_port(env_port, &port)) {
      fprintf(stderr, "Invalid ESL_PORT: %s\n", env_port);
      return EXIT_FAILURE;
    }
  }

  password = argc > 3 ? argv[3] : getenv("ESL_PASSWORD");
  user = argc > 4 ? argv[4] : getenv("ESL_USER");

  if (password == nullptr || *password == '\0') {
    fprintf(stderr,
            "Usage: %s [host] [port] [password] [user]\n"
            "Or set ESL_PASSWORD, with optional ESL_HOST, ESL_PORT, and "
            "ESL_USER.\n",
            argv[0]);
    return EXIT_FAILURE;
  }

  if (esl_connect(&handle, host, port, user, password) != ESL_SUCCESS) {
    fprintf(stderr, "Failed to connect to ESL at %s:%d\n", host, port);
    return EXIT_FAILURE;
  }

  if (esl_send_recv(&handle, "api status\n\n") != ESL_SUCCESS) {
    fprintf(stderr, "Failed to send command\n");
    (void)esl_disconnect(&handle);
    return EXIT_FAILURE;
  }

  if (handle.last_sr_event != nullptr &&
      handle.last_sr_event->body != nullptr) {
    printf("%s\n", handle.last_sr_event->body);
  } else if (*handle.last_sr_reply != '\0') {
    // this is unlikely to happen with api or bgapi (which is hardcoded above)
    // but prefix but may be true for other commands
    printf("%s\n", handle.last_sr_reply);
  } else {
    fprintf(stderr, "No reply received\n");
  }

  (void)esl_disconnect(&handle);

  return EXIT_SUCCESS;
}

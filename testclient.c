#include <stdio.h>
#include <stdlib.h>

#include "esl.h"

int main(void) {
  esl_handle_t handle = {0};

  if (esl_connect(&handle, "localhost", 8021, nullptr, "ClueCon") !=
      ESL_SUCCESS) {
    fprintf(stderr, "Failed to connect to ESL at localhost:8021\n");
    return EXIT_FAILURE;
  }

  if (esl_send_recv(&handle, "api status\n\n") != ESL_SUCCESS) {
    fprintf(stderr, "Failed to send command\n");
    esl_disconnect(&handle);
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

  esl_disconnect(&handle);

  return EXIT_SUCCESS;
}

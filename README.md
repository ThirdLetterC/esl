# ESL Client Library

Pure C client library for the FreeSWITCH Event Socket Library (ESL). It includes the core socket/event handling code under `src/`, public headers in `src/include/`, and a small sample program in `testclient.c` that connects to a running FreeSWITCH instance and prints the output of `api status`.

## Requirements
- FreeSWITCH with the event socket listener enabled (default: `localhost:8021`, password `ClueCon`) to run the sample client.
- A C23-capable toolchain. The sources use modern C features such as `auto`, `nullptr`, and `[[nodiscard]]`.
- POSIX: `pthread` is used for threading; Windows builds need Winsock.

## Build quickstart
Compile everything (library + sample) with a single command:

```bash
cc -std=c23 -I src/include -I ../parson \
  src/*.c testclient.c -pthread -o testclient
```

Run the example while FreeSWITCH is up:

```bash
./testclient
```

`testclient` connects to `localhost:8021`, sends `api status`, and prints the reply body if present (otherwise the raw reply line).

## API highlights
- `esl_connect` / `esl_connect_timeout` to open authenticated connections.
- `esl_send_recv` for synchronous command/response, or `esl_send` + `esl_recv_event[_timed]` for manual polling.
- `esl_events` and `esl_filter` to subscribe to and scope incoming events.
- `esl_sendevent` / `esl_sendmsg` to push custom events, and `esl_execute` to trigger applications on a channel UUID.
- `esl_json_*` helpers wrap Parson for lightweight JSON parsing/serialization when dealing with `JSON` event payloads.

## Notes
- Public headers live in `src/include/`; keep them on your include path when integrating.
- The sources are licensed under the BSD-style terms found at the top of each file.

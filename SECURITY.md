# Security Model

This repository is a C23 POSIX client library for the FreeSWITCH Event Socket
Library (ESL). It opens TCP connections, authenticates with a FreeSWITCH
instance, parses inbound ESL frames, serializes outbound commands/events, and
includes optional listener helpers plus JSON wrappers around the bundled
Parson parser.

It is not a complete security boundary by itself. It does not provide transport
encryption, peer identity verification, rate limiting, connection quotas, or
application-level authorization.

## Trust boundaries

- All bytes read from a socket are untrusted, including ESL headers,
  `Content-Length`, event bodies, and `text/event-json` payloads.
- Any peer accepted through `esl_listen()` or `esl_listen_threaded()` is
  untrusted until the caller authenticates and authorizes it.
- Caller-supplied strings passed to `esl_send()`, `esl_execute()`,
  `esl_filter()`, `esl_events()`, `esl_sendmsg()`, and `esl_sendevent()` are
  treated as trusted local input for transport purposes only. Their semantics
  are not validated by the library.
- Logger callbacks, listen callbacks, and application code consuming parsed
  events are part of the trusted computing base.

## Attacker model

- A malicious or compromised FreeSWITCH endpoint can send malformed headers,
  oversized bodies, fragmented packets, unexpected event ordering, and invalid
  JSON.
- A remote peer can repeatedly connect to listener helpers to consume threads,
  sockets, and memory.
- A local operator can accidentally expose secrets by enabling verbose logging
  or by using example credentials unchanged.
- An attacker can force allocation failures or denial-of-service conditions by
  driving the code toward its configured size limits.

## Protected assets

- Process memory safety while buffering, parsing, serializing, and destroying
  ESL frames and events.
- Integrity of `esl_handle_t` connection state, queued events, and socket
  lifecycle.
- Availability under malformed input, partial reads/writes, and oversized
  frames within the implemented limits.
- Confidentiality of caller-supplied credentials and event data, subject to the
  limitations below.

## Defensive posture

- The build enforces strict compilation flags in `build.zig`:
  `-std=c23 -Wall -Wextra -Wpedantic -Werror`.
- Optional sanitizer instrumentation is available with
  `zig build -Dsanitize=true`.
- The unit test target `zig build test` exercises buffer bounds, event parsing,
  JSON helpers, socket wait behavior, and mutex lifecycle.
- Socket connection setup uses `poll()`-based timeout handling in
  `esl_connect_timeout()` when a timeout is provided.
- Partial writes are handled explicitly in `esl_send()`, and the library adds
  the terminating blank line if the caller omitted it.
- The inbound packet buffer is capped at `67'108'864` bytes (64 MiB).
- Parsed `Content-Length` is capped at `16'777'216` bytes (16 MiB).
- Plain-text nested event parsing is bounded to:
  - `4'096` headers
  - `65'536` bytes per header line
- JSON parsing and JSON event conversion are bounded to:
  - `16'777'216` bytes per JSON text/value
  - `4'096` top-level object members
  - `4'096` array items per header
  - `1'024` bytes per JSON header name
- Buffer and event allocation paths are checked for `nullptr`, and most dynamic
  allocations use zero-initializing `calloc()`.
- Invalid percent-encoding in `esl_url_decode()` is left unchanged instead of
  decoding partial or malformed sequences.
- Serialization paths in `esl_event_serialize()` and related helpers include
  size checks before allocation growth and fail on overflow-risk calculations.
- Connection teardown frees queued events, packet buffers, and mutex state, and
  marks the handle disconnected on transport failure.

## Limitations and caller responsibilities

- Transport security is not provided. ESL authentication happens over plaintext
  TCP unless the caller places the connection inside an external secure
  channel.
- Peer identity is not verified. The library authenticates to a server, but it
  does not authenticate the server to the client.
- The sample client in `examples/testclient.c` targets
  `localhost:8021` with password `ClueCon`. That is a development example, not
  a production-safe credential choice.
- Verbose logging can expose secrets. If the caller enables the default logger
  or installs a custom logger, outbound commands and inbound event contents may
  include authentication material, command arguments, headers, and bodies.
- Secret zeroization is not implemented. Passwords and event data may remain in
  process memory until overwritten or freed.
- `esl_send()` uses plain `send()`. On platforms where `SIGPIPE` is generated
  for a closed peer, the caller should ignore or handle `SIGPIPE` at the
  process level.
- Thread safety is limited. `esl_handle_t` contains a mutex, but the API should
  not be treated as safe for unsynchronized concurrent access from multiple
  threads. Global logger configuration is also process-global and unsynchronized.
- `esl_listen()` and `esl_listen_threaded()` currently bind to `INADDR_ANY`;
  their `host` parameter is ignored. Treat them as exposed on all interfaces
  unless you restrict reachability externally.
- `esl_listen_threaded()` creates one detached thread per accepted connection
  and does not enforce authentication, rate limits, or concurrency caps beyond
  the caller-supplied backlog.
- Availability protection is bounded, not complete. Size checks reduce memory
  abuse, but a determined peer can still consume CPU time, sockets, and worker
  threads.
- Application-level validation is caller-owned. Parsed headers, bodies, and
  JSON fields are transported safely only within the library's limits; their
  meaning and authorization requirements still belong to the embedding
  application.
- If a stalled peer is unacceptable, prefer timed APIs such as
  `esl_connect_timeout()`, `esl_recv_event_timed()`, and
  `esl_send_recv_timed()` and enforce higher-level watchdogs.

## Build and deployment guidance

- Build with `zig build` and run `zig build test` before release changes.
- Use `zig build -Dsanitize=true` during development and fuzzing.
- Run the library behind a trusted network boundary or a TLS tunnel when
  credentials or sensitive events cross untrusted networks.
- Disable or redact verbose logging in production.
- Do not expose listener helpers directly to the public internet without
  external authentication, network filtering, and connection limits.

# Endpoint protocol

Endpoints are named procedures: structured data in, structured data out. The same call resolves over HTTP on remote apps and as an in-process invocation on local apps; the caller cannot tell which. This document specifies the call contract, the fault envelope, and the transport bindings.

## Calls

A call names a handler and carries a JSON object body. Arguments are declared per handler in the endpoints document (see `src/abi/endpoints.jtd.json`); the runtime constructs the body from the binding's static `body` map and the values of named inputs, coerced to the declared argument types. A call completes with exactly one outcome:

- **ok** — the handler's response body (a UI fragment for `onResponse: "patch"`, otherwise opaque data).
- **fault** — the call itself failed. Faults are machinery, not domain outcomes: a handler that wants to say "that name is taken" returns an ordinary `ok` fragment or data describing it. Faults are for bad arguments, missing handlers, refused or crashed calls, and dead transports.

## Faults

A fault is a JSON object:

```json
{
  "code": "invalid",
  "message": "name must be a string",
  "issues": [ { "path": "name", "code": "missing" } ]
}
```

- **code** (required): one of the closed set below, as a string.
- **message** (optional): human-readable description.
- **issues** (optional): for `invalid`, a list of `{ path, code }` argument issues. `path` addresses the argument; `code` is a short machine token (`missing`, `type`, `unknown`).

### Codes

Transport-class codes are synthesized by the framework and must never be produced by handlers. Application-class codes are produced by handlers and by argument validation (which runs in the runtime before dispatch and again at the server).

| Code            | Class       | HTTP | Meaning |
| --------------- | ----------- | ---- | ------- |
| invalid         | application | 422  | Arguments failed the declared contract. |
| unauthenticated | application | 401  | No valid credentials. |
| denied          | application | 403  | Authenticated but not allowed. |
| missing         | application | 404  | Requested entity does not exist. |
| conflict        | application | 409  | State conflict (concurrent edit, already exists). |
| failed          | application | 500  | Handler crashed or refused for an internal reason. |
| unavailable     | transport   | 503  | Could not reach the handler; safe to retry. |
| timeout         | transport   | 408  | No completion within the deadline; completion unknown. |
| cancelled       | transport   | —    | Superseded or aborted by the caller. |

## HTTP binding

- A call to handler `h` is `POST <prefix>/h` with `Content-Type: application/json` and the body object. The default prefix is `/api`.
- `ok` is a 200 with the raw response body. Success is never enveloped.
- A fault is the fault object as a JSON body, with the mapped HTTP status from the table. Fault bodies are always JSON regardless of what the request asked for.
- **Intermediary rule.** Clients must not trust that a non-200 response was produced by the application: proxies and load balancers generate their own error pages. On a non-200, the client strictly parses the body as a fault envelope (unknown fields rejected, code must be from the closed set). If it conforms, that fault is the outcome. If not, a fault is synthesized from the status: 408 → `timeout`; 429, 502, 503, 504 → `unavailable`; anything else → `failed` with the status in the message.

## In-process binding

The resolver returns the outcome directly: an ok body, or a fault envelope plus the fault discriminant. No statuses exist and none are invented.

## Runtime behavior

- The runtime validates tree wiring against the endpoints document when a tree or fragment is rendered: a binding that names an unknown handler, or patches without a target, is a contract violation and fatal.
- Argument construction failures (missing required input, value not coercible to the declared type) produce a local `invalid` fault through the same path as a remote fault; the call is never sent.
- One call may be outstanding per binding; dispatch while pending is dropped.
- Faults are reported to the host (`report`) and otherwise change nothing. Routing faults into UI state (error bindings over signals) is future work and will layer on this channel.

## ABI

`deliver(token, outcome, ptr, len)` carries the transport's raw result; the runtime owns classification.

| outcome  | meaning |
| -------- | ------- |
| 0        | ok; body follows |
| 1        | fault envelope; body follows |
| 2        | unreachable (no response exists) |
| 3        | timeout |
| 4        | cancelled |
| 100..599 | HTTP status; raw body follows; runtime applies the intermediary rule |

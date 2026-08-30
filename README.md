# zenoh-node

[![npm version](https://img.shields.io/npm/v/zenoh-node.svg)](https://www.npmjs.com/package/zenoh-node)
[![node](https://img.shields.io/node/v/zenoh-node.svg)](https://nodejs.org)
[![prebuilds](https://img.shields.io/badge/prebuilds-linux--x64%20%7C%20linux--arm64-blue)](#installation)
[![license](https://img.shields.io/badge/license-MIT-green.svg)](./LICENSE)

**Native Node.js bindings for [Zenoh](https://zenoh.io) — fast, brokerless, peer-to-peer pub/sub, query, and storage for the backend.**

`zenoh-node` binds Node.js directly to Zenoh's native C stack (`zenoh-c`) through a N-API addon. Your Node process becomes a *first-class peer* on the Zenoh network — not a browser tab tunneling through a router. That single architectural decision is what makes it faster and fundamentally better suited to backend services than the browser-oriented official Zenoh TypeScript library.

> **Node.js + Zenoh, without WebSockets.** Direct native transport. True peer mode. Zero broker required.

---

## Features

- ⚡ **Native N-API binding** to `zenoh-c` — no WebSocket bridge, no `zenohd` middleman
- 🔗 **True peer-to-peer** mode over TCP / UDP / Unix domain sockets
- 📡 **Full Zenoh API** — `put`, `subscribe`, `declareQueryable`, `get`
- 🌲 **Key-expression wildcards** — `fortem/auth/**`
- 🧠 **Low GC pressure** — byte arrays piped straight into V8, not re-parsed from WebSocket frames
- 📦 **Prebuilt binaries** for `linux-x64` and `linux-arm64` — install with no toolchain
- 🌍 **Polyglot interop** with `zenoh-cpp`, `zenoh-python`, `zenoh-java`, `zenoh-rs`, and ROS 2
- 🔒 **TypeScript types** included

---

## Why this exists

The official Eclipse Zenoh TypeScript library is built for the **browser**. To reach that sandbox it routes every packet over a **WebSocket** to an external `zenohd` daemon. That's the right call in a web page — but on the backend, where Node.js is a real OS process with real sockets, the WebSocket layer is pure overhead you never asked for.

`zenoh-node` throws that layer away and speaks the native Zenoh wire protocol directly.

## The advantages

### 1. No WebSocket overhead

The TypeScript library forces every payload through a WebSocket loop: HTTP upgrade headers, per-frame masking keys, frame packaging, and TCP fragmentation on top of it all. A native backend binding communicates directly over Zenoh's low-overhead wire protocol — roughly **5 bytes of framing per message** — skipping the network syscalls and frame processing WebSockets impose. The result is dramatically lower end-to-end latency: your Node app stops being a bottlenecked client and becomes an active participant capable of sub-millisecond exchanges on the local fabric.

### 2. True peer-to-peer routing

A WebSocket-bound node is locked into a **client → broker** topology — it *must* talk through an external `zenohd` router. Binding to native sockets (TCP, UDP, Unix domain sockets) unlocks Zenoh's native **peer mode**: your Node runtime forms direct data pipes with other high-performance services — Rust backends, C-based embedded microcontrollers, ROS 2 nodes — with **no middleman daemon** in the path.

```js
const { Session } = require('zenoh-node')

const peer = new Session()                          // multicast peer discovery
const direct = new Session('tcp/192.168.1.10:7447') // explicit peer locator
```

### 3. CPU and memory efficiency

Over WebSockets, Node burns event-loop cycles parsing frames and allocating buffers for every text/binary transition — churn that feeds the garbage collector. A native transport pipes byte arrays straight into V8 memory over raw sockets, so a single backend node sustains **higher message throughput with fewer GC pauses**, keeping the event loop free for real work. That matters most exactly where the TS library hurts worst: high-frequency telemetry and raw video frames.

---

## zenoh-node vs. the official TypeScript library

| | **zenoh-node** (this package) | **zenoh-ts** (official) |
|---|---|---|
| Target environment | Backend Node.js | Browser |
| Transport | Native Zenoh wire protocol (TCP/UDP/UDS) | WebSocket → `zenohd` |
| Per-message framing | ~5 bytes | HTTP upgrade + WS frame + mask + TCP |
| Topology | Peer **or** client | Client only |
| External router (`zenohd`) required | **No** (peer mode) | **Yes** |
| Direct P2P with Rust / C / ROS 2 peers | ✅ | ❌ (via broker) |
| Binary payloads | Straight into V8 memory | Re-framed per message |
| Install | Prebuilt native addon | Pure JS |

---

## Benchmarks

A runnable benchmark harness is included so you can measure on your own hardware:

```bash
node bench/latency.js       # round-trip request/reply latency
node bench/throughput.js    # sustained put/subscribe message rate
```

Because the transport is native rather than a WebSocket bridged through `zenohd`, the direct path avoids the framing and routing overhead the official browser library adds on every hop.

---

## Installation

```bash
npm install zenoh-node
```

Prebuilt binaries ship for **linux-x64** and **linux-arm64**, so most installs need no toolchain. Building from source requires CMake, a C++ compiler, and a Rust toolchain (`cargo`).

Requires **Node.js ≥ 18** (N-API 8).

## Quick start

```js
const { Session } = require('zenoh-node')
const session = new Session()

// Publish
session.put('fortem/auth/status', 'online')

// Subscribe (wildcards supported)
session.subscribe('fortem/auth/**', (keyexpr, payload) => {
  console.log(`${keyexpr} = ${payload}`)
})

// Serve queries
session.declareQueryable('fortem/config', (keyexpr, params, reply) => {
  reply(JSON.stringify({ ready: true }))
})

// Query the network
session.get('fortem/config', '', (keyexpr, payload) => {
  console.log('reply:', payload)
}, () => console.log('done'))

session.close()
```

## API

| Method | Purpose |
|--------|---------|
| `new Session(locator?)` | Open a peer-mode session. Omit the locator for multicast discovery, or pass `"tcp/host:7447"` / `{ connect, listen }`. |
| `put(keyexpr, payload)` | Publish a value on a key expression. |
| `subscribe(keyexpr, cb)` | Receive every value matching a key expression (wildcards `*` / `**` supported). |
| `declareQueryable(keyexpr, cb)` | Answer `get()` requests from other peers; call `reply()` once per query. |
| `get(keyexpr, params, onReply, onDone?)` | Query matching queryables across the network. |
| `close()` | Release the session and all resources. |

Full type definitions ship in [`index.d.ts`](./index.d.ts).

## Interoperability

Because `zenoh-node` speaks the standard Zenoh wire protocol, it interoperates out of the box with every official Zenoh client over the same key expressions and locators:

- **C++** — [`zenoh-cpp`](https://github.com/eclipse-zenoh/zenoh-cpp)
- **Python** — [`zenoh-python`](https://github.com/eclipse-zenoh/zenoh-python) (`pip install zenoh`)
- **Java / Kotlin** — [`zenoh-java`](https://github.com/eclipse-zenoh/zenoh-java)
- **Rust** — [`zenoh`](https://github.com/eclipse-zenoh/zenoh)
- **ROS 2** — via [`rmw_zenoh`](https://github.com/ros2/rmw_zenoh)

Node becomes just another peer in a polyglot mesh.

## License

[MIT](./LICENSE)

---

<sub>**Keywords:** zenoh, zenoh-node, node zenoh, zenoh nodejs, native zenoh bindings, zenoh napi, zenoh n-api, pub/sub, publish subscribe, peer-to-peer, p2p messaging, brokerless, message bus, IPC, telemetry, robotics, ROS 2, edge computing, low-latency, real-time, zenoh-c, alternative to WebSocket, backend messaging.</sub>

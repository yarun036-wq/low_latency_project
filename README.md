# Low-Latency Limit Order Book

This project is a compact C++20 limit order book and matching engine intended as a low-latency portfolio piece.

## Design

- Preallocated order pool sized at startup
- Intrusive FIFO queues per price level
- O(1) cancel by `order_id`
- Price-time priority matching
- No heap allocation on the hot path once the book and scratch buffers are initialized

## Layout

- `include/lob/order_book.hpp`: public API and core data structures
- `include/lob/latency_histogram.hpp`: log-bucket latency histogram for p50/p99/p999 reporting
- `src/order_book.cpp`: matching engine implementation
- `src/main.cpp`: deterministic demo plus synthetic benchmark harness
- `src/gateway.cpp`: minimal TCP command gateway around the engine
- `tests/order_book_tests.cpp`: reference-model parity checks and order-type scenarios

## Build

```bash
cmake -S . -B build
cmake --build build --config Release
```

Run:

```bash
./build/order_book_bench
./build/order_book_bench 1000000
./build/order_book_tests
./build/order_book_gateway 9090
```

On Windows with Visual Studio generators, the executable is typically under `build/Release/order_book_bench.exe`.

## Supported Order Semantics

- `Limit` and `Market`
- `GTC`, `IOC`, and `FOK`
- Price-time priority for resting orders
- Market and IOC/FOK remainders do not rest on the book

## Gateway Protocol

Connect over TCP and send one command per line:

```text
BUY 101 100500 25 LIMIT GTC
SELL 102 100450 10 LIMIT IOC
CANCEL 101
TOP
STATS
QUIT
```

Response lines are plain text so you can test quickly with `nc`/`telnet` or a custom client.

## Benchmark Output

The benchmark now reports:

- average ns/op
- p50 latency
- p99 latency
- p999 latency
- aggregate submitted/cancelled/traded counters

## What To Improve Next

- Split hot and cold fields in the order node
- Add replay input from captured market/order streams
- Replace the reference test model with randomized long-run fuzzing
- Move gateway parsing to a fixed-buffer parser to reduce allocations

# Low-Latency C++ Trading Projects

This repository contains two compact C++20 low-latency portfolio projects:

- a limit order book and matching engine
- a crypto multi-exchange arbitrage detector with USD-normalized pricing

## Crypto Arbitrage Detector

This module compares the same crypto asset across exchanges in different countries, converts every venue quote into USD, adjusts for fees/slippage, and finds the best buy venue and best sell venue.

### What It Does

- ingests per-exchange top-of-book quotes
- converts EUR/JPY/KRW/USDT-style quotes into USD
- computes effective buy and sell prices after costs
- finds best cross-exchange arbitrage opportunities
- reports low-latency benchmark stats for update-and-detect loops

### Crypto Files

- `include/arb/crypto_arb.hpp`: public arbitrage detector API
- `src/crypto_arb.cpp`: normalization and opportunity engine
- `src/crypto_arb_main.cpp`: demo plus synthetic benchmark
- `tests/crypto_arb_tests.cpp`: normalization and opportunity checks

### Run Crypto Module

```bash
./build/crypto_arb_bench
./build/crypto_arb_bench 1000000
./build/crypto_arb_tests
```

The crypto executable prints opportunities like:

```text
BTC/USD: buy on Japan venue in USD-normalized terms, sell on US venue, spread after fees = ...
```

## Limit Order Book

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
./build/crypto_arb_bench
./build/crypto_arb_tests
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

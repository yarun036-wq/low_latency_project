# Low-Latency Limit Order Book

A compact C++20 matching engine that simulates the core of an electronic exchange. It accepts buy and sell orders, matches them by price-time priority, supports cancel-by-ID, and exposes both a benchmark harness and a simple TCP gateway.

This project is meant to demonstrate low-latency systems work rather than UI or full brokerage integration. The focus is on data structures, memory behavior, predictable matching logic, and measurable latency.

## Why This Project

In a trading system, correctness is only part of the problem. The hot path must also be fast and predictable. This project was built to show:

- low-allocation order handling
- deterministic price-time matching
- efficient cancel lookup
- measurable latency at p50, p99, and p999
- clean separation between matching logic, tests, and external interface

## Core Features

- Buy and sell order handling
- `Limit` and `Market` orders
- `GTC`, `IOC`, and `FOK` time-in-force semantics
- Price-time priority
- O(1)-style order lookup for cancels
- Preallocated order storage
- Benchmark executable
- Reference-model correctness tests
- Minimal TCP text gateway

## System Design

- `include/lob/order_book.hpp`
  Public API and core data structures
- `include/lob/latency_histogram.hpp`
  Lightweight log-bucket latency tracker for tail metrics
- `src/order_book.cpp`
  Matching engine implementation
- `src/main.cpp`
  Demo and synthetic benchmark driver
- `src/gateway.cpp`
  TCP command gateway for interactive testing
- `tests/order_book_tests.cpp`
  Correctness checks against a simple reference model

### Architecture

```text
Client Orders / Test Flow
           |
           v
   +-------------------+
   |   OrderBook API   |
   +-------------------+
           |
           v
   +-------------------+
   | Matching Engine   |
   | price-time logic  |
   +-------------------+
      |            |
      |            +--------------------+
      v                                 v
+-------------+                 +----------------+
| Trade Events |                 | Top of Book    |
| Exec Report  |                 | Book State     |
+-------------+                 +----------------+
      |
      +--------------------+
                           |
                           v
                 +--------------------+
                 | Bench / Tests /    |
                 | TCP Gateway        |
                 +--------------------+
```

### Data Structure Choices

- Preallocated order pool to reduce heap activity on the hot path
- Intrusive FIFO queues per price level to preserve time priority
- Direct order ID lookup for efficient cancel handling

## Build

Cross-platform with CMake:

```bash
cmake -S . -B build
cmake --build build --config Release
```

Windows helper:

```bat
build_windows.bat
```

## Run

```bash
./build/order_book_bench
./build/order_book_bench 1000000
./build/order_book_tests
./build/order_book_gateway 9090
```

On Windows, the executables are generated under `build/`.

## Gateway Commands

Send one line per command:

```text
BUY 101 100500 25 LIMIT GTC
SELL 102 100450 10 LIMIT IOC
CANCEL 101
TOP
STATS
QUIT
```

Responses are plain text so the engine can be exercised quickly via `nc`, `telnet`, or a small custom client.

## Benchmark Output

The benchmark reports:

- average nanoseconds per operation
- `p50` latency
- `p99` latency
- `p999` latency
- submitted, cancelled, executed, and trade counters

### Sample Output

```text
Deterministic demo trades
  incoming=4 resting=1 px=100100 qty=40
  incoming=4 resting=2 px=100100 qty=10
  report accepted=1 executed=50 remaining=0 rested=0
Top of book
  Bid: 99900 qty=60 orders=1
  Ask: 100100 qty=20 orders=1

Benchmark
  iterations: 10000
  avg ns/op:  157.91
  p50 ns:     64
  p99 ns:     512
  p999 ns:    4096
```

## Interview Summary

Short version:

> Built a low-latency C++ limit order book and matching engine with price-time priority, market/limit order support, cancel handling, correctness tests, and tail-latency benchmarking.

Longer version:

> I built a compact exchange-core style system in C++ that processes buy and sell orders, matches them by price-time priority, supports cancels and richer order semantics, and exposes measurable latency through a benchmark harness. The main engineering focus was on predictable hot-path behavior, memory layout, and correctness under realistic matching scenarios.

## GitHub About Suggestions

Suggested repo description:

> Low-latency C++ limit order book and matching engine with price-time priority, cancel handling, tests, and tail-latency benchmarking.

Suggested tags:

`c-plus-plus`, `low-latency`, `matching-engine`, `limit-order-book`, `trading-systems`, `market-microstructure`, `benchmarking`, `cmake`

## Next Improvements

- Split hot and cold fields in the order node
- Add replay input from historical order streams
- Add long-run randomized fuzz/property testing
- Reduce gateway-side parsing allocations

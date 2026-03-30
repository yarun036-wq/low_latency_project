# Low-Latency Pre-Trade Risk Engine

A compact C++20 pre-trade risk engine that validates orders before they reach a matching engine or exchange gateway. It applies fat-finger checks, per-symbol position limits, gross and net exposure caps, and open-order limits, while exposing both a benchmark harness and deterministic correctness tests.

This project is meant to demonstrate the decision path that sits directly in front of execution. The focus is on fast in-memory guardrails, predictable hot-path checks, measurable latency, and a design that feels close to real HFT infrastructure rather than a toy trading bot.

## Why This Project

In a trading stack, orders should be rejected before they create operational or financial risk. This project was built to show:

- constant-time style risk checks on the hot path
- low-allocation in-memory state for users, symbols, and open orders
- realistic trading controls such as fat-finger and position limits
- measurable latency at `p50`, `p99`, and `p999`
- clean separation between risk state, benchmark logic, and tests

## Core Features

- Per-order max quantity validation
- Price-band / fat-finger checks against a reference price
- Per-symbol position limit checks
- Portfolio gross exposure checks
- Portfolio net exposure checks
- Open-order count limits
- Reservation and release of risk on accept, fill, and cancel
- Benchmark executable
- Deterministic unit tests

## System Design

- `include/risk/pre_trade_risk.hpp`
  Public API, order model, risk decisions, and configuration types
- `include/risk/latency_histogram.hpp`
  Lightweight tail-latency histogram helper
- `src/pre_trade_risk.cpp`
  Risk engine implementation
- `src/pre_trade_risk_main.cpp`
  Demo and synthetic benchmark driver
- `tests/pre_trade_risk_tests.cpp`
  Deterministic correctness tests

### Architecture

```text
Incoming Orders
      |
      v
+----------------------+
|  Pre-Trade Risk API  |
+----------------------+
      |
      v
+----------------------+
| Risk Checks          |
| qty / price band     |
| position / exposure  |
| open-order caps      |
+----------------------+
      |
      +--------------------------+
      |                          |
      v                          v
+------------------+    +------------------+
| Accept + Reserve |    | Reject + Reason  |
+------------------+    +------------------+
      |
      v
+---------------------------+
| Fill / Cancel / Release   |
+---------------------------+
      |
      v
+---------------------------+
| Bench / Tests / Metrics   |
+---------------------------+
```

### Data Structure Choices

- Pre-sized hash tables for users, symbols, and live orders
- Compact order state to reserve and release exposure cheaply
- User-symbol keyed position map to avoid scanning orders during checks

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
./build/pre_trade_risk_bench
./build/pre_trade_risk_bench 1000000
./build/pre_trade_risk_tests
```

## Benchmark Output

The benchmark reports:

- average nanoseconds per operation
- `p50` latency
- `p99` latency
- `p999` latency
- accepted and rejected order counts
- live reserved order count

### Sample Output

```text
Deterministic risk demo
  order=1 accepted=1 projected_position=500 gross=50075000 net=50075000
  order=2 accepted=0 reason=PriceBandExceeded
  order=3 accepted=1 projected_position=-350
  position user=101 symbol=1 after fill=300

Benchmark
  iterations: 10000
  avg ns/op:  192.64
  p50 ns:     128
  p99 ns:     512
  p999 ns:    1024
```

## Interview Summary

Short version:

> Built a low-latency C++ pre-trade risk engine that validates incoming orders against fat-finger, position, exposure, and open-order limits before execution.

Longer version:

> I built a compact HFT-style pre-trade risk engine in C++ that sits in front of execution, checks orders against portfolio and symbol-level constraints, reserves risk state on accept, and releases it on fills and cancels. The main engineering focus was predictable hot-path checks, in-memory state management, and measurable latency.

## GitHub About Suggestions

Suggested repo description:

> Low-latency C++ pre-trade risk engine with position, exposure, and fat-finger checks plus benchmarking and tests.

Suggested tags:

`c-plus-plus`, `low-latency`, `risk-engine`, `hft`, `trading-systems`, `pre-trade-risk`, `benchmarking`, `cmake`

## Next Improvements

- Add per-strategy and per-venue limits
- Add SIMD-friendly batched risk checks
- Add lock-free handoff between network and risk threads
- Add persistent replay input from historical order streams

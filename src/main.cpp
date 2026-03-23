#include "lob/order_book.hpp"
#include "lob/latency_histogram.hpp"

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

void print_top(const lob::TopOfBook& top) {
  std::cout << "Top of book\n";

  if (top.best_bid) {
    std::cout << "  Bid: " << top.best_bid->price
              << " qty=" << top.best_bid->total_quantity
              << " orders=" << top.best_bid->order_count << '\n';
  } else {
    std::cout << "  Bid: empty\n";
  }

  if (top.best_ask) {
    std::cout << "  Ask: " << top.best_ask->price
              << " qty=" << top.best_ask->total_quantity
              << " orders=" << top.best_ask->order_count << '\n';
  } else {
    std::cout << "  Ask: empty\n";
  }
}

void run_deterministic_demo() {
  lob::OrderBook book(200'000, 32);
  std::vector<lob::Trade> trades;

  book.add({1, lob::Side::Sell, 100'100, 40}, trades);
  book.add({2, lob::Side::Sell, 100'100, 30}, trades);
  book.add({3, lob::Side::Buy, 99'900, 60}, trades);
  const auto report = book.add({4, lob::Side::Buy, 100'100, 50}, trades);

  std::cout << "Deterministic demo trades\n";
  for (const auto& trade : trades) {
    std::cout << "  incoming=" << trade.incoming_order_id
              << " resting=" << trade.resting_order_id
              << " px=" << trade.price
              << " qty=" << trade.quantity << '\n';
  }
  std::cout << "  report accepted=" << report.accepted
            << " executed=" << report.executed_quantity
            << " remaining=" << report.remaining_quantity
            << " rested=" << report.rested << '\n';

  print_top(book.top_of_book());
}

void run_benchmark(std::uint32_t iterations) {
  lob::OrderBook book(200'000, iterations + 1'024U);
  std::vector<lob::Trade> trades;
  trades.reserve(16);
  lob::LatencyHistogram histogram;

  std::mt19937 rng(7);
  std::uniform_int_distribution<std::uint32_t> side_dist(0, 1);
  std::uniform_int_distribution<std::uint32_t> quantity_dist(1, 250);
  std::uniform_int_distribution<std::uint32_t> offset_dist(0, 50);
  std::bernoulli_distribution cancel_dist(0.08);

  std::vector<lob::OrderId> live_ids;
  live_ids.reserve(iterations);
  lob::OrderId next_id = 1;

  const auto start = Clock::now();

  for (std::uint32_t i = 0; i < iterations; ++i) {
    const auto op_start = Clock::now();
    if (!live_ids.empty() && cancel_dist(rng)) {
      const auto cancel_index = static_cast<std::size_t>(rng() % live_ids.size());
      const lob::OrderId id = live_ids[cancel_index];
      if (book.cancel(id)) {
        live_ids[cancel_index] = live_ids.back();
        live_ids.pop_back();
      }
      const auto op_end = Clock::now();
      histogram.record(static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start).count()
      ));
      continue;
    }

    const lob::Side side = side_dist(rng) == 0 ? lob::Side::Buy : lob::Side::Sell;
    const lob::Price anchor = side == lob::Side::Buy ? 100'000 : 100'020;
    const lob::Price price = side == lob::Side::Buy
      ? anchor - offset_dist(rng)
      : anchor + offset_dist(rng);

    const lob::AddOrder order{
      .id = next_id++,
      .side = side,
      .price = price,
      .quantity = quantity_dist(rng)
    };

    const auto report = book.add(order, trades);
    if (report.accepted && book.has_order(order.id)) {
      live_ids.push_back(order.id);
    }

    const auto op_end = Clock::now();
    histogram.record(static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start).count()
    ));
  }

  const auto end = Clock::now();
  const auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
  const auto per_op = static_cast<double>(elapsed_ns) / static_cast<double>(iterations);
  const auto stats = book.stats();

  std::cout << "\nBenchmark\n";
  std::cout << "  iterations: " << iterations << '\n';
  std::cout << "  total ns:   " << elapsed_ns << '\n';
  std::cout << "  avg ns/op:  " << std::fixed << std::setprecision(2) << per_op << '\n';
  std::cout << "  p50 ns:     " << histogram.percentile(50.0) << '\n';
  std::cout << "  p99 ns:     " << histogram.percentile(99.0) << '\n';
  std::cout << "  p999 ns:    " << histogram.percentile(99.9) << '\n';
  std::cout << "  submitted:  " << stats.submitted << '\n';
  std::cout << "  cancelled:  " << stats.cancelled << '\n';
  std::cout << "  traded qty: " << stats.executed << '\n';
  std::cout << "  trades:     " << stats.trades << '\n';

  print_top(book.top_of_book());
}

}  // namespace

int main(int argc, char** argv) {
  std::uint32_t iterations = 500'000;
  if (argc > 1) {
    iterations = static_cast<std::uint32_t>(std::stoul(argv[1]));
  }

  run_deterministic_demo();
  run_benchmark(iterations);
  return 0;
}

#include "risk/latency_histogram.hpp"
#include "risk/pre_trade_risk.hpp"

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>

namespace {

using Clock = std::chrono::steady_clock;

void configure(risk::PreTradeRiskEngine& engine) {
  engine.configure_symbol(1, {5'000, 20'000, 250}, {100'000});
  engine.configure_symbol(2, {3'000, 15'000, 300}, {50'000});
  engine.configure_user(101, {900'000'000ULL, 600'000'000ULL, 200'000});
  engine.configure_user(102, {700'000'000ULL, 450'000'000ULL, 150'000});
}

void deterministic_demo() {
  risk::PreTradeRiskEngine engine(4, 4, 32);
  configure(engine);

  const auto accepted = engine.check_and_reserve({1, 101, 1, risk::Side::Buy, 100'150, 500});
  const auto fat_finger = engine.check_and_reserve({2, 101, 1, risk::Side::Buy, 110'000, 100});
  const auto accepted_sell = engine.check_and_reserve({3, 101, 1, risk::Side::Sell, 99'900, 350});
  engine.on_fill(1, 300, 100'150);
  engine.cancel(3);

  std::cout << "Deterministic risk demo\n";
  std::cout << "  order=1 accepted=" << accepted.accepted
            << " projected_position=" << accepted.projected_position
            << " gross=" << accepted.projected_gross_notional
            << " net=" << accepted.projected_net_notional << '\n';
  std::cout << "  order=2 accepted=" << fat_finger.accepted
            << " reason=" << risk::to_string(fat_finger.reason) << '\n';
  std::cout << "  order=3 accepted=" << accepted_sell.accepted
            << " projected_position=" << accepted_sell.projected_position << '\n';
  std::cout << "  position user=101 symbol=1 after fill=" << engine.position(101, 1).value_or(0) << '\n';
}

void run_benchmark(std::uint32_t iterations) {
  risk::PreTradeRiskEngine engine(8, 8, iterations + 1'024U);
  configure(engine);

  risk::LatencyHistogram histogram;
  std::mt19937 rng(17);
  std::uniform_int_distribution<int> user_dist(101, 102);
  std::uniform_int_distribution<int> symbol_dist(1, 2);
  std::uniform_int_distribution<int> side_dist(0, 1);
  std::uniform_int_distribution<int> qty_dist(1, 1'500);
  std::uniform_int_distribution<int> price_move(-180, 180);
  std::bernoulli_distribution fill_dist(0.20);
  std::bernoulli_distribution cancel_dist(0.12);

  std::vector<risk::OrderId> live_orders;
  live_orders.reserve(iterations);
  risk::OrderId next_order_id = 1;

  const auto start = Clock::now();
  for (std::uint32_t i = 0; i < iterations; ++i) {
    const auto op_start = Clock::now();

    if (!live_orders.empty() && cancel_dist(rng)) {
      const auto index = static_cast<std::size_t>(rng() % live_orders.size());
      const auto order_id = live_orders[index];
      if (engine.cancel(order_id)) {
        live_orders[index] = live_orders.back();
        live_orders.pop_back();
      }
    } else if (!live_orders.empty() && fill_dist(rng)) {
      const auto index = static_cast<std::size_t>(rng() % live_orders.size());
      const auto order_id = live_orders[index];
      if (engine.on_fill(order_id, 1, 100'000)) {
        if (!engine.has_open_order(order_id)) {
          live_orders[index] = live_orders.back();
          live_orders.pop_back();
        }
      }
    } else {
      const auto symbol = static_cast<risk::SymbolId>(symbol_dist(rng));
      const auto base_price = symbol == 1 ? 100'000ULL : 50'000ULL;
      const risk::OrderRequest order{
        next_order_id++,
        static_cast<risk::UserId>(user_dist(rng)),
        symbol,
        side_dist(rng) == 0 ? risk::Side::Buy : risk::Side::Sell,
        static_cast<risk::Price>(base_price + price_move(rng)),
        static_cast<risk::Quantity>(qty_dist(rng))
      };
      const auto decision = engine.check_and_reserve(order);
      if (decision.accepted) {
        live_orders.push_back(order.order_id);
      }
    }

    const auto op_end = Clock::now();
    histogram.record(static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start).count()));
  }

  const auto end = Clock::now();
  const auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
  const auto per_op = static_cast<double>(elapsed_ns) / static_cast<double>(iterations);
  const auto stats = engine.stats();

  std::cout << "\nBenchmark\n";
  std::cout << "  iterations: " << iterations << '\n';
  std::cout << "  total ns:   " << elapsed_ns << '\n';
  std::cout << "  avg ns/op:  " << std::fixed << std::setprecision(2) << per_op << '\n';
  std::cout << "  p50 ns:     " << histogram.percentile(50.0) << '\n';
  std::cout << "  p99 ns:     " << histogram.percentile(99.0) << '\n';
  std::cout << "  p999 ns:    " << histogram.percentile(99.9) << '\n';
  std::cout << "  checks:     " << stats.checks << '\n';
  std::cout << "  accepted:   " << stats.accepted << '\n';
  std::cout << "  rejected:   " << stats.rejected << '\n';
  std::cout << "  live orders:" << live_orders.size() << '\n';
}

}  // namespace

int main(int argc, char** argv) {
  std::uint32_t iterations = 500'000;
  if (argc > 1) {
    iterations = static_cast<std::uint32_t>(std::stoul(argv[1]));
  }

  deterministic_demo();
  run_benchmark(iterations);
  return 0;
}

#include "arb/crypto_arb.hpp"
#include "lob/latency_histogram.hpp"

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

void seed_reference_data(arb::CryptoArbitrageDetector& detector) {
  detector.set_fx_rate({arb::QuoteCurrency::USD, 1.0});
  detector.set_fx_rate({arb::QuoteCurrency::USDT, 1.0});
  detector.set_fx_rate({arb::QuoteCurrency::EUR, 1.08});
  detector.set_fx_rate({arb::QuoteCurrency::JPY, 0.0067});
  detector.set_fx_rate({arb::QuoteCurrency::KRW, 0.00075});

  detector.upsert_quote({
    .venue = "Coinbase",
    .country = "United States",
    .instrument = "BTC/USD",
    .currency = arb::QuoteCurrency::USD,
    .bid_price = 60'520.0,
    .ask_price = 60'540.0,
    .bid_size = 1.6,
    .ask_size = 1.8,
    .exchange_timestamp_ns = 1,
    .fees = {.taker_fee_bps = 8.0, .slippage_bps = 2.0, .fixed_cost_usd = 1.0}
  });

  detector.upsert_quote({
    .venue = "Bitstamp",
    .country = "Luxembourg",
    .instrument = "BTC/USD",
    .currency = arb::QuoteCurrency::EUR,
    .bid_price = 55'980.0,
    .ask_price = 56'000.0,
    .bid_size = 1.2,
    .ask_size = 1.4,
    .exchange_timestamp_ns = 2,
    .fees = {.taker_fee_bps = 10.0, .slippage_bps = 2.0, .fixed_cost_usd = 1.5}
  });

  detector.upsert_quote({
    .venue = "bitFlyer",
    .country = "Japan",
    .instrument = "BTC/USD",
    .currency = arb::QuoteCurrency::JPY,
    .bid_price = 9'086'000.0,
    .ask_price = 9'090'000.0,
    .bid_size = 1.0,
    .ask_size = 1.1,
    .exchange_timestamp_ns = 3,
    .fees = {.taker_fee_bps = 12.0, .slippage_bps = 3.0, .fixed_cost_usd = 1.0}
  });

  detector.upsert_quote({
    .venue = "Upbit",
    .country = "South Korea",
    .instrument = "BTC/USD",
    .currency = arb::QuoteCurrency::KRW,
    .bid_price = 80'450'000.0,
    .ask_price = 80'520'000.0,
    .bid_size = 1.3,
    .ask_size = 1.5,
    .exchange_timestamp_ns = 4,
    .fees = {.taker_fee_bps = 5.0, .slippage_bps = 2.0, .fixed_cost_usd = 2.0}
  });

  detector.upsert_quote({
    .venue = "Binance",
    .country = "Global",
    .instrument = "ETH/USD",
    .currency = arb::QuoteCurrency::USDT,
    .bid_price = 3'340.0,
    .ask_price = 3'341.0,
    .bid_size = 14.0,
    .ask_size = 12.0,
    .exchange_timestamp_ns = 5,
    .fees = {.taker_fee_bps = 5.0, .slippage_bps = 1.0, .fixed_cost_usd = 0.2}
  });

  detector.upsert_quote({
    .venue = "Kraken",
    .country = "United States",
    .instrument = "ETH/USD",
    .currency = arb::QuoteCurrency::USD,
    .bid_price = 3'356.0,
    .ask_price = 3'358.0,
    .bid_size = 11.0,
    .ask_size = 10.0,
    .exchange_timestamp_ns = 6,
    .fees = {.taker_fee_bps = 9.0, .slippage_bps = 1.0, .fixed_cost_usd = 0.2}
  });
}

void print_opportunities(const std::vector<arb::ArbitrageOpportunity>& opportunities) {
  std::cout << "USD-normalized arbitrage opportunities\n";
  for (const auto& item : opportunities) {
    std::cout << "  " << item.instrument
              << ": buy on " << item.buy_venue << " (" << item.buy_country << ")"
              << " at $" << std::fixed << std::setprecision(2) << item.buy_cost_usd
              << ", sell on " << item.sell_venue << " (" << item.sell_country << ")"
              << " at $" << item.sell_value_usd
              << ", spread $" << item.net_spread_usd
              << " (" << item.net_spread_bps << " bps)"
              << ", size " << item.executable_size
              << '\n';
  }
}

void run_benchmark(std::uint32_t iterations) {
  arb::CryptoArbitrageDetector detector(32);
  seed_reference_data(detector);

  lob::LatencyHistogram histogram;
  std::mt19937 rng(17);
  std::uniform_real_distribution<double> usd_jitter(-20.0, 20.0);
  std::uniform_real_distribution<double> eur_jitter(-18.0, 18.0);
  std::uniform_real_distribution<double> jpy_jitter(-3'000.0, 3'000.0);

  const auto start = Clock::now();
  for (std::uint32_t i = 0; i < iterations; ++i) {
    const auto op_start = Clock::now();

    detector.upsert_quote({
      .venue = "Coinbase",
      .country = "United States",
      .instrument = "BTC/USD",
      .currency = arb::QuoteCurrency::USD,
      .bid_price = 60'520.0 + usd_jitter(rng),
      .ask_price = 60'540.0 + usd_jitter(rng),
      .bid_size = 1.4,
      .ask_size = 1.8,
      .exchange_timestamp_ns = static_cast<std::uint64_t>(i),
      .fees = {.taker_fee_bps = 8.0, .slippage_bps = 2.0, .fixed_cost_usd = 1.0}
    });

    detector.upsert_quote({
      .venue = "Bitstamp",
      .country = "Luxembourg",
      .instrument = "BTC/USD",
      .currency = arb::QuoteCurrency::EUR,
      .bid_price = 55'980.0 + eur_jitter(rng),
      .ask_price = 56'000.0 + eur_jitter(rng),
      .bid_size = 1.2,
      .ask_size = 1.3,
      .exchange_timestamp_ns = static_cast<std::uint64_t>(i),
      .fees = {.taker_fee_bps = 10.0, .slippage_bps = 2.0, .fixed_cost_usd = 1.5}
    });

    detector.upsert_quote({
      .venue = "bitFlyer",
      .country = "Japan",
      .instrument = "BTC/USD",
      .currency = arb::QuoteCurrency::JPY,
      .bid_price = 9'086'000.0 + jpy_jitter(rng),
      .ask_price = 9'090'000.0 + jpy_jitter(rng),
      .bid_size = 1.1,
      .ask_size = 1.0,
      .exchange_timestamp_ns = static_cast<std::uint64_t>(i),
      .fees = {.taker_fee_bps = 12.0, .slippage_bps = 3.0, .fixed_cost_usd = 1.0}
    });

    const auto best = detector.detect_best("BTC/USD");
    (void)best;

    const auto op_end = Clock::now();
    histogram.record(static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start).count()
    ));
  }

  const auto end = Clock::now();
  const auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

  std::cout << "\nCrypto arbitrage benchmark\n";
  std::cout << "  iterations: " << iterations << '\n';
  std::cout << "  avg ns/op:  " << std::fixed << std::setprecision(2)
            << static_cast<double>(elapsed_ns) / static_cast<double>(iterations) << '\n';
  std::cout << "  p50 ns:     " << histogram.percentile(50.0) << '\n';
  std::cout << "  p99 ns:     " << histogram.percentile(99.0) << '\n';
  std::cout << "  p999 ns:    " << histogram.percentile(99.9) << '\n';
}

}  // namespace

int main(int argc, char** argv) {
  std::uint32_t iterations = 200'000;
  if (argc > 1) {
    iterations = static_cast<std::uint32_t>(std::stoul(argv[1]));
  }

  arb::CryptoArbitrageDetector detector(32);
  seed_reference_data(detector);
  print_opportunities(detector.detect_all());
  run_benchmark(iterations);
  return 0;
}

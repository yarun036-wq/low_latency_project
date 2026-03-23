#include "arb/crypto_arb.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "crypto_arb_tests failure: " << message << '\n';
    std::exit(1);
  }
}

void test_usd_normalization_and_best_venue() {
  arb::CryptoArbitrageDetector detector(8);
  detector.set_fx_rate({arb::QuoteCurrency::USD, 1.0});
  detector.set_fx_rate({arb::QuoteCurrency::EUR, 1.10});
  detector.set_fx_rate({arb::QuoteCurrency::JPY, 0.0065});

  require(detector.upsert_quote({
    .venue = "US-A",
    .country = "United States",
    .instrument = "BTC/USD",
    .currency = arb::QuoteCurrency::USD,
    .bid_price = 60'780.0,
    .ask_price = 60'790.0,
    .bid_size = 2.0,
    .ask_size = 2.0,
    .exchange_timestamp_ns = 1,
    .fees = {.taker_fee_bps = 5.0, .slippage_bps = 1.0, .fixed_cost_usd = 0.5}
  }), "failed to insert usd quote");

  require(detector.upsert_quote({
    .venue = "EU-B",
    .country = "Germany",
    .instrument = "BTC/USD",
    .currency = arb::QuoteCurrency::EUR,
    .bid_price = 54'780.0,
    .ask_price = 54'800.0,
    .bid_size = 1.5,
    .ask_size = 1.5,
    .exchange_timestamp_ns = 2,
    .fees = {.taker_fee_bps = 5.0, .slippage_bps = 1.0, .fixed_cost_usd = 0.5}
  }), "failed to insert eur quote");

  const auto best = detector.detect_best("BTC/USD");
  require(best.has_value(), "expected arbitrage opportunity");
  require(best->buy_venue == "EU-B", "expected EU venue to be cheapest in USD terms");
  require(best->sell_venue == "US-A", "expected US venue to be highest sell venue");
  require(best->net_spread_usd > 0.0, "spread should be positive");
}

void test_invalid_quotes_are_rejected() {
  arb::CryptoArbitrageDetector detector(4);
  const bool accepted = detector.upsert_quote({
    .venue = "BadVenue",
    .country = "Nowhere",
    .instrument = "ETH/USD",
    .currency = arb::QuoteCurrency::USD,
    .bid_price = 3000.0,
    .ask_price = 2999.0,
    .bid_size = 1.0,
    .ask_size = 1.0,
    .exchange_timestamp_ns = 1,
    .fees = {}
  });
  require(!accepted, "crossed quote should be rejected");
}

void test_detect_all_orders_by_spread() {
  arb::CryptoArbitrageDetector detector(8);
  detector.set_fx_rate({arb::QuoteCurrency::USD, 1.0});
  detector.set_fx_rate({arb::QuoteCurrency::USDT, 1.0});

  detector.upsert_quote({
    .venue = "Venue-1",
    .country = "US",
    .instrument = "BTC/USD",
    .currency = arb::QuoteCurrency::USD,
    .bid_price = 60'700.0,
    .ask_price = 60'705.0,
    .bid_size = 1.0,
    .ask_size = 1.0,
    .exchange_timestamp_ns = 1,
    .fees = {.taker_fee_bps = 5.0, .slippage_bps = 1.0, .fixed_cost_usd = 0.25}
  });

  detector.upsert_quote({
    .venue = "Venue-2",
    .country = "Global",
    .instrument = "BTC/USD",
    .currency = arb::QuoteCurrency::USDT,
    .bid_price = 60'590.0,
    .ask_price = 60'595.0,
    .bid_size = 1.0,
    .ask_size = 1.0,
    .exchange_timestamp_ns = 2,
    .fees = {.taker_fee_bps = 5.0, .slippage_bps = 1.0, .fixed_cost_usd = 0.25}
  });

  detector.upsert_quote({
    .venue = "Venue-3",
    .country = "US",
    .instrument = "ETH/USD",
    .currency = arb::QuoteCurrency::USD,
    .bid_price = 3'355.0,
    .ask_price = 3'356.0,
    .bid_size = 4.0,
    .ask_size = 4.0,
    .exchange_timestamp_ns = 3,
    .fees = {.taker_fee_bps = 4.0, .slippage_bps = 1.0, .fixed_cost_usd = 0.1}
  });

  detector.upsert_quote({
    .venue = "Venue-4",
    .country = "Global",
    .instrument = "ETH/USD",
    .currency = arb::QuoteCurrency::USDT,
    .bid_price = 3'330.0,
    .ask_price = 3'331.0,
    .bid_size = 4.0,
    .ask_size = 4.0,
    .exchange_timestamp_ns = 4,
    .fees = {.taker_fee_bps = 4.0, .slippage_bps = 1.0, .fixed_cost_usd = 0.1}
  });

  const auto opportunities = detector.detect_all();
  require(opportunities.size() == 2, "expected one opportunity per instrument");
  require(opportunities[0].net_spread_usd >= opportunities[1].net_spread_usd, "opportunities should be sorted");
}

}  // namespace

int main() {
  test_usd_normalization_and_best_venue();
  test_invalid_quotes_are_rejected();
  test_detect_all_orders_by_spread();
  std::cout << "crypto_arb_tests: all checks passed\n";
  return 0;
}

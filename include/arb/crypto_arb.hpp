#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace arb {

enum class QuoteCurrency : std::uint8_t {
  USD,
  EUR,
  JPY,
  KRW,
  USDT
};

struct FeeModel {
  double taker_fee_bps{};
  double slippage_bps{};
  double fixed_cost_usd{};
};

struct QuoteSnapshot {
  std::string venue;
  std::string country;
  std::string instrument;
  QuoteCurrency currency{QuoteCurrency::USD};
  double bid_price{};
  double ask_price{};
  double bid_size{};
  double ask_size{};
  std::uint64_t exchange_timestamp_ns{};
  FeeModel fees{};
};

struct FxRate {
  QuoteCurrency currency{QuoteCurrency::USD};
  double usd_per_unit{};
};

struct NormalizedQuote {
  std::string venue;
  std::string country;
  std::string instrument;
  QuoteCurrency currency{QuoteCurrency::USD};
  double gross_bid_usd{};
  double gross_ask_usd{};
  double effective_sell_usd{};
  double effective_buy_usd{};
  double executable_size{};
  std::uint64_t exchange_timestamp_ns{};
};

struct ArbitrageOpportunity {
  std::string instrument;
  std::string buy_venue;
  std::string buy_country;
  std::string sell_venue;
  std::string sell_country;
  double buy_cost_usd{};
  double sell_value_usd{};
  double net_spread_usd{};
  double net_spread_bps{};
  double executable_size{};
};

class CryptoArbitrageDetector {
public:
  explicit CryptoArbitrageDetector(std::size_t max_quotes);

  void set_fx_rate(const FxRate& rate);
  bool upsert_quote(const QuoteSnapshot& snapshot);
  std::optional<ArbitrageOpportunity> detect_best(std::string_view instrument) const;
  std::vector<ArbitrageOpportunity> detect_all() const;
  std::vector<NormalizedQuote> normalized_quotes(std::string_view instrument) const;

private:
  std::optional<double> usd_per_unit(QuoteCurrency currency) const noexcept;
  std::optional<NormalizedQuote> normalize(const QuoteSnapshot& snapshot) const;

  std::vector<QuoteSnapshot> quotes_;
  double fx_to_usd_[5]{1.0, 1.08, 0.0067, 0.00075, 1.0};
};

std::string_view to_string(QuoteCurrency currency) noexcept;

}  // namespace arb

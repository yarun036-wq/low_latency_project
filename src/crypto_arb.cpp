#include "arb/crypto_arb.hpp"

#include <algorithm>
#include <array>
#include <limits>

namespace arb {

namespace {

constexpr std::size_t currency_index(QuoteCurrency currency) noexcept {
  return static_cast<std::size_t>(currency);
}

double fee_multiplier(double bps) noexcept {
  return bps / 10'000.0;
}

}  // namespace

CryptoArbitrageDetector::CryptoArbitrageDetector(std::size_t max_quotes) {
  quotes_.reserve(max_quotes);
}

void CryptoArbitrageDetector::set_fx_rate(const FxRate& rate) {
  fx_to_usd_[currency_index(rate.currency)] = rate.usd_per_unit;
}

bool CryptoArbitrageDetector::upsert_quote(const QuoteSnapshot& snapshot) {
  if (snapshot.instrument.empty() ||
      snapshot.venue.empty() ||
      snapshot.bid_price <= 0.0 ||
      snapshot.ask_price <= 0.0 ||
      snapshot.bid_price > snapshot.ask_price ||
      snapshot.bid_size <= 0.0 ||
      snapshot.ask_size <= 0.0) {
    return false;
  }

  const auto match = std::find_if(quotes_.begin(), quotes_.end(), [&](const QuoteSnapshot& quote) {
    return quote.instrument == snapshot.instrument && quote.venue == snapshot.venue;
  });

  if (match != quotes_.end()) {
    *match = snapshot;
    return true;
  }

  quotes_.push_back(snapshot);
  return true;
}

std::optional<double> CryptoArbitrageDetector::usd_per_unit(QuoteCurrency currency) const noexcept {
  const double rate = fx_to_usd_[currency_index(currency)];
  if (rate <= 0.0) {
    return std::nullopt;
  }
  return rate;
}

std::optional<NormalizedQuote> CryptoArbitrageDetector::normalize(const QuoteSnapshot& snapshot) const {
  const auto fx = usd_per_unit(snapshot.currency);
  if (!fx) {
    return std::nullopt;
  }

  const double gross_bid_usd = snapshot.bid_price * *fx;
  const double gross_ask_usd = snapshot.ask_price * *fx;
  const double total_bps = snapshot.fees.taker_fee_bps + snapshot.fees.slippage_bps;
  const double buy_cost = gross_ask_usd * (1.0 + fee_multiplier(total_bps)) + snapshot.fees.fixed_cost_usd;
  const double sell_value = gross_bid_usd * (1.0 - fee_multiplier(total_bps)) - snapshot.fees.fixed_cost_usd;

  return NormalizedQuote{
    .venue = snapshot.venue,
    .country = snapshot.country,
    .instrument = snapshot.instrument,
    .currency = snapshot.currency,
    .gross_bid_usd = gross_bid_usd,
    .gross_ask_usd = gross_ask_usd,
    .effective_sell_usd = sell_value,
    .effective_buy_usd = buy_cost,
    .executable_size = std::min(snapshot.bid_size, snapshot.ask_size),
    .exchange_timestamp_ns = snapshot.exchange_timestamp_ns
  };
}

std::vector<NormalizedQuote> CryptoArbitrageDetector::normalized_quotes(std::string_view instrument) const {
  std::vector<NormalizedQuote> normalized;
  normalized.reserve(quotes_.size());

  for (const auto& quote : quotes_) {
    if (quote.instrument != instrument) {
      continue;
    }
    const auto converted = normalize(quote);
    if (converted) {
      normalized.push_back(*converted);
    }
  }

  return normalized;
}

std::optional<ArbitrageOpportunity> CryptoArbitrageDetector::detect_best(std::string_view instrument) const {
  const auto normalized = normalized_quotes(instrument);
  if (normalized.size() < 2) {
    return std::nullopt;
  }

  std::optional<ArbitrageOpportunity> best;
  double best_spread = 0.0;

  for (const auto& buy : normalized) {
    for (const auto& sell : normalized) {
      if (buy.venue == sell.venue) {
        continue;
      }

      const double executable_size = std::min(buy.executable_size, sell.executable_size);
      if (executable_size <= 0.0) {
        continue;
      }

      const double spread = sell.effective_sell_usd - buy.effective_buy_usd;
      if (spread <= best_spread) {
        continue;
      }

      best_spread = spread;
      best = ArbitrageOpportunity{
        .instrument = std::string(instrument),
        .buy_venue = buy.venue,
        .buy_country = buy.country,
        .sell_venue = sell.venue,
        .sell_country = sell.country,
        .buy_cost_usd = buy.effective_buy_usd,
        .sell_value_usd = sell.effective_sell_usd,
        .net_spread_usd = spread,
        .net_spread_bps = buy.effective_buy_usd > 0.0 ? (spread / buy.effective_buy_usd) * 10'000.0 : 0.0,
        .executable_size = executable_size
      };
    }
  }

  return best;
}

std::vector<ArbitrageOpportunity> CryptoArbitrageDetector::detect_all() const {
  std::vector<std::string> instruments;
  instruments.reserve(quotes_.size());

  for (const auto& quote : quotes_) {
    if (std::find(instruments.begin(), instruments.end(), quote.instrument) == instruments.end()) {
      instruments.push_back(quote.instrument);
    }
  }

  std::vector<ArbitrageOpportunity> opportunities;
  opportunities.reserve(instruments.size());
  for (const auto& instrument : instruments) {
    const auto best = detect_best(instrument);
    if (best) {
      opportunities.push_back(*best);
    }
  }

  std::sort(opportunities.begin(), opportunities.end(), [](const ArbitrageOpportunity& left, const ArbitrageOpportunity& right) {
    return left.net_spread_usd > right.net_spread_usd;
  });

  return opportunities;
}

std::string_view to_string(QuoteCurrency currency) noexcept {
  switch (currency) {
    case QuoteCurrency::USD:
      return "USD";
    case QuoteCurrency::EUR:
      return "EUR";
    case QuoteCurrency::JPY:
      return "JPY";
    case QuoteCurrency::KRW:
      return "KRW";
    case QuoteCurrency::USDT:
      return "USDT";
  }
  return "UNKNOWN";
}

}  // namespace arb

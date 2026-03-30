#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <unordered_map>

namespace risk {

using OrderId = std::uint64_t;
using UserId = std::uint64_t;
using SymbolId = std::uint32_t;
using Price = std::uint64_t;
using Quantity = std::uint32_t;

enum class Side : std::uint8_t {
  Buy,
  Sell
};

enum class RejectReason : std::uint8_t {
  None,
  UnknownUser,
  UnknownSymbol,
  DuplicateOrderId,
  InvalidOrder,
  MaxOrderQuantityExceeded,
  PriceBandExceeded,
  SymbolPositionLimitExceeded,
  GrossExposureLimitExceeded,
  NetExposureLimitExceeded,
  OpenOrderLimitExceeded
};

struct OrderRequest {
  OrderId order_id{};
  UserId user_id{};
  SymbolId symbol_id{};
  Side side{};
  Price price{};
  Quantity quantity{};
};

struct SymbolRiskLimits {
  Quantity max_order_quantity{};
  Quantity max_position_abs{};
  std::uint32_t max_price_deviation_bps{};
};

struct PortfolioRiskLimits {
  std::uint64_t max_gross_notional{};
  std::uint64_t max_net_notional{};
  std::uint32_t max_open_orders{};
};

struct MarketState {
  Price reference_price{};
};

struct RiskDecision {
  bool accepted{false};
  RejectReason reason{RejectReason::None};
  std::int64_t projected_position{};
  std::uint64_t projected_gross_notional{};
  std::uint64_t projected_net_notional{};
};

struct Stats {
  std::uint64_t checks{};
  std::uint64_t accepted{};
  std::uint64_t rejected{};
};

class PreTradeRiskEngine {
public:
  PreTradeRiskEngine(std::size_t expected_users, std::size_t expected_symbols, std::size_t expected_open_orders);

  void configure_symbol(SymbolId symbol_id, const SymbolRiskLimits& limits, MarketState market);
  void update_market(SymbolId symbol_id, MarketState market);
  void configure_user(UserId user_id, const PortfolioRiskLimits& limits);

  [[nodiscard]] RiskDecision check_and_reserve(const OrderRequest& order);
  bool cancel(OrderId order_id);
  bool on_fill(OrderId order_id, Quantity fill_quantity, Price fill_price);

  [[nodiscard]] bool has_open_order(OrderId order_id) const;
  [[nodiscard]] std::optional<std::int64_t> position(UserId user_id, SymbolId symbol_id) const;
  [[nodiscard]] const Stats& stats() const noexcept;

private:
  struct SymbolConfig {
    SymbolRiskLimits limits{};
    MarketState market{};
  };

  struct UserState {
    PortfolioRiskLimits limits{};
    std::uint64_t gross_open_notional{};
    std::int64_t net_open_notional{};
    std::uint32_t open_orders{};
  };

  struct OrderState {
    OrderId order_id{};
    UserId user_id{};
    SymbolId symbol_id{};
    Side side{};
    Price price{};
    Quantity remaining_quantity{};
    std::uint64_t reserved_notional{};
  };

  struct UserSymbolState {
    std::int64_t position{};
    Quantity pending_buy_quantity{};
    Quantity pending_sell_quantity{};
  };

  [[nodiscard]] static std::int64_t signed_quantity(Side side, Quantity quantity) noexcept;
  [[nodiscard]] static std::int64_t signed_notional(Side side, Price price, Quantity quantity) noexcept;

  void reserve_order(const OrderRequest& order) noexcept;
  void release_order(const OrderState& order_state) noexcept;

  std::unordered_map<SymbolId, SymbolConfig> symbols_;
  std::unordered_map<UserId, UserState> users_;
  std::unordered_map<OrderId, OrderState> open_orders_;
  std::unordered_map<std::uint64_t, UserSymbolState> user_symbol_state_;
  Stats stats_{};
};

[[nodiscard]] std::string_view to_string(Side side) noexcept;
[[nodiscard]] std::string_view to_string(RejectReason reason) noexcept;

}  // namespace risk

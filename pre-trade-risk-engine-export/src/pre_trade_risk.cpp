#include "risk/pre_trade_risk.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace risk {

namespace {

std::uint64_t user_symbol_key(UserId user_id, SymbolId symbol_id) noexcept {
  return (static_cast<std::uint64_t>(user_id) << 32U) | static_cast<std::uint64_t>(symbol_id);
}

}  // namespace

PreTradeRiskEngine::PreTradeRiskEngine(
    std::size_t expected_users,
    std::size_t expected_symbols,
    std::size_t expected_open_orders) {
  users_.reserve(expected_users);
  symbols_.reserve(expected_symbols);
  open_orders_.reserve(expected_open_orders);
  user_symbol_state_.reserve(expected_users * std::max<std::size_t>(expected_symbols, 1U));
}

void PreTradeRiskEngine::configure_symbol(SymbolId symbol_id, const SymbolRiskLimits& limits, MarketState market) {
  symbols_[symbol_id] = SymbolConfig{limits, market};
}

void PreTradeRiskEngine::update_market(SymbolId symbol_id, MarketState market) {
  auto it = symbols_.find(symbol_id);
  if (it != symbols_.end()) {
    it->second.market = market;
  }
}

void PreTradeRiskEngine::configure_user(UserId user_id, const PortfolioRiskLimits& limits) {
  users_[user_id].limits = limits;
}

RiskDecision PreTradeRiskEngine::check_and_reserve(const OrderRequest& order) {
  ++stats_.checks;
  RiskDecision decision;

  const auto user_it = users_.find(order.user_id);
  if (user_it == users_.end()) {
    decision.reason = RejectReason::UnknownUser;
  } else if (!symbols_.contains(order.symbol_id)) {
    decision.reason = RejectReason::UnknownSymbol;
  } else if (open_orders_.contains(order.order_id)) {
    decision.reason = RejectReason::DuplicateOrderId;
  } else if (order.quantity == 0 || order.price == 0) {
    decision.reason = RejectReason::InvalidOrder;
  } else {
    const auto& symbol = symbols_.at(order.symbol_id);
    auto& user = user_it->second;
    const std::uint64_t notional = static_cast<std::uint64_t>(order.price) * static_cast<std::uint64_t>(order.quantity);

    if (order.quantity > symbol.limits.max_order_quantity) {
      decision.reason = RejectReason::MaxOrderQuantityExceeded;
    } else if (symbol.market.reference_price == 0) {
      decision.reason = RejectReason::InvalidOrder;
    } else {
      const std::uint64_t price_delta = order.price > symbol.market.reference_price
          ? order.price - symbol.market.reference_price
          : symbol.market.reference_price - order.price;
      const std::uint64_t deviation_bps = (price_delta * 10'000ULL) / symbol.market.reference_price;
      if (deviation_bps > symbol.limits.max_price_deviation_bps) {
        decision.reason = RejectReason::PriceBandExceeded;
      } else {
        const auto symbol_state_it = user_symbol_state_.find(user_symbol_key(order.user_id, order.symbol_id));
        const UserSymbolState empty_state{};
        const auto& symbol_state = symbol_state_it == user_symbol_state_.end() ? empty_state : symbol_state_it->second;
        decision.projected_position = symbol_state.position
            + static_cast<std::int64_t>(symbol_state.pending_buy_quantity)
            - static_cast<std::int64_t>(symbol_state.pending_sell_quantity)
            + signed_quantity(order.side, order.quantity);
        if (std::llabs(decision.projected_position) >
            static_cast<long long>(symbol.limits.max_position_abs)) {
          decision.reason = RejectReason::SymbolPositionLimitExceeded;
        } else if (user.open_orders + 1U > user.limits.max_open_orders) {
          decision.reason = RejectReason::OpenOrderLimitExceeded;
        } else {
          decision.projected_gross_notional = user.gross_open_notional + notional;
          decision.projected_net_notional = static_cast<std::uint64_t>(
              std::llabs(user.net_open_notional + signed_notional(order.side, order.price, order.quantity)));

          if (decision.projected_gross_notional > user.limits.max_gross_notional) {
            decision.reason = RejectReason::GrossExposureLimitExceeded;
          } else if (decision.projected_net_notional > user.limits.max_net_notional) {
            decision.reason = RejectReason::NetExposureLimitExceeded;
          } else {
            decision.accepted = true;
            decision.reason = RejectReason::None;
            reserve_order(order);
            ++stats_.accepted;
            return decision;
          }
        }
      }
    }
  }

  ++stats_.rejected;
  return decision;
}

bool PreTradeRiskEngine::cancel(OrderId order_id) {
  const auto it = open_orders_.find(order_id);
  if (it == open_orders_.end()) {
    return false;
  }
  release_order(it->second);
  open_orders_.erase(it);
  return true;
}

bool PreTradeRiskEngine::on_fill(OrderId order_id, Quantity fill_quantity, Price fill_price) {
  const auto it = open_orders_.find(order_id);
  if (it == open_orders_.end() || fill_quantity == 0 || fill_quantity > it->second.remaining_quantity) {
    return false;
  }

  auto& order = it->second;
  auto& symbol_state = user_symbol_state_[user_symbol_key(order.user_id, order.symbol_id)];
  auto& position = symbol_state.position;
  position += signed_quantity(order.side, fill_quantity);
  if (order.side == Side::Buy) {
    symbol_state.pending_buy_quantity -= fill_quantity;
  } else {
    symbol_state.pending_sell_quantity -= fill_quantity;
  }

  auto user_it = users_.find(order.user_id);
  if (user_it == users_.end()) {
    return false;
  }

  auto& user = user_it->second;
  const std::uint64_t reserved_release = std::min<std::uint64_t>(
      order.reserved_notional,
      static_cast<std::uint64_t>(order.price) * static_cast<std::uint64_t>(fill_quantity));
  user.gross_open_notional -= reserved_release;
  user.net_open_notional -= signed_notional(order.side, order.price, fill_quantity);
  order.reserved_notional -= reserved_release;
  order.remaining_quantity -= fill_quantity;

  (void)fill_price;

  if (order.remaining_quantity == 0) {
    if (user.open_orders > 0) {
      --user.open_orders;
    }
    open_orders_.erase(it);
  }
  return true;
}

bool PreTradeRiskEngine::has_open_order(OrderId order_id) const {
  return open_orders_.contains(order_id);
}

std::optional<std::int64_t> PreTradeRiskEngine::position(UserId user_id, SymbolId symbol_id) const {
  const auto it = user_symbol_state_.find(user_symbol_key(user_id, symbol_id));
  if (it == user_symbol_state_.end()) {
    return std::nullopt;
  }
  return it->second.position;
}

const Stats& PreTradeRiskEngine::stats() const noexcept {
  return stats_;
}

std::int64_t PreTradeRiskEngine::signed_quantity(Side side, Quantity quantity) noexcept {
  return side == Side::Buy ? static_cast<std::int64_t>(quantity) : -static_cast<std::int64_t>(quantity);
}

std::int64_t PreTradeRiskEngine::signed_notional(Side side, Price price, Quantity quantity) noexcept {
  const auto value = static_cast<std::int64_t>(price) * static_cast<std::int64_t>(quantity);
  return side == Side::Buy ? value : -value;
}

void PreTradeRiskEngine::reserve_order(const OrderRequest& order) noexcept {
  auto& user = users_[order.user_id];
  auto& symbol_state = user_symbol_state_[user_symbol_key(order.user_id, order.symbol_id)];
  const std::uint64_t notional = static_cast<std::uint64_t>(order.price) * static_cast<std::uint64_t>(order.quantity);
  user.gross_open_notional += notional;
  user.net_open_notional += signed_notional(order.side, order.price, order.quantity);
  ++user.open_orders;
  if (order.side == Side::Buy) {
    symbol_state.pending_buy_quantity += order.quantity;
  } else {
    symbol_state.pending_sell_quantity += order.quantity;
  }

  open_orders_.emplace(order.order_id, OrderState{
      order.order_id,
      order.user_id,
      order.symbol_id,
      order.side,
      order.price,
      order.quantity,
      notional});
}

void PreTradeRiskEngine::release_order(const OrderState& order_state) noexcept {
  auto& user = users_[order_state.user_id];
  auto& symbol_state = user_symbol_state_[user_symbol_key(order_state.user_id, order_state.symbol_id)];
  user.gross_open_notional -= order_state.reserved_notional;
  user.net_open_notional -= signed_notional(order_state.side, order_state.price, order_state.remaining_quantity);
  if (user.open_orders > 0) {
    --user.open_orders;
  }
  if (order_state.side == Side::Buy) {
    symbol_state.pending_buy_quantity -= order_state.remaining_quantity;
  } else {
    symbol_state.pending_sell_quantity -= order_state.remaining_quantity;
  }
}

std::string_view to_string(Side side) noexcept {
  return side == Side::Buy ? "Buy" : "Sell";
}

std::string_view to_string(RejectReason reason) noexcept {
  switch (reason) {
    case RejectReason::None: return "None";
    case RejectReason::UnknownUser: return "UnknownUser";
    case RejectReason::UnknownSymbol: return "UnknownSymbol";
    case RejectReason::DuplicateOrderId: return "DuplicateOrderId";
    case RejectReason::InvalidOrder: return "InvalidOrder";
    case RejectReason::MaxOrderQuantityExceeded: return "MaxOrderQuantityExceeded";
    case RejectReason::PriceBandExceeded: return "PriceBandExceeded";
    case RejectReason::SymbolPositionLimitExceeded: return "SymbolPositionLimitExceeded";
    case RejectReason::GrossExposureLimitExceeded: return "GrossExposureLimitExceeded";
    case RejectReason::NetExposureLimitExceeded: return "NetExposureLimitExceeded";
    case RejectReason::OpenOrderLimitExceeded: return "OpenOrderLimitExceeded";
  }
  return "Unknown";
}

}  // namespace risk

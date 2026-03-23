#include "lob/order_book.hpp"

#include <algorithm>
#include <stdexcept>

namespace lob {

OrderBook::OrderBook(Price max_price, std::size_t max_orders)
    : max_price_(max_price),
      bids_(static_cast<std::size_t>(max_price) + 1U),
      asks_(static_cast<std::size_t>(max_price) + 1U),
      orders_(max_orders),
      best_ask_(max_price + 1U) {
  free_list_.reserve(max_orders);
  order_lookup_.reserve(max_orders);
  for (std::uint32_t index = 0; index < max_orders; ++index) {
    free_list_.push_back(static_cast<std::uint32_t>(max_orders - 1U - index));
  }
}

bool OrderBook::validate_new_order(const AddOrder& order) const noexcept {
  return order.id > 0 &&
         order.price <= max_price_ &&
         order.quantity > 0 &&
         !order_lookup_.contains(order.id);
}

Quantity OrderBook::available_to_cross(const AddOrder& order) const noexcept {
  Quantity available = 0;

  if (order.side == Side::Buy) {
    for (Price price = best_ask_; price <= max_price_ && available < order.quantity; ++price) {
      if (asks_[price].empty()) {
        continue;
      }
      if (order.type == OrderType::Limit && price > order.price) {
        break;
      }
      available += asks_[price].total_quantity;
    }
  } else {
    for (Price price = best_bid_; price > 0 && available < order.quantity; --price) {
      if (bids_[price].empty()) {
        continue;
      }
      if (order.type == OrderType::Limit && price < order.price) {
        break;
      }
      available += bids_[price].total_quantity;
    }
  }

  return available;
}

std::uint32_t OrderBook::allocate_node() {
  if (free_list_.empty()) {
    throw std::runtime_error("order pool exhausted");
  }

  const std::uint32_t index = free_list_.back();
  free_list_.pop_back();
  return index;
}

void OrderBook::release_node(std::uint32_t index) noexcept {
  orders_[index] = {};
  free_list_.push_back(index);
}

void OrderBook::append_to_level(std::uint32_t index) noexcept {
  auto& node = orders_[index];
  auto& levels = node.side == Side::Buy ? bids_ : asks_;
  auto& level = levels[node.price];

  node.prev = level.tail;
  node.next = invalid_index;

  if (level.tail != invalid_index) {
    orders_[level.tail].next = index;
  } else {
    level.head = index;
  }

  level.tail = index;
  level.total_quantity += node.quantity;
  level.order_count += 1;

  if (node.side == Side::Buy) {
    best_bid_ = std::max(best_bid_, node.price);
  } else {
    best_ask_ = std::min(best_ask_, node.price);
  }
}

void OrderBook::unlink_from_level(std::uint32_t index) noexcept {
  auto& node = orders_[index];
  auto& levels = node.side == Side::Buy ? bids_ : asks_;
  auto& level = levels[node.price];

  if (node.prev != invalid_index) {
    orders_[node.prev].next = node.next;
  } else {
    level.head = node.next;
  }

  if (node.next != invalid_index) {
    orders_[node.next].prev = node.prev;
  } else {
    level.tail = node.prev;
  }

  level.total_quantity -= node.quantity;
  level.order_count -= 1;

  if (level.empty()) {
    if (node.side == Side::Buy) {
      update_best_bid_after_remove(node.price);
    } else {
      update_best_ask_after_remove(node.price);
    }
  }
}

void OrderBook::update_best_bid_after_remove(Price removed_price) noexcept {
  if (removed_price != best_bid_) {
    return;
  }

  while (best_bid_ > 0 && bids_[best_bid_].empty()) {
    --best_bid_;
  }
}

void OrderBook::update_best_ask_after_remove(Price removed_price) noexcept {
  if (removed_price != best_ask_) {
    return;
  }

  while (best_ask_ <= max_price_ && asks_[best_ask_].empty()) {
    ++best_ask_;
  }
}

void OrderBook::consume_level_head(PriceLevel& level, OrderNode& incoming, std::vector<Trade>& trades) {
  while (incoming.quantity > 0 && !level.empty()) {
    const std::uint32_t resting_index = level.head;
    auto& resting = orders_[resting_index];

    const Quantity matched = std::min(incoming.quantity, resting.quantity);
    trades.push_back(Trade{
      .resting_order_id = resting.id,
      .incoming_order_id = incoming.id,
      .price = resting.price,
      .quantity = matched
    });

    incoming.quantity -= matched;
    resting.quantity -= matched;
    stats_.trades += 1;
    stats_.executed += matched;
    level.total_quantity -= matched;

    if (resting.quantity == 0) {
      order_lookup_.erase(resting.id);
      unlink_from_level(resting_index);
      release_node(resting_index);
    }
  }
}

void OrderBook::match_buy(OrderNode& incoming, std::vector<Trade>& trades) {
  while (incoming.quantity > 0 && best_ask_ <= max_price_ && best_ask_ <= incoming.price) {
    auto& level = asks_[best_ask_];
    consume_level_head(level, incoming, trades);
    if (level.empty()) {
      update_best_ask_after_remove(best_ask_);
    }
  }
}

void OrderBook::match_sell(OrderNode& incoming, std::vector<Trade>& trades) {
  while (incoming.quantity > 0 && best_bid_ > 0 && best_bid_ >= incoming.price) {
    auto& level = bids_[best_bid_];
    consume_level_head(level, incoming, trades);
    if (level.empty()) {
      update_best_bid_after_remove(best_bid_);
    }
  }
}

ExecutionReport OrderBook::add(const AddOrder& order, std::vector<Trade>& trades) {
  ExecutionReport report;
  if (!validate_new_order(order)) {
    return report;
  }

  trades.clear();
  report.accepted = true;

  if (order.tif == TimeInForce::Fok && available_to_cross(order) < order.quantity) {
    report.cancelled = true;
    report.remaining_quantity = order.quantity;
    return report;
  }

  OrderNode incoming{
    .id = order.id,
    .side = order.side,
    .price = order.price,
    .quantity = order.quantity,
    .next = invalid_index,
    .prev = invalid_index,
    .active = true
  };

  if (incoming.side == Side::Buy) {
    match_buy(incoming, trades);
  } else {
    match_sell(incoming, trades);
  }

  stats_.submitted += 1;
  report.executed_quantity = order.quantity - incoming.quantity;
  report.remaining_quantity = incoming.quantity;

  if (incoming.quantity == 0U) {
    report.fully_filled = true;
    return report;
  }

  if (order.type == OrderType::Market || order.tif == TimeInForce::Ioc || order.tif == TimeInForce::Fok) {
    report.cancelled = true;
    return report;
  }

  const std::uint32_t index = allocate_node();
  orders_[index] = incoming;
  order_lookup_.emplace(incoming.id, index);
  append_to_level(index);
  report.rested = true;
  return report;
}

bool OrderBook::cancel(OrderId id) {
  if (id == 0) {
    return false;
  }

  const auto lookup = order_lookup_.find(id);
  if (lookup == order_lookup_.end()) {
    return false;
  }

  const std::uint32_t index = lookup->second;
  auto& node = orders_[index];
  order_lookup_.erase(lookup);
  unlink_from_level(index);
  release_node(index);
  stats_.cancelled += 1;
  return true;
}

bool OrderBook::has_order(OrderId id) const noexcept {
  return order_lookup_.contains(id);
}

std::optional<BookLevel> OrderBook::snapshot_level(Side side, Price price) const noexcept {
  if (price > max_price_) {
    return std::nullopt;
  }

  const auto& levels = side == Side::Buy ? bids_ : asks_;
  const auto& level = levels[price];
  if (level.empty()) {
    return std::nullopt;
  }

  return BookLevel{
    .price = price,
    .total_quantity = level.total_quantity,
    .order_count = level.order_count
  };
}

TopOfBook OrderBook::top_of_book() const {
  return TopOfBook{
    .best_bid = snapshot_level(Side::Buy, best_bid_),
    .best_ask = snapshot_level(Side::Sell, best_ask_)
  };
}

const Stats& OrderBook::stats() const noexcept {
  return stats_;
}

Price OrderBook::max_price() const noexcept {
  return max_price_;
}

std::size_t OrderBook::capacity() const noexcept {
  return orders_.size();
}

std::string_view to_string(Side side) noexcept {
  return side == Side::Buy ? "BUY" : "SELL";
}

std::string_view to_string(OrderType type) noexcept {
  return type == OrderType::Limit ? "LIMIT" : "MARKET";
}

std::string_view to_string(TimeInForce tif) noexcept {
  switch (tif) {
    case TimeInForce::Gtc:
      return "GTC";
    case TimeInForce::Ioc:
      return "IOC";
    case TimeInForce::Fok:
      return "FOK";
  }
  return "UNKNOWN";
}

}  // namespace lob

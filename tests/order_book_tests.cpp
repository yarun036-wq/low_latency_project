#include "lob/order_book.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

struct RefOrder {
  lob::OrderId id{};
  lob::Side side{};
  lob::Price price{};
  lob::Quantity quantity{};
};

class ReferenceBook {
public:
  lob::ExecutionReport add(const lob::AddOrder& order, std::vector<lob::Trade>& trades) {
    lob::ExecutionReport report;
    if (ids_.contains(order.id) || order.quantity == 0) {
      return report;
    }

    report.accepted = true;
    trades.clear();
    lob::Quantity remaining = order.quantity;

    if (order.tif == lob::TimeInForce::Fok && available_to_cross(order) < order.quantity) {
      report.cancelled = true;
      report.remaining_quantity = order.quantity;
      return report;
    }

    if (order.side == lob::Side::Buy) {
      while (remaining > 0 && !asks_.empty()) {
        auto it = asks_.begin();
        if (order.type == lob::OrderType::Limit && it->first > order.price) {
          break;
        }
        auto& queue = it->second;
        while (remaining > 0 && !queue.empty()) {
          auto& resting = queue.front();
          const auto matched = std::min(remaining, resting.quantity);
          trades.push_back({resting.id, order.id, resting.price, matched});
          remaining -= matched;
          resting.quantity -= matched;
          if (resting.quantity == 0) {
            ids_.erase(resting.id);
            queue.erase(queue.begin());
          }
        }
        if (queue.empty()) {
          asks_.erase(it);
        }
      }
    } else {
      while (remaining > 0 && !bids_.empty()) {
        auto it = std::prev(bids_.end());
        if (order.type == lob::OrderType::Limit && it->first < order.price) {
          break;
        }
        auto& queue = it->second;
        while (remaining > 0 && !queue.empty()) {
          auto& resting = queue.front();
          const auto matched = std::min(remaining, resting.quantity);
          trades.push_back({resting.id, order.id, resting.price, matched});
          remaining -= matched;
          resting.quantity -= matched;
          if (resting.quantity == 0) {
            ids_.erase(resting.id);
            queue.erase(queue.begin());
          }
        }
        if (queue.empty()) {
          bids_.erase(it);
        }
      }
    }

    report.executed_quantity = order.quantity - remaining;
    report.remaining_quantity = remaining;
    report.fully_filled = remaining == 0;

    if (remaining > 0 &&
        order.type == lob::OrderType::Limit &&
        order.tif == lob::TimeInForce::Gtc) {
      auto& book_side = order.side == lob::Side::Buy ? bids_ : asks_;
      book_side[order.price].push_back({order.id, order.side, order.price, remaining});
      ids_.insert(order.id);
      report.rested = true;
    } else if (remaining > 0) {
      report.cancelled = true;
    }

    return report;
  }

  bool cancel(lob::OrderId id) {
    for (auto* side : {&bids_, &asks_}) {
      for (auto level_it = side->begin(); level_it != side->end(); ++level_it) {
        auto& queue = level_it->second;
        auto it = std::find_if(queue.begin(), queue.end(), [&](const RefOrder& order) {
          return order.id == id;
        });
        if (it != queue.end()) {
          queue.erase(it);
          ids_.erase(id);
          if (queue.empty()) {
            side->erase(level_it);
          }
          return true;
        }
      }
    }
    return false;
  }

  bool has_order(lob::OrderId id) const {
    return ids_.contains(id);
  }

  lob::TopOfBook top() const {
    lob::TopOfBook top;
    if (!bids_.empty()) {
      const auto& [price, queue] = *bids_.rbegin();
      lob::Quantity qty = 0;
      for (const auto& order : queue) {
        qty += order.quantity;
      }
      top.best_bid = lob::BookLevel{price, qty, queue.size()};
    }
    if (!asks_.empty()) {
      const auto& [price, queue] = *asks_.begin();
      lob::Quantity qty = 0;
      for (const auto& order : queue) {
        qty += order.quantity;
      }
      top.best_ask = lob::BookLevel{price, qty, queue.size()};
    }
    return top;
  }

private:
  lob::Quantity available_to_cross(const lob::AddOrder& order) const {
    lob::Quantity available = 0;
    if (order.side == lob::Side::Buy) {
      for (const auto& [price, queue] : asks_) {
        if (order.type == lob::OrderType::Limit && price > order.price) {
          break;
        }
        for (const auto& resting : queue) {
          available += resting.quantity;
        }
      }
    } else {
      for (auto it = bids_.rbegin(); it != bids_.rend(); ++it) {
        if (order.type == lob::OrderType::Limit && it->first < order.price) {
          break;
        }
        for (const auto& resting : it->second) {
          available += resting.quantity;
        }
      }
    }
    return available;
  }

  std::map<lob::Price, std::vector<RefOrder>> bids_;
  std::map<lob::Price, std::vector<RefOrder>> asks_;
  std::unordered_set<lob::OrderId> ids_;
};

void require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "test failure: " << message << '\n';
    std::exit(1);
  }
}

void require_top_equal(const lob::TopOfBook& lhs, const lob::TopOfBook& rhs) {
  require(lhs.best_bid.has_value() == rhs.best_bid.has_value(), "best bid presence mismatch");
  require(lhs.best_ask.has_value() == rhs.best_ask.has_value(), "best ask presence mismatch");
  if (lhs.best_bid && rhs.best_bid) {
    require(lhs.best_bid->price == rhs.best_bid->price, "best bid price mismatch");
    require(lhs.best_bid->total_quantity == rhs.best_bid->total_quantity, "best bid qty mismatch");
    require(lhs.best_bid->order_count == rhs.best_bid->order_count, "best bid count mismatch");
  }
  if (lhs.best_ask && rhs.best_ask) {
    require(lhs.best_ask->price == rhs.best_ask->price, "best ask price mismatch");
    require(lhs.best_ask->total_quantity == rhs.best_ask->total_quantity, "best ask qty mismatch");
    require(lhs.best_ask->order_count == rhs.best_ask->order_count, "best ask count mismatch");
  }
}

void run_core_scenarios() {
  lob::OrderBook book(1'000, 64);
  std::vector<lob::Trade> trades;

  auto report = book.add({1, lob::Side::Sell, 101, 10}, trades);
  require(report.accepted && report.rested, "gtc sell should rest");
  report = book.add({2, lob::Side::Buy, 101, 6}, trades);
  require(report.accepted && report.executed_quantity == 6 && !report.rested, "crossing buy should trade");
  require(book.has_order(1), "residual sell should remain");

  report = book.add({3, lob::Side::Buy, 101, 10, lob::OrderType::Limit, lob::TimeInForce::Ioc}, trades);
  require(report.cancelled && report.executed_quantity == 4 && !book.has_order(3), "ioc remainder should cancel");

  report = book.add({4, lob::Side::Buy, 101, 2, lob::OrderType::Limit, lob::TimeInForce::Fok}, trades);
  require(!report.accepted || report.cancelled, "fok with no liquidity should cancel");

  report = book.add({5, lob::Side::Sell, 105, 8}, trades);
  require(report.rested, "new ask should rest");
  report = book.add({6, lob::Side::Buy, 0, 8, lob::OrderType::Market, lob::TimeInForce::Ioc}, trades);
  require(report.fully_filled && !book.has_order(6), "market order should not rest");
}

void run_reference_parity() {
  lob::OrderBook book(2'000, 512);
  ReferenceBook ref;
  std::vector<lob::Trade> trades_book;
  std::vector<lob::Trade> trades_ref;

  std::vector<lob::AddOrder> orders = {
    {1, lob::Side::Buy, 100, 10},
    {2, lob::Side::Buy, 101, 8},
    {3, lob::Side::Sell, 104, 7},
    {4, lob::Side::Sell, 101, 5},
    {5, lob::Side::Buy, 0, 4, lob::OrderType::Market, lob::TimeInForce::Ioc},
    {6, lob::Side::Sell, 99, 3, lob::OrderType::Limit, lob::TimeInForce::Ioc},
    {7, lob::Side::Buy, 102, 12, lob::OrderType::Limit, lob::TimeInForce::Fok},
    {8, lob::Side::Sell, 103, 11}
  };

  for (const auto& order : orders) {
    const auto report_book = book.add(order, trades_book);
    const auto report_ref = ref.add(order, trades_ref);

    require(report_book.accepted == report_ref.accepted, "accepted mismatch");
    require(report_book.rested == report_ref.rested, "rested mismatch");
    require(report_book.fully_filled == report_ref.fully_filled, "fill mismatch");
    require(report_book.cancelled == report_ref.cancelled, "cancelled mismatch");
    require(report_book.executed_quantity == report_ref.executed_quantity, "executed qty mismatch");
    require(report_book.remaining_quantity == report_ref.remaining_quantity, "remaining qty mismatch");
    require(trades_book.size() == trades_ref.size(), "trade count mismatch");
    require_top_equal(book.top_of_book(), ref.top());
  }

  require(book.cancel(1) == ref.cancel(1), "cancel parity mismatch");
  require(book.cancel(9999) == ref.cancel(9999), "missing cancel parity mismatch");
  require_top_equal(book.top_of_book(), ref.top());
}

}  // namespace

int main() {
  run_core_scenarios();
  run_reference_parity();
  std::cout << "order_book_tests: all checks passed\n";
  return 0;
}

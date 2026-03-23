#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <limits>
#include <optional>
#include <string_view>
#include <vector>

namespace lob {

using OrderId = std::uint64_t;
using Price = std::uint32_t;
using Quantity = std::uint32_t;

enum class Side : std::uint8_t {
  Buy,
  Sell
};

enum class OrderType : std::uint8_t {
  Limit,
  Market
};

enum class TimeInForce : std::uint8_t {
  Gtc,
  Ioc,
  Fok
};

struct AddOrder {
  OrderId id{};
  Side side{};
  Price price{};
  Quantity quantity{};
  OrderType type{OrderType::Limit};
  TimeInForce tif{TimeInForce::Gtc};
};

struct Trade {
  OrderId resting_order_id{};
  OrderId incoming_order_id{};
  Price price{};
  Quantity quantity{};
};

struct BookLevel {
  Price price{};
  Quantity total_quantity{};
  std::size_t order_count{};
};

struct TopOfBook {
  std::optional<BookLevel> best_bid;
  std::optional<BookLevel> best_ask;
};

struct Stats {
  std::uint64_t submitted{};
  std::uint64_t cancelled{};
  std::uint64_t executed{};
  std::uint64_t trades{};
};

struct ExecutionReport {
  bool accepted{false};
  bool rested{false};
  bool fully_filled{false};
  bool cancelled{false};
  Quantity executed_quantity{};
  Quantity remaining_quantity{};
};

class OrderBook {
public:
  OrderBook(Price max_price, std::size_t max_orders);

  ExecutionReport add(const AddOrder& order, std::vector<Trade>& trades);
  bool cancel(OrderId id);
  bool has_order(OrderId id) const noexcept;
  TopOfBook top_of_book() const;
  const Stats& stats() const noexcept;

  Price max_price() const noexcept;
  std::size_t capacity() const noexcept;

private:
  static constexpr std::uint32_t invalid_index = std::numeric_limits<std::uint32_t>::max();

  struct OrderNode {
    OrderId id{};
    Side side{};
    Price price{};
    Quantity quantity{};
    std::uint32_t next{invalid_index};
    std::uint32_t prev{invalid_index};
    bool active{false};
  };

  struct PriceLevel {
    std::uint32_t head{invalid_index};
    std::uint32_t tail{invalid_index};
    Quantity total_quantity{};
    std::uint32_t order_count{};

    bool empty() const noexcept {
      return head == invalid_index;
    }
  };

  bool validate_new_order(const AddOrder& order) const noexcept;
  Quantity available_to_cross(const AddOrder& order) const noexcept;
  std::uint32_t allocate_node();
  void release_node(std::uint32_t index) noexcept;
  void append_to_level(std::uint32_t index) noexcept;
  void unlink_from_level(std::uint32_t index) noexcept;
  void match_buy(OrderNode& incoming, std::vector<Trade>& trades);
  void match_sell(OrderNode& incoming, std::vector<Trade>& trades);
  void consume_level_head(PriceLevel& level, OrderNode& incoming, std::vector<Trade>& trades);
  void update_best_bid_after_remove(Price removed_price) noexcept;
  void update_best_ask_after_remove(Price removed_price) noexcept;
  std::optional<BookLevel> snapshot_level(Side side, Price price) const noexcept;

  Price max_price_{};
  std::vector<PriceLevel> bids_;
  std::vector<PriceLevel> asks_;
  std::vector<OrderNode> orders_;
  std::vector<std::uint32_t> free_list_;
  std::unordered_map<OrderId, std::uint32_t> order_lookup_;
  Price best_bid_{};
  Price best_ask_{};
  Stats stats_{};
};

std::string_view to_string(Side side) noexcept;
std::string_view to_string(OrderType type) noexcept;
std::string_view to_string(TimeInForce tif) noexcept;

}  // namespace lob

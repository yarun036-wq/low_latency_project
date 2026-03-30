#include "risk/pre_trade_risk.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "test failure: " << message << '\n';
    std::exit(1);
  }
}

risk::PreTradeRiskEngine build_engine() {
  risk::PreTradeRiskEngine engine(4, 4, 64);
  engine.configure_symbol(1, {1'000, 2'000, 100}, {100'000});
  engine.configure_symbol(2, {500, 1'000, 50}, {50'000});
  engine.configure_user(7, {300'000'000ULL, 200'000'000ULL, 3});
  return engine;
}

void run_core_scenarios() {
  auto engine = build_engine();

  const auto accepted = engine.check_and_reserve({1, 7, 1, risk::Side::Buy, 100'050, 200});
  require(accepted.accepted, "first order should be accepted");
  require(engine.has_open_order(1), "accepted order should reserve risk");

  const auto duplicate = engine.check_and_reserve({1, 7, 1, risk::Side::Buy, 100'050, 200});
  require(!duplicate.accepted && duplicate.reason == risk::RejectReason::DuplicateOrderId,
          "duplicate ids must reject");

  const auto fat_finger = engine.check_and_reserve({2, 7, 1, risk::Side::Buy, 102'000, 50});
  require(!fat_finger.accepted && fat_finger.reason == risk::RejectReason::PriceBandExceeded,
          "price band should reject");

  const auto too_large = engine.check_and_reserve({3, 7, 1, risk::Side::Buy, 100'000, 5'000});
  require(!too_large.accepted && too_large.reason == risk::RejectReason::MaxOrderQuantityExceeded,
          "max order qty should reject");

  require(engine.on_fill(1, 125, 100'050), "fill should apply");
  require(engine.position(7, 1).value_or(0) == 125, "fill should update position");

  require(engine.cancel(1), "remaining order should cancel");
  require(!engine.has_open_order(1), "cancel should release reservation");
}

void run_limit_scenarios() {
  risk::PreTradeRiskEngine engine(2, 2, 8);
  engine.configure_symbol(1, {1'000, 250, 150}, {100'000});
  engine.configure_user(1, {50'000'000ULL, 30'000'000ULL, 10});

  const auto buy_one = engine.check_and_reserve({10, 1, 1, risk::Side::Buy, 100'000, 100});
  const auto buy_two = engine.check_and_reserve({11, 1, 1, risk::Side::Buy, 100'000, 100});
  const auto buy_three = engine.check_and_reserve({12, 1, 1, risk::Side::Buy, 100'000, 100});
  require(buy_one.accepted && buy_two.accepted, "first two orders should pass");
  require(!buy_three.accepted && buy_three.reason == risk::RejectReason::SymbolPositionLimitExceeded,
          "position cap should reject");

  require(engine.cancel(11), "cancel should free position headroom");
  const auto sell = engine.check_and_reserve({13, 1, 1, risk::Side::Sell, 100'000, 50});
  require(sell.accepted, "sell order should offset exposure");
}

void run_user_guardrails() {
  risk::PreTradeRiskEngine engine(2, 2, 8);
  engine.configure_symbol(1, {1'000, 2'000, 150}, {100'000});
  engine.configure_user(1, {20'000'000ULL, 15'000'000ULL, 2});

  const auto one = engine.check_and_reserve({20, 1, 1, risk::Side::Buy, 100'000, 50});
  const auto two = engine.check_and_reserve({21, 1, 1, risk::Side::Buy, 100'000, 50});
  const auto three = engine.check_and_reserve({22, 1, 1, risk::Side::Buy, 100'000, 50});
  require(one.accepted && two.accepted, "open order capacity should accept first two");
  require(!three.accepted && three.reason == risk::RejectReason::OpenOrderLimitExceeded,
          "open order cap should reject third");

  risk::PreTradeRiskEngine net_engine(2, 2, 8);
  net_engine.configure_symbol(1, {1'000, 5'000, 150}, {100'000});
  net_engine.configure_user(1, {100'000'000ULL, 10'000'000ULL, 10});
  const auto net_reject = net_engine.check_and_reserve({30, 1, 1, risk::Side::Buy, 100'000, 150});
  require(!net_reject.accepted && net_reject.reason == risk::RejectReason::NetExposureLimitExceeded,
          "net exposure cap should reject");
}

}  // namespace

int main() {
  run_core_scenarios();
  run_limit_scenarios();
  run_user_guardrails();
  std::cout << "pre_trade_risk_tests: all checks passed\n";
  return 0;
}

cmake_minimum_required(VERSION 3.20)

project(pre_trade_risk_engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(pre_trade_risk
  src/pre_trade_risk.cpp
)

target_include_directories(pre_trade_risk
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
  target_compile_options(pre_trade_risk PRIVATE /W4 /permissive- /O2 /EHsc)
else()
  target_compile_options(pre_trade_risk PRIVATE -Wall -Wextra -Wpedantic -O3)
endif()

add_executable(pre_trade_risk_bench
  src/pre_trade_risk_main.cpp
)

target_link_libraries(pre_trade_risk_bench PRIVATE pre_trade_risk)
if(MSVC)
  target_compile_options(pre_trade_risk_bench PRIVATE /EHsc)
endif()

add_executable(pre_trade_risk_tests
  tests/pre_trade_risk_tests.cpp
)

target_link_libraries(pre_trade_risk_tests PRIVATE pre_trade_risk)
if(MSVC)
  target_compile_options(pre_trade_risk_tests PRIVATE /EHsc)
endif()

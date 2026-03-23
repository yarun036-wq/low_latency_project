cmake_minimum_required(VERSION 3.20)

project(low_latency_lob VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(order_book
  src/order_book.cpp
)

target_include_directories(order_book
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
  target_compile_options(order_book PRIVATE /W4 /permissive- /O2 /EHsc)
else()
  target_compile_options(order_book PRIVATE -Wall -Wextra -Wpedantic -O3)
endif()

add_executable(order_book_bench
  src/main.cpp
)

target_link_libraries(order_book_bench PRIVATE order_book)
if(MSVC)
  target_compile_options(order_book_bench PRIVATE /EHsc)
endif()

add_executable(order_book_tests
  tests/order_book_tests.cpp
)

target_link_libraries(order_book_tests PRIVATE order_book)
if(MSVC)
  target_compile_options(order_book_tests PRIVATE /EHsc)
endif()

add_executable(order_book_gateway
  src/gateway.cpp
)

target_link_libraries(order_book_gateway PRIVATE order_book)
if(MSVC)
  target_compile_options(order_book_gateway PRIVATE /EHsc)
endif()

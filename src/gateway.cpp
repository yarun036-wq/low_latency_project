#include "lob/order_book.hpp"

#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
using SocketHandle = SOCKET;
static constexpr SocketHandle invalid_socket_handle = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using SocketHandle = int;
static constexpr SocketHandle invalid_socket_handle = -1;
#endif

namespace {

void close_socket(SocketHandle socket_fd) {
#ifdef _WIN32
  closesocket(socket_fd);
#else
  close(socket_fd);
#endif
}

bool send_line(SocketHandle client, const std::string& line) {
  const std::string payload = line + "\n";
  const auto sent = send(client, payload.c_str(), static_cast<int>(payload.size()), 0);
  return sent == static_cast<int>(payload.size());
}

std::string top_to_string(const lob::TopOfBook& top) {
  std::ostringstream out;
  out << "TOP ";
  if (top.best_bid) {
    out << "BID " << top.best_bid->price << ' ' << top.best_bid->total_quantity << ' ';
  } else {
    out << "BID - - ";
  }
  if (top.best_ask) {
    out << "ASK " << top.best_ask->price << ' ' << top.best_ask->total_quantity;
  } else {
    out << "ASK - -";
  }
  return out.str();
}

std::string stats_to_string(const lob::Stats& stats) {
  std::ostringstream out;
  out << "STATS submitted=" << stats.submitted
      << " cancelled=" << stats.cancelled
      << " executed=" << stats.executed
      << " trades=" << stats.trades;
  return out.str();
}

std::string handle_command(lob::OrderBook& book, const std::string& line) {
  std::istringstream input(line);
  std::string command;
  input >> command;

  if (command == "BUY" || command == "SELL") {
    lob::OrderId id = 0;
    lob::Price price = 0;
    lob::Quantity qty = 0;
    std::string type_token = "LIMIT";
    std::string tif_token = "GTC";
    input >> id >> price >> qty >> type_token >> tif_token;
    if (!input) {
      return "ERR malformed order";
    }

    const auto side = command == "BUY" ? lob::Side::Buy : lob::Side::Sell;
    const auto type = type_token == "MARKET" ? lob::OrderType::Market : lob::OrderType::Limit;
    lob::TimeInForce tif = lob::TimeInForce::Gtc;
    if (tif_token == "IOC") {
      tif = lob::TimeInForce::Ioc;
    } else if (tif_token == "FOK") {
      tif = lob::TimeInForce::Fok;
    }

    std::vector<lob::Trade> trades;
    const auto report = book.add({id, side, price, qty, type, tif}, trades);

    std::ostringstream out;
    out << "OK accepted=" << report.accepted
        << " rested=" << report.rested
        << " filled=" << report.fully_filled
        << " cancelled=" << report.cancelled
        << " executed=" << report.executed_quantity
        << " remaining=" << report.remaining_quantity
        << " trades=" << trades.size();
    return out.str();
  }

  if (command == "CANCEL") {
    lob::OrderId id = 0;
    input >> id;
    return book.cancel(id) ? "OK cancelled=1" : "OK cancelled=0";
  }

  if (command == "TOP") {
    return top_to_string(book.top_of_book());
  }

  if (command == "STATS") {
    return stats_to_string(book.stats());
  }

  if (command == "HELP") {
    return "BUY|SELL <id> <price> <qty> [LIMIT|MARKET] [GTC|IOC|FOK] | CANCEL <id> | TOP | STATS | QUIT";
  }

  if (command == "QUIT") {
    return "BYE";
  }

  return "ERR unknown command";
}

}  // namespace

int main(int argc, char** argv) {
  const int port = argc > 1 ? std::stoi(argv[1]) : 9090;

#ifdef _WIN32
  WSADATA wsa_data{};
  if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
    std::cerr << "failed to initialize winsock\n";
    return 1;
  }
#endif

  SocketHandle server = socket(AF_INET, SOCK_STREAM, 0);
  if (server == invalid_socket_handle) {
    std::cerr << "failed to create socket\n";
    return 1;
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(static_cast<std::uint16_t>(port));

  int reuse = 1;
  setsockopt(server, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

  if (bind(server, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    std::cerr << "bind failed\n";
    close_socket(server);
    return 1;
  }

  if (listen(server, 8) != 0) {
    std::cerr << "listen failed\n";
    close_socket(server);
    return 1;
  }

  std::cout << "order_book_gateway listening on port " << port << '\n';
  lob::OrderBook book(2'000'000, 1'000'000);

  for (;;) {
    SocketHandle client = accept(server, nullptr, nullptr);
    if (client == invalid_socket_handle) {
      continue;
    }

    send_line(client, "WELCOME order_book_gateway");
    send_line(client, "Type HELP for commands");

    std::string buffer;
    char chunk[1024];

    for (;;) {
      const auto received = recv(client, chunk, sizeof(chunk), 0);
      if (received <= 0) {
        break;
      }

      buffer.append(chunk, chunk + received);
      std::size_t pos = 0;
      while ((pos = buffer.find('\n')) != std::string::npos) {
        std::string line = buffer.substr(0, pos);
        buffer.erase(0, pos + 1);
        if (!line.empty() && line.back() == '\r') {
          line.pop_back();
        }

        const std::string response = handle_command(book, line);
        send_line(client, response);
        if (response == "BYE") {
          close_socket(client);
          client = invalid_socket_handle;
          break;
        }
      }

      if (client == invalid_socket_handle) {
        break;
      }
    }

    if (client != invalid_socket_handle) {
      close_socket(client);
    }
  }

  close_socket(server);
#ifdef _WIN32
  WSACleanup();
#endif
  return 0;
}

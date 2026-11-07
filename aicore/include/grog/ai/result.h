#pragma once
#include <string>
#include <utility>
#include <variant>

namespace grog::ai {

enum class ErrorKind { Config, Network, Auth, RateLimit, Server, Timeout, BadResponse, Validation, Io, Unsupported };

const char* errorKindName(ErrorKind kind);

struct Error {
  ErrorKind kind;
  std::string message;  // player/developer safe: never contains API keys
};

// Expected failures travel as values, not exceptions.
template <class T>
class Result {
 public:
  Result(T value) : data_(std::move(value)) {}
  Result(Error error) : data_(std::move(error)) {}

  bool ok() const { return std::holds_alternative<T>(data_); }
  explicit operator bool() const { return ok(); }

  T& value() { return std::get<T>(data_); }
  const T& value() const { return std::get<T>(data_); }
  const Error& error() const { return std::get<Error>(data_); }

 private:
  std::variant<T, Error> data_;
};

}  // namespace grog::ai

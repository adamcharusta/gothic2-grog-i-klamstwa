#include <grog/ai/result.h>

namespace grog::ai {

const char* errorKindName(ErrorKind kind) {
  switch(kind) {
    case ErrorKind::Config: return "Config";
    case ErrorKind::Network: return "Network";
    case ErrorKind::Auth: return "Auth";
    case ErrorKind::RateLimit: return "RateLimit";
    case ErrorKind::Server: return "Server";
    case ErrorKind::Timeout: return "Timeout";
    case ErrorKind::BadResponse: return "BadResponse";
    case ErrorKind::Validation: return "Validation";
    case ErrorKind::Io: return "Io";
    case ErrorKind::Unsupported: return "Unsupported";
  }
  return "Unknown";
}

}  // namespace grog::ai

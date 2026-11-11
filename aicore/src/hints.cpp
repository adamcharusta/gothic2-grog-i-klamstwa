#include <grog/ai/hints.h>

namespace grog::ai {

std::string playerHint(ErrorKind kind, Language lang) {
  const bool pl = lang == Language::Pl;
  switch(kind) {
    case ErrorKind::Auth:
      return pl ? "Klucz API został odrzucony - sprawdź api_key w GrogAI.ini."
                : "The API key was rejected - check api_key in GrogAI.ini.";
    case ErrorKind::RateLimit:
      return pl ? "Dostawca AI odrzuca zapytania (limit albo brak środków na koncie)."
                : "The AI provider refuses requests (rate limit or no credit on the account).";
    case ErrorKind::Network:
      return pl ? "Brak połączenia z dostawcą AI - sprawdź internet i base_url."
                : "Cannot reach the AI provider - check the internet connection and base_url.";
    case ErrorKind::Timeout:
      return pl ? "Dostawca AI odpowiada zbyt wolno - spróbuj później albo zwiększ timeout_s."
                : "The AI provider is too slow - try later or raise timeout_s.";
    case ErrorKind::Server:
      return pl ? "Dostawca AI ma awarię - spróbuj później." : "The AI provider has an outage - try later.";
    case ErrorKind::Config:
      return pl ? "Błąd w GrogAI.ini: model lub ustawienia nie pasują do dostawcy."
                : "GrogAI.ini error: the model or settings don't fit the provider.";
    default:
      return {};
  }
}

}  // namespace grog::ai

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

std::string voiceHint(ErrorKind kind, Language lang) {
  const bool pl = lang == Language::Pl;
  switch(kind) {
    case ErrorKind::Auth:
      return pl ? "Klucz głosu został odrzucony - sprawdź api_key w sekcji [tts] w GrogAI.ini. Zostają same napisy."
                : "The voice API key was rejected - check api_key in the [tts] section of GrogAI.ini. Subtitles only.";
    case ErrorKind::RateLimit:
      return pl ? "Limit głosu wyczerpany (limit albo brak środków) - zostają same napisy."
                : "Voice limit reached (rate limit or no credit) - subtitles only.";
    case ErrorKind::Network:
    case ErrorKind::Timeout:
    case ErrorKind::Server:
      return pl ? "Głos jest niedostępny - zostają same napisy." : "Voice unavailable - subtitles only.";
    case ErrorKind::BadResponse:
      return pl ? "Serwer głosu musi zwracać WAV (response_format=wav) - zostają same napisy."
                : "The voice server must return WAV (response_format=wav) - subtitles only.";
    case ErrorKind::Config:
      return pl ? "Serwer głosu odrzuca żądanie - sprawdź model, base_url i voices w sekcji [tts] pliku GrogAI.ini. "
                  "Zostają same napisy."
                : "The voice server rejects the request - check model, base_url and voices in the [tts] section of "
                  "GrogAI.ini. Subtitles only.";
    default:
      return {};
  }
}

}  // namespace grog::ai

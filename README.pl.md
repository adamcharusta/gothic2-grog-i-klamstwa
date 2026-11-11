# Grog i kłamstwa

*[English version](README.md)*

Mini-kampania do **Gothic II: Noc Kruka**, działająca na forku [OpenGothic](https://github.com/Try/OpenGothic).
Akcja toczy się w obozie piratów w Jarkendarze. Greg zgubił swój ulubiony kufel, a ty musisz go odnaleźć.

Czym się wyróżnia: fabułę prowadzi **Mistrz Gry sterowany przez AI**. Nie wybierasz gotowych kwestii, tylko **wpisujesz własne**,
a piraci odpowiadają zgodnie ze swoim charakterem. Każda rozgrywka losuje inne tło i innego winowajcę.

> Status: wczesna faza prac. Paczki budują się automatycznie: bety z `develop`, wydania z `master`.

## Wymagania

- Legalna kopia **Gothic II: Noc Kruka** (np. Steam lub GOG). Mod nie zawiera oryginalnych plików gry.
- Windows 10/11 x64 i karta graficzna z obsługą Vulkan lub DirectX 12 (wymaganie OpenGothic).
- Klucz API do wybranego dostawcy AI (patrz niżej).

## Instalacja

**Instalator:** uruchom `GrogIKlamstwa-<wersja>-setup.exe`, wskaż folder Gothic II
(na Steamie zwykle `C:\Program Files (x86)\Steam\steamapps\common\Gothic II`) i uruchom grę z utworzonego skrótu.

**Zip:** rozpakuj w dowolnym miejscu, wpisz ścieżkę do folderu Gothic II w pliku `gothic_path.txt` obok `Start-GrogIKlamstwa.bat`,
a potem uruchom `Start-GrogIKlamstwa.bat`.

## Konfiguracja AI

Skopiuj `GrogAI.example.ini` jako `GrogAI.ini` i wpisz dostawcę, model i klucz API. Nigdy nie udostępniaj pliku `GrogAI.ini`.
Połóż go obok `Start-GrogIKlamstwa.bat` (instalacja z zipa) albo w `%APPDATA%\GrogIKlamstwa\` (zalecane przy instalatorze, bo do Program Files nie da się pisać). Logi trafiają do `logs\` obok moda albo do `%APPDATA%\GrogIKlamstwa\logs`.

W grze: podejdź do pirata i zagadaj jak zwykle. Wpisz kwestię i naciśnij Enter; **Enter na pustej linii**, Esc albo **KONIEC** kończy rozmowę.

### Jak zdobyć klucz API

Za AI płacisz bezpośrednio jego dostawcy. Cała sesja na małym modelu kosztuje grosze: pierwszy playtest na `gpt-4.1-mini` kosztował ok. 0,01 $.
Większość dostawców wymaga podpięcia karty albo doładowania konta, nawet na małą kwotę.

Klucz wklej w `api_key =`, bez cudzysłowów. Możesz też ustawić zmienną środowiskową `GROGAI_API_KEY` i zostawić `api_key` puste.
`base_url` trzeba podać u każdego dostawcy poza Anthropic.

**Anthropic (Claude):** klucz wygenerujesz na https://console.anthropic.com/settings/keys (Settings → API keys).
```ini
provider = anthropic
model = claude-sonnet-5-5
```

**OpenAI:** klucz wygenerujesz na https://platform.openai.com/api-keys (zaczyna się od `sk-proj-`). Listę modeli dostępnych w projekcie znajdziesz w Settings → Project → Limits.
```ini
provider = openai_compatible
base_url = https://api.openai.com/v1
model = gpt-4.1-mini
```
Modele rozumujące (`gpt-5*`, `o3`, `o4-mini`) też działają, ale odpowiadają wolniej i każda kwestia kosztuje więcej.

**DeepSeek:** klucz wygenerujesz na https://platform.deepseek.com/api_keys.
```ini
provider = openai_compatible
base_url = https://api.deepseek.com
model = deepseek-chat
```

**OpenRouter** (wiele modeli na jednym kluczu): klucz wygenerujesz na https://openrouter.ai/keys. Nazwy modeli mają postać `dostawca/model`.
```ini
provider = openai_compatible
base_url = https://openrouter.ai/api/v1
model = openai/gpt-4.1-mini
```

**Google Gemini:** klucz wygenerujesz na https://aistudio.google.com/apikey.
```ini
provider = openai_compatible
base_url = https://generativelanguage.googleapis.com/v1beta/openai
model = gemini-2.5-flash
```

**Ollama** (działa na twoim komputerze, za darmo, bez klucza): zainstaluj Ollamę, uruchom `ollama pull qwen2.5:14b` i nie wyłączaj Ollamy podczas gry.
```ini
provider = openai_compatible
base_url = http://localhost:11434/v1
model = qwen2.5:14b
api_key =
```

**LM Studio** (działa na twoim komputerze, za darmo, bez klucza): wczytaj model i włącz lokalny serwer w zakładce Developer.
```ini
provider = openai_compatible
base_url = http://localhost:1234/v1
model = <nazwa modelu pokazana w LM Studio>
api_key =
```

Dwa dodatkowe ustawienia:
- `director_model`: tańszy model tego samego dostawcy dla Mistrza Gry.
- `director_interval_s`: jak często Mistrz Gry wkracza do akcji (domyślnie co 240 sekund gry).

## Głos NPC (opcjonalnie)

Piraci mogą mówić swoje odpowiedzi na głos. Głos jest **domyślnie wyłączony**; bez niego dostajesz same napisy, tak jak dotąd.
Konfiguruje się go w sekcji `[tts]` pliku `GrogAI.ini` (wzór w `GrogAI.example.ini`). Kwestie bohatera nie są czytane na głos.

Przy włączonym głosie odpowiedź NPC pojawia się dopiero, gdy audio jest gotowe, więc trwa to dłużej: około sekundy więcej z OpenAI,
kilka sekund z lokalnym serwerem na procesorze. Napis trwa tyle, co dźwięk, a Esc przerywa głos. Szybkie wczytanie (quickload)
albo nowa gra w trakcie rozmowy czeka na trwające zapytania o głos, zanim się załaduje.

### Głosy OpenAI

Jeśli w `[ai]` masz już klucz OpenAI, dodaj tę sekcję (klucz z `[ai]` jest użyty ponownie, gdy `base_url` jest taki sam):

```ini
[tts]
provider = openai_compatible
base_url = https://api.openai.com/v1
model = gpt-4o-mini-tts
```

Każdy pirat dostaje własny głos OpenAI i opis stylu (zachrypnięty pijany stary pirat, nerwowy młody i tak dalej).
Piraci mający tego samego oryginalnego aktora dubbingowego dostają też ten sam głos OpenAI. Koszt: `mod/data/pricing.json` zakłada
około 0,015 $ za minutę audio (`tts.gpt-4o-mini-tts.usd_per_minute`). To szacunek: sprawdź aktualny cennik OpenAI.
Można też użyć lokalnego serwera zgodnego z OpenAI (na przykład Kokoro-FastAPI): wpisz jego adres w `base_url`, a w `model`
model, który ten serwer oferuje. Piraci proszą o nazwy głosów OpenAI (`onyx`, `ash`, ...), więc serwer musi je przyjmować
(albo mapować na własne głosy). Kokoro ma inne nazwy głosów i nie ma polskiego głosu, więc najpierw sprawdź jego dokumentację.

### Oryginalne głosy (lokalne klonowanie, dla zaawansowanych)

Z `voices = original` piraci mówią głosami sklonowanymi z oryginalnego polskiego dubbingu Gothica II. Te głosy mówią
**tylko po polsku** (wygenerowana konfiguracja ma `language: pl`), więc używaj tego trybu z `[game] language = pl`.
Klonowanie odbywa się **wyłącznie na Twoim komputerze, z Twojej legalnej kopii gry**. Mod nie zawiera żadnych nagrań głosu. Chmurowe usługi klonowania
nie są używane ani wspierane. Głosy należą do oryginalnych aktorów dubbingowych: **nigdy nie udostępniaj ani nie publikuj
wyciągniętych próbek ani sklonowanych kwestii.**

Potrzebujesz lokalnego serwera mowy zgodnego z OpenAI, który przypisuje nazwę głosu do referencyjnego pliku WAV i go klonuje
(robi to XTTS-v2). Poniższy przykład używa [openedai-speech](https://github.com/matatonic/openedai-speech). Uwaga: ten projekt
jest zarchiwizowany (od 2026-01-04), a jego autor nazywa go w większości przestarzałym. Działa każdy serwer z endpointem
`/v1/audio/speech`, który przypisuje nazwę głosu do referencyjnego WAV; openedai-speech jest po prostu tym, którego konfigurację
zapisuje narzędzie poniżej. Jego referencyjne audio jest ograniczone do 30 sekund.

1. Wyciągnij próbki z gry (wymaga Pythona 3). `--out` to folder `voices` serwera:

   ```
   python tools/voices/extract_voices.py --gothic "C:\Program Files (x86)\Steam\steamapps\common\Gothic II" --out "<folder openedai-speech>\voices"
   ```

   Narzędzie czyta `Data\Speech*.vdf`, zapisuje `<pirat>.wav` dla każdego z 16 piratów (po około 20 s, zmienisz to przez `--seconds`)
   oraz `voice_to_speaker.yaml`. Kod wyjścia 0 = wszyscy 16 piratów, 1 = błędne argumenty, 2 = brak `Data\Speech*.vdf`,
   3 = część piratów bez próbki (lista na stderr). Przy kodzie 3 próbki pozostałych piratów i plik yaml i tak są zapisane, ale
   piratów bez próbki nie ma w yaml, więc serwer nie może ich zagrać: gra pokazuje podpowiedź o głosie i wyłącza głos do końca
   sesji. Napraw wyciąganie próbek, zanim zagrasz w tym trybie. Angus i Hank nie mają w oryginale własnych kwestii, więc
   używają ogólnego głosu swojego aktora i brzmią tak samo.
2. Skopiuj `voice_to_speaker.yaml` do `config\voice_to_speaker.yaml` serwera (albo scal z własnym) i uruchom serwer.
3. Ustaw w `GrogAI.ini`:

   ```ini
   [tts]
   provider = openai_compatible
   base_url = http://localhost:8000/v1
   model = tts-1-hd
   voices = original
   timeout_s = 60
   ```

   Użyj portu, na którym nasłuchuje Twój serwer. Zalecana jest karta graficzna: XTTS na procesorze potrzebuje
   kilku sekund na kwestię, a każda odpowiedź na nią czeka. Na wolnych komputerach zostaw `timeout_s = 60`.

Po ponownym wyciągnięciu próbek albo zmianie serwera usuń folder `tts_cache` (obok moda albo
`%APPDATA%\GrogIKlamstwa\tts_cache`): są w nim stare pożegnania i kwestie awaryjne starymi głosami.

### Problemy z głosem

Gdy głos nie działa, gra pokazuje jedną podpowiedź (zły klucz `[tts]`, brak środków, serwer niedostępny, odrzucone żądanie,
serwer nie zwraca 16-bitowego WAV) i toczy się dalej z samymi napisami. Po odrzuconym kluczu lub żądaniu albo po dwóch nieudanych
zapytaniach z rzędu głos zostaje wyłączony do wczytania zapisu lub nowej gry, żeby martwy serwer nie spowalniał każdej
odpowiedzi. Każde zapytanie trafia jako linia `tts` do `logs\session-<seed>.jsonl` (czas, liczba znaków, długość audio, koszt).
`summary-<seed>.json` zawiera `tts_requests`, `tts_errors`, `tts_audio_s`, `tts_cost_usd` i `tts_latency_avg_ms`.
Stałe kwestie, jak pożegnania i kwestie awaryjne, są zapisywane w `tts_cache\`.

## Przetestowane modele

| Dostawca | Model | Wynik |
|----------|-------|-------|
| OpenAI | `gpt-4.1-mini` | Playtest 1 (beta.5): rozmowy działają, ok. 0,01 $ za sesję. Komendy czasem miały zły format (poprawione w beta.6). |
| Pozostali | — | Jeszcze nie testowane |

Słabsze i małe modele lokalne częściej źle wydają komendy gry: brakuje wpisów w dzienniku, bójki nie startują.
Łatwiej też wychodzą z roli. Jeśli piraci wypadają płasko, spróbuj większego modelu.

## Rozwiązywanie problemów

Gdy coś jest nie tak z AI, gra wyświetla jednolinijkową podpowiedź:
- *Klucz API został odrzucony*: `api_key` jest zły albo unieważniony.
- *Dostawca AI odrzuca zapytania*: przekroczony limit zapytań albo brak środków na koncie (doładuj konto albo podepnij płatność).
- *Brak połączenia z dostawcą AI*: nie ma internetu albo `base_url` jest zły. Przy Ollamie i LM Studio sprawdź, czy program jest włączony.
- *Błąd w GrogAI.ini*: dostawca nie ma modelu o tej nazwie albo ścieżka w `base_url` jest zła.
- *Brak pliku GrogAI.ini*: połóż plik obok `Start-GrogIKlamstwa.bat` albo w `%APPDATA%\GrogIKlamstwa\`.

Jeśli odpowiedź AI się nie powiedzie, pirat mówi gotową kwestię, więc gra toczy się dalej. Szczegóły znajdziesz w `logs\session-<seed>.jsonl`.
Klucz API nigdy tam nie trafia.

## Logi benchmarku

Każda sesja zapisuje dwa pliki:
- `logs\session-<seed>.jsonl`: każde zapytanie do AI z czasem odpowiedzi, liczbą tokenów i kosztem.
- `logs\summary-<seed>.json`: zakończenie, czas gry, koszt, opóźnienia i odsetek odrzuconych komend.

Żeby porównać sesje albo dostawców, uruchom (wymaga Pythona 3):

```
python tools/bench/aggregate.py logs
```

## Licencja

MIT, patrz [LICENSE](LICENSE). Komponenty zewnętrzne: [packaging/THIRD_PARTY.md](packaging/THIRD_PARTY.md).

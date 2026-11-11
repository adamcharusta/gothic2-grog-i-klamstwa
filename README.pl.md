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

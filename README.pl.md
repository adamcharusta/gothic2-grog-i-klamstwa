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

Wkrótce: instrukcje krok po kroku dla Anthropic (Claude), OpenAI, DeepSeek, OpenRouter, Google Gemini i modeli lokalnych (Ollama, LM Studio).

## Licencja

MIT, patrz [LICENSE](LICENSE). Komponenty zewnętrzne: [packaging/THIRD_PARTY.md](packaging/THIRD_PARTY.md).

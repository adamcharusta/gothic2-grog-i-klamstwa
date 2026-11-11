# 02 — Characters, hero, world

Tags: **[U]** user · **[P]** proposal · **[B]** builder.

## Cast

[U] Only characters from the original Jarkendar pirate camp, plus pirates normally not in it:
**Greg, Skip, Angus, Hank, Malcolm**. [P] All alive and present in or right next to the camp
(in the original, Angus, Hank and Malcolm die, and Greg and Skip arrive later).

Working list (verify against original NotR scripts [B]):
Greg (captain), Francis (stands in for Greg, lives in his hut), Garett (quartermaster), Samuel (distiller, grog),
Alligator Jack (hunter), Henry (palisade guard), Morgan (lazy, leads the "scouts"), Bill, Brandon, Matt,
Owen, Malcolm, Skip, Angus, Hank, Bones, plus any unnamed camp pirates.

[B] Verify the full list by NPC instances in scripts (e.g. `PIR_xxxx_Addon_*`) and record it in the mod data.

[B] Starting places (checked in the original `GOTHIC.DAT`, playtest 2): all cast members are inserted by the original
startup, but some start far away and are not simulated there, so they never walk to the player. Malcolm stands alone at
the lumber camp outside the palisade (his partner Owen drinks on the beach), Angus and Hank stand in the waterhole cave,
and Alligator Jack waits on the path to the camp. `cast.json` `routine` gives them a camp routine on a new game (Owen and
Malcolm get their original lumberjack pair routines `RTN_PRESTART_1367/1368`). Each cast member also has a `hangout`
text (PL/EN) that goes into the actor and director prompts, so the AI sends the player to the right place.

[P] No bandits, Water Mages, far-away monsters or other NPCs. Exception: monsters spawned by events (see [03](03-game-master.md)).

## Character cards (data, not code) [P]

Each NPC has a card file (e.g. `mod/data/npc/greg.json` + per-language texts) with:

- identity: name, script instance, camp role, rank;
- personality and **speech style** with sample lines in PL/EN (key against the "chatbot" anti-goal);
- lore knowledge: what they know about the camp, Greg, the world; what they do NOT know;
- relationships with other pirates (likes, hates, owes money…);
- behaviour rules: can they be persuaded, by what, do they take bribes, how they react to being beaten;
- items they can own or hand over.

[B] The builder writes card texts based on the original NotR dialogues and lore. Original lines are the best style reference.

## Hero

- [U] The player is the **Nameless Hero in pirate clothes** (NotR light pirate outfit).
- [U] **Mid level**: can take on anyone in a fist fight but isn't overpowered. The Matt fight should be even.
- [B] Exact stats and skills, tuned by playtesting. [P] Starting point: level ~10–12, basic one-handed, simple weapon.
- [P] The hero is accepted in the camp: recently joined, Greg knows him. Details in the world card.

## Fighting

[P] Brawls are original Gothic duels to knockdown, not death. Code reports the result (win/loss, who vs whom) as an event for the GM,
which adapts the scene. [P] Story-critical NPCs cannot die (immortal, or knockdown instead of death). Pirates only knock the player down.
Monsters can kill the player.

## Map

- [U] The camp and **a few metres** beyond the palisade.
- [P] Do not cut the world file. Load the original Jarkendar world (ADDONWORLD.ZEN), remove NPCs and monsters outside the cast,
  close the area with **invisible barriers** or a zone that turns the hero back with a remark ("Nothing for me out there").
  Technique is [B].
- [P] The game starts directly in the camp (no intro, no portal), at the time of day set by the scenario.
- [B] Strip objects and NPCs from the world startup scripts so that original quests and dialogues do not fire in the background.
  The pirates' original **INFO dialogues must be disabled** (the AI conversation replaces them). Original daily routines (TA_*) stay.

## Lore

[P] Shared lore base (`mod/data/lore/`): the Gothic 2 world during NotR from the pirates' perspective (Jarkendar, Raven's bandits,
Water Mages, Khorinis, Greg's ship, camp rules, grog). The GM and characters receive **relevant excerpts**, never all of it.
[B] The builder writes it from the game. Limited to knowledge pirates could realistically have.

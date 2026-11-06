# 01 — Story, scenario rolling, endings

Tags: **[U]** user · **[P]** proposal · **[B]** builder.

## Main storyline

- [U] At the start Greg is furious and orders the Nameless Hero to find his favourite tankard.
- [U] The investigation is a chain of conversations: the player asks, gets a lead, and a quest log entry appears
  (e.g. "Talk to Skip"). Example flow given by the user:
  Greg says he last saw Skip near his hut → Skip says Greg gave him the tankard to wash, he put it with the other dishes,
  now it's gone, but he saw Matt drinking from a similar one → Matt resists; the player can **persuade** him or **beat him up**.
- [U] Main ending: the player returns the tankard to Greg and receives a **key to a reward chest**,
  which contains the **captain's outfit**.
- [U] The player may also fail to find it, or **deceive** Greg. The outcome depends on the player.
- [U] The GM behaves like a pen-and-paper GM: a fixed spine, improvised route. Beating someone up instead of persuading
  them never blocks progress, it changes the scene.

## Scenario rolling at game start

[U] Each run has a different background and culprit. [P] The **game code** rolls it, not the AI.
The AI receives the result as a secret it must respect.

Rolled:

1. **Scenario type** [U]:
   - **Lost at dice.** Drunk Greg gambled the tankard away. Roll who he played with. That pirate owns it fairly and won't give it back.
   - **Lost it.** Roll a place within the playable area. Someone found it, or it's still lying there.
   - **Stolen.** Roll the thief and motive (greed, revenge on Greg, a bet, accident…).
   - **It's in his hut and he forgot.** Comedy variant: everyone denies everything, and the tankard was at Greg's all along.
2. **Details**: who, where, why. Culprit pool: every pirate **except Greg** [P]
   (in the "in his hut" variant Greg is the unwitting "culprit").
3. **Roles of other NPCs** [P]: *witness* (knows a true fragment), *liar* (has a reason to make things up, e.g. hides
   something of their own), *gossip* (repeats hearsay, sometimes wrongly), *knows nothing*.
4. **Seed** [P] is logged so a benchmark run can be repeated on the same scenario.

### Solvability guarantee [P]

The generator builds a **clue graph**: at least **two independent paths** lead from Greg to the tankard's holder or location.
If the player "burns" one (e.g. beats up a witness who then sulks), another remains. The GM may add false leads and new paths
but **cannot remove the truth**. Code checks that a reachable path still exists; if none does, it forces the GM to introduce one
(e.g. a gossip at the campfire lets something slip).

[B] Scenario template format, number of clues, exact algorithm.

## Story items [P]

- **Greg's tankard**: unique story item.
- **Look-alike tankards**: ordinary, similar visual, different name/description. For confusion and for fooling Greg.
- **Reward chest key** and **chest** (e.g. in Greg's hut).
- **Captain's outfit**: the existing NotR pirate captain armour.

## Endings [P]

Code detects the ending moment; the GM plays the scene and decides the reaction within the rules.

1. **Real tankard returned.** Greg gives the key → chest → captain's outfit. Epilogue depends on whether and how the player exposed the culprit.
2. **Deception.** A look-alike tankard, or blaming an innocent. Greg may fall for it or see through it, depending on evidence,
   traces left and what others tell him. The GM decides, but code supplies the facts (which tankard was actually given, who knows what).
3. **Failure / giving up.** The player tells Greg they give up (or a time limit expires, [B] whether one exists).
   Greg is furious, no key. Possibly a "consolation" bribe from the culprit for keeping quiet.

After the ending: a summary screen (scenario, who did it, benchmark stats) and "new game" with a fresh roll.

## Side quests and events

[U] The GM can add random events, which also test whether the AI can manipulate the game in real time.
Details in [03-game-master.md](03-game-master.md). [P] Examples: a swampshark attacks Alligator Jack, a drinking session at the
campfire, two pirates brawling, a pirate approaching the player with a small request. Side threads may or may not tie into the investigation.

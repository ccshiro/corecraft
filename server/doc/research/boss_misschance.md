NOTE: This has been disproven for TBC by parsing combat logs and doing
statistical analysis (all uses logs were in 2.3 and 2.4).

## Subject
In this document I intend to propose that the assumption that a NPC labeled BOSS
attacking you is considered a character_level+3 NPC is incorrect. Instead, I
propose that NPCs with the BOSS tag have a completely different effect on your
parry/block/dodge.

The current understanding is that your chance to cause a BOSSs melee swing to
miss you is:

Miss:  5% + Defense Miss chance (the latter does not exist in 6.2)
Dodge: Your Dodge - 0.6%
Parry: Your Parry - 0.6%
Block: Your Block - 0.6% (subject to second roll, see below)

I propose that this understanding is incorrect, and intend to provide the data
to disprove it.

## Data
Character: Level 100 warrior
Target: Golemagg the Incinerator Level ?? (level 60 molten core)

Rates for warrior:
Dodge: 5.00%  (0 rating)
Parry: 15.13% (515 rating, adds 3.18% before DR)
Block: 33.97%

Golemagg Hits on Warrior: 5000
Miss: 0 (0%)
Dodge: 32 (0.64%)
Parry: 522 (10.44%)
Block: 1329 (26.58%)

## Theory
I propose the following two theory statements.
1. Boss NPCs reduce your chance to dodge, parry and block by 4%.
2. Boss NPCs do not have the innate 5% miss chance that other attackers have.

Note on point 2: 
I was able to confirm that bosses can indeed still miss, just lacking the
built-in base 5% miss chance. That they can miss was confirmed on Prince
Malchezaar in second phase, where he equips two weapons and is considered
dual-wielding.

## Note
This data was gathered in Warlord of Draenor (6.2.2). In MoP they moved Block
out of the Attack Table to a roll that happens after the attack table (source:
http://us.battle.net/wow/en/forum/topic/2489160859). Which would make the
chance to block after the actual dodge and parry on the fight:

Total Block Chance = Block * (1.0 - 0.0064 - 0.1044) ~= 0.3021

Given the 4% reduction upon this we would expect to see 30.21%-4% = 26.21%
blocked attacks.

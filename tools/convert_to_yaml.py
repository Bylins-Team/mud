#!/usr/bin/env python3
# -*- coding: koi8-r -*-
"""
Convert old MUD world format files to YAML format.
Created by Claude Code, 2026.01.12

Usage:
    python3 convert_to_yaml.py --input lib.template/world --output lib/world --type all
    python3 convert_to_yaml.py --input lib.template/world/mob/1.mob --output lib/world/mob/100.yaml --type mob
"""

import argparse
import os
import re
import sys
from pathlib import Path

# Global name registries for cross-references
ROOM_NAMES = {}    # vnum -> name
MOB_NAMES = {}     # vnum -> name
OBJ_NAMES = {}     # vnum -> name
TRIGGER_NAMES = {} # vnum -> name
ZONE_NAMES = {}    # vnum -> name

# Action flags (MOB_x) - from constants.cpp action_bits[]
ACTION_FLAGS = [
    "kSpec", "kSentinel", "kScavenger", "kIsNpc", "kAware", "kAggressive",
    "kStayZone", "kWimpy", "kAggressive_Mob", "kMemory", "kHelper", "kNoCharm",
    "kNoSummoned", "kNoSleep", "kNoBash", "kNoBlind", "kNoTrack", "kFireCreature",
    "kAirCreature", "kWaterCreature", "kEarthCreature", "kRacing", "kMounting",
    "kProtected", "kSwimming", "kFlying", "kScrStay", "kNoTerrainAttack",
    "kNoFear", "kNoMagicTerrainAttack",
    # Plane 1
    "kFreemaker", "kProgrammedLootGroup", "kIgnoresFear", "kClone", "kCorpse",
    "kLooting", "kTutelar", "kMentalShadow", "kSummoner", "kNoSilence",
    "kNoHolder", "kDeleted"
]

# Room flags (room_bits[])
ROOM_FLAGS = [
    "kDarked", "kDeathTrap", "kNoMob", "kIndoors", "kPeaceFul", "kSoundproof",
    "kNoTrack", "kNoMagic", "kTunnel", "kNoTeleportIn", "kGodRoom", "kHouse",
    "kHouseCrash", "kAtrium", "kOlc", "kBfsMark", "kMoOnlyRoom", "kNoSummon",
    "kNoTeleportOut", "kNoBattle", "kSlowDeathTrap", "kArena", "kNoWeather",
    "kDeathIceTrap", "kNoPk", "kIceTrap", "kNoRelocateIn", "kForestTrap",
    "kArenaTrap", "kNoSummonOut",
    # Plane 1
    "kExchangeRoom", "kNoArmor", "kDecayedDeathTrap", "kDominationArena",
    "kHoleInTheSky", "kAutoQuest", "kNoBirdArrival"
]

# Object types (item_types[])
OBJ_TYPES = [
    "kUndefined", "kLightSource", "kScroll", "kWand", "kStaff", "kWeapon",
    "kTreasure", "kArmor", "kPotion", "kOther", "kTrash", "kContainer",
    "kNote", "kLiquidContainer", "kKey", "kFood", "kMoney", "kPen",
    "kBoat", "kFountain", "kIngredient", "kMagicIngredient", "kCraftMaterial",
    "kBandage", "kLingredient", "kSpellbook", "kMap", "kMagicArrow",
    "kMagicContaner", "kCraftbook", "kMedal", "kEnchant"
]

# Extra flags (extra_bits[])
EXTRA_FLAGS = [
    "kGlow", "kHum", "kNoRent", "kNoDonate", "kNoInvis", "kInvisible",
    "kMagic", "kNoDrop", "kBless", "kAntiGood", "kAntiEvil", "kAntiNeutral",
    "kNoSell", "kAntiMage", "kAntiCleric", "kAntiThief", "kAntiWarrior",
    "kAntiAssassin", "kAntiGuard", "kAntiPaladine", "kAntiRanger",
    "kAntiPolymorphed", "kAntiMerchant", "kAntiMono", "kAntiPoly",
    "kAntiVigilant", "kAntiMale", "kAntiFemale", "kTwoHands", "k1InLoot",
    # Plane 1
    "kTransformed", "kACS1", "kACS2", "kACS3", "kBloody", "kHard", "kNoPare",
    "kTimer", "kTickTimer", "kRepopDecay", "kNoLocate", "kLimitTimer",
    "kNotDisturbInSacrum", "kNot1InLoot", "k1Inloot1", "k1Inloot2",
    "k1Inloot3", "k1Inloot4", "kSet", "kNamed", "kNoalter", "kNotMultiLoot",
    "kSeized", "kProtectedByTriggeredFlag", "kUniqLoot", "kDecay"
]

# Wear flags (wear_bits[])
WEAR_FLAGS = [
    "kTake", "kFinger", "kNeck", "kBody", "kHead", "kLegs", "kFeet",
    "kHands", "kArms", "kShield", "kShoulders", "kWaist", "kWrist",
    "kWield", "kHold", "kBoth", "kQuiver"
]

# Anti flags (anti_bits[])
ANTI_FLAGS = [
    "kAntiMage", "kAntiCleric", "kAntiThief", "kAntiWarrior", "kAntiAssassin",
    "kAntiGuard", "kAntiPaladine", "kAntiRanger", "kAntiMerchant",
    "kAntiMono", "kAntiPoly", "kAntiVigilant", "kAntiMale", "kAntiFemale",
    "kAntiCharmice", "kAntiArchmage", "kAntiCharmer", "kAntiNecromancer",
    "kAntiProgrammer", "kAntiVictim", "kAntiSkeleton", "kAntiZombie",
    "kAntiBogatur", "kAntiVarvar"
]

# Genders
GENDERS = ["kNeutral", "kMale", "kFemale", "kPoly"]

# Positions
POSITIONS = [
    "kDead", "kMortally", "kIncapacitated", "kStunned", "kSleeping",
    "kResting", "kSitting", "kFight", "kStanding"
]

# Sector types
SECTORS = [
    "kInside", "kCity", "kField", "kForest", "kHills", "kMountain",
    "kWaterSwim", "kWaterNoSwim", "kFlying", "kUnderwater", "kSecret",
    "kRock", "kSwamp", "kRoad", "kIceSnow", "kSnow", "kSand", "kThinIce"
]

# Apply types
APPLY_TYPES = [
    "kNone", "kStr", "kDex", "kInt", "kWis", "kCon", "kCha", "kClass",
    "kLevel", "kAge", "kCharWeight", "kCharHeight", "kMana", "kHp", "kMove",
    "kGold", "kExp", "kAC", "kHitroll", "kDamroll", "kSavingWill",
    "kSavingCritical", "kSavingStability", "kSavingReflex", "kHPRegen",
    "kManaRegen", "kMoveRegen", "kPhysicDamagePercent", "kMagicDamagePercent",
    "kWeight", "kHeight", "kSize", "kAgeAdd", "kHitrollBonus", "kDamrollBonus",
    "kSavingNoneBonus", "kCastSuccess", "kMR", "kAbsorb", "kLikeSkillNumber",
    "kSkillsMagicNumber", "kMagicResistance", "kAffectKnowLevel", "kFeat",
    "kInitiative", "kReligion", "kAbsorbHp", "kPerceptionRange", "kPoisonAdd"
]

# Trigger attach types
TRIGGER_ATTACH_TYPES = ["kMob", "kObj", "kRoom"]

# Trigger types (letters -> flag names)
TRIGGER_TYPES = {
    'g': 'kGlobal',      # Global
    'r': 'kRandom',      # Random
    'c': 'kCommand',     # Command
    's': 'kSpeech',      # Speech
    'a': 'kAct',         # Act
    'd': 'kDeath',       # Death
    'f': 'kFight',       # Fight
    'h': 'kHitprcnt',    # HitPercent
    'e': 'kGreet',       # Greet
    'o': 'kEntry',       # Entry
    'b': 'kReceive',     # Receive
    'q': 'kLoad',        # Load
    'l': 'kLeave',       # Leave
    'n': 'kDoor',        # Door
    'i': 'kGive',        # Give
    'j': 'kConsume',     # Consume
    't': 'kTimer',       # Timer
    'u': 'kGet',         # Get
    'p': 'kDrop',        # Drop
    'm': 'kSit',         # Sit
    'k': 'kCast',        # Cast
    'w': 'kTime',        # Time
    'x': 'kUnlock',      # Unlock
    'y': 'kOpen',        # Open
    'z': 'kClose',       # Close
    '1': 'kStart',       # Start
    '2': 'kIncome',      # Income
}

# Zone reset command types
ZONE_CMD_TYPES = {
    'M': 'LOAD_MOB',
    'O': 'LOAD_OBJ',
    'G': 'GIVE_OBJ',
    'E': 'EQUIP_MOB',
    'P': 'PUT_IN_OBJ',
    'D': 'SET_DOOR',
    'R': 'REMOVE_OBJ',
    'T': 'ATTACH_TRIGGER',
    'V': 'SET_VARIABLE',
    'Q': 'SPEC_LOAD',
}

# Directions for zone door commands
ZONE_DIRECTIONS = ['kNorth', 'kEast', 'kSouth', 'kWest', 'kUp', 'kDown']


def get_room_name(vnum):
    """Get room name by vnum for comments."""
    return ROOM_NAMES.get(vnum, '')


def get_mob_name(vnum):
    """Get mob name by vnum for comments."""
    return MOB_NAMES.get(vnum, '')


def get_obj_name(vnum):
    """Get object name by vnum for comments."""
    return OBJ_NAMES.get(vnum, '')


def get_trigger_name(vnum):
    """Get trigger name by vnum for comments."""
    return TRIGGER_NAMES.get(vnum, '')


def load_name_from_yaml(filepath):
    """Extract name and vnum from a YAML file."""
    try:
        with open(filepath, 'r', encoding='koi8-r') as f:
            content = f.read()
        vnum = None
        name = None
        for line in content.split('\n'):
            if line.startswith('vnum:'):
                vnum = int(line.split(':')[1].strip())
            elif line.startswith('name:'):
                # Remove quotes
                name = line.split(':', 1)[1].strip().strip('"')
                break
        return vnum, name
    except:
        return None, None


def load_names_from_yaml_dir(yaml_dir, name_key='name'):
    """Load names from YAML files in directory."""
    names = {}
    yaml_path = Path(yaml_dir)
    for yaml_file in yaml_path.glob('*.yaml'):
        try:
            with open(yaml_file, 'r', encoding='koi8-r') as f:
                content = f.read()
            vnum = None
            name = None
            for line in content.split('\n'):
                line = line.strip()
                if line.startswith('vnum:'):
                    vnum = int(line.split(':')[1].strip())
                elif line.startswith(f'{name_key}:'):
                    # Remove quotes
                    name = line.split(':', 1)[1].strip().strip('"')
                    if vnum is not None:
                        break
            if vnum is not None and name:
                names[vnum] = name
        except:
            pass
    return names


def build_name_registries(world_dir):
    """Build all name registries from world files."""
    global ROOM_NAMES, MOB_NAMES, OBJ_NAMES, TRIGGER_NAMES, ZONE_NAMES

    world_path = Path(world_dir)

    # Load room names - try .wld first, then .yaml
    wld_files = list(world_path.glob('wld/*.wld'))
    if wld_files:
        for wld_file in wld_files:
            rooms = parse_wld_file(str(wld_file))
            for room in rooms:
                ROOM_NAMES[room['vnum']] = room['name']
    else:
        ROOM_NAMES = load_names_from_yaml_dir(world_path / 'wld', 'name')

    # Load mob names - try .mob first, then .yaml
    mob_files = list(world_path.glob('mob/*.mob'))
    if mob_files:
        for mob_file in mob_files:
            mobs = parse_mob_file(str(mob_file))
            for mob in mobs:
                MOB_NAMES[mob['vnum']] = mob.get('names', {}).get('nominative', '')
    else:
        # For mobs, name is under names.nominative - need to extract differently
        for yaml_file in (world_path / 'mob').glob('*.yaml'):
            try:
                with open(yaml_file, 'r', encoding='koi8-r') as f:
                    content = f.read()
                vnum = None
                name = None
                in_names = False
                for line in content.split('\n'):
                    stripped = line.strip()
                    # Check for top-level keys (not indented)
                    is_top_level = line and not line[0].isspace() and ':' in line
                    if stripped.startswith('vnum:'):
                        vnum = int(stripped.split(':')[1].strip())
                    elif stripped == 'names:':
                        in_names = True
                    elif in_names and stripped.startswith('nominative:'):
                        name = stripped.split(':', 1)[1].strip().strip('"')
                        break
                    elif in_names and is_top_level and not stripped.startswith('names:'):
                        in_names = False
                if vnum is not None and name:
                    MOB_NAMES[vnum] = name
            except:
                pass

    # Load object names - try .obj first, then .yaml
    obj_files = list(world_path.glob('obj/*.obj'))
    if obj_files:
        for obj_file in obj_files:
            objs = parse_obj_file(str(obj_file))
            for obj in objs:
                OBJ_NAMES[obj['vnum']] = obj.get('names', {}).get('nominative', '')
    else:
        # For objects, name is under names.nominative
        for yaml_file in (world_path / 'obj').glob('*.yaml'):
            try:
                with open(yaml_file, 'r', encoding='koi8-r') as f:
                    content = f.read()
                vnum = None
                name = None
                in_names = False
                for line in content.split('\n'):
                    stripped = line.strip()
                    # Check for top-level keys (not indented)
                    is_top_level = line and not line[0].isspace() and ':' in line
                    if stripped.startswith('vnum:'):
                        vnum = int(stripped.split(':')[1].strip())
                    elif stripped == 'names:':
                        in_names = True
                    elif in_names and stripped.startswith('nominative:'):
                        name = stripped.split(':', 1)[1].strip().strip('"')
                        break
                    elif in_names and is_top_level and not stripped.startswith('names:'):
                        in_names = False
                if vnum is not None and name:
                    OBJ_NAMES[vnum] = name
            except:
                pass

    # Load trigger names - try .trg first, then .yaml
    trg_files = list(world_path.glob('trg/*.trg'))
    if trg_files:
        for trg_file in trg_files:
            triggers = parse_trg_file(str(trg_file))
            for trg in triggers:
                TRIGGER_NAMES[trg['vnum']] = trg['name']
    else:
        TRIGGER_NAMES = load_names_from_yaml_dir(world_path / 'trg', 'name')

    # Load zone names - try .zon first, then .yaml
    zon_files = list(world_path.glob('zon/*.zon'))
    if zon_files:
        for zon_file in zon_files:
            zone = parse_zon_file(str(zon_file))
            if zone:
                ZONE_NAMES[zone['vnum']] = zone['name']
    else:
        ZONE_NAMES = load_names_from_yaml_dir(world_path / 'zon', 'name')

    print(f"  Loaded names: {len(ROOM_NAMES)} rooms, {len(MOB_NAMES)} mobs, "
          f"{len(OBJ_NAMES)} objs, {len(TRIGGER_NAMES)} triggers, {len(ZONE_NAMES)} zones")


def parse_ascii_flags(flags_str, flag_names, planes=4):
    """Parse ASCII flags like 'd0o0p0' into list of flag names."""
    result = []
    if not flags_str or flags_str == "0":
        return result

    i = 0
    while i < len(flags_str):
        if flags_str[i].isalpha():
            letter = flags_str[i].lower()
            plane = 0
            # Check for plane digit
            if i + 1 < len(flags_str) and flags_str[i + 1].isdigit():
                plane = int(flags_str[i + 1])
                i += 2
            else:
                i += 1

            # Convert letter to index (a=0, b=1, etc)
            bit_pos = ord(letter) - ord('a')
            if bit_pos >= 0 and bit_pos < 30:
                flag_index = plane * 30 + bit_pos
                if flag_index < len(flag_names):
                    result.append(flag_names[flag_index])
        else:
            i += 1

    return result


def parse_mob_file(filepath):
    """Parse a mob file and return list of mob dictionaries."""
    mobs = []

    try:
        with open(filepath, 'r', encoding='koi8-r') as f:
            content = f.read()
    except:
        try:
            with open(filepath, 'r', encoding='cp1251') as f:
                content = f.read()
        except:
            with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
                content = f.read()

    # Split by #vnum
    mob_blocks = re.split(r'^#(\d+)\s*$', content, flags=re.MULTILINE)

    i = 1  # Skip first empty element
    while i < len(mob_blocks) - 1:
        try:
            vnum = int(mob_blocks[i])
        except ValueError:
            i += 2
            continue
        block = mob_blocks[i + 1]
        i += 2

        lines = block.strip().split('\n')
        if len(lines) < 12:
            continue

        mob = {'vnum': vnum}

        # Parse names (7 lines ending with ~)
        idx = 0
        names = []
        while idx < len(lines) and len(names) < 7:
            line = lines[idx].rstrip()
            if line.endswith('~'):
                names.append(line[:-1])
            else:
                names.append(line)
            idx += 1

        if len(names) >= 7:
            mob['names'] = {
                'aliases': names[0],
                'nominative': names[1],
                'genitive': names[2],
                'dative': names[3],
                'accusative': names[4],
                'instrumental': names[5],
                'prepositional': names[6]
            }

        # Parse descriptions (two descriptions ending with ~)
        short_desc = []
        while idx < len(lines):
            line = lines[idx]
            if line.rstrip().endswith('~'):
                short_desc.append(line.rstrip()[:-1])
                idx += 1
                break
            short_desc.append(line)
            idx += 1

        long_desc = []
        while idx < len(lines):
            line = lines[idx]
            if line.rstrip() == '~':
                idx += 1
                break
            if line.rstrip().endswith('~'):
                long_desc.append(line.rstrip()[:-1])
                idx += 1
                break
            long_desc.append(line)
            idx += 1

        mob['descriptions'] = {
            'short': '\n'.join(short_desc).strip(),
            'long': '\n'.join(long_desc).strip()
        }

        # Parse flags line: action_flags affect_flags alignment mob_type
        if idx < len(lines):
            flags_line = lines[idx].split()
            idx += 1

            if len(flags_line) >= 3:
                mob['action_flags'] = parse_ascii_flags(flags_line[0], ACTION_FLAGS)
                mob['affect_flags'] = parse_ascii_flags(flags_line[1], [])  # TODO: affected_bits
                try:
                    mob['alignment'] = int(flags_line[2])
                except ValueError:
                    mob['alignment'] = 0  # Default if malformed
                if len(flags_line) >= 4:
                    mob['mob_type'] = flags_line[3]

        # Parse stats line: level hitroll armor HP damage
        if idx < len(lines):
            stats_line = lines[idx]
            idx += 1

            # Parse format: level hitroll armor XdY+Z AdB+C
            match = re.match(r'(\d+)\s+(\d+)\s+(\d+)\s+(\d+)d(\d+)\+(\d+)\s+(\d+)d(\d+)\+(\d+)', stats_line)
            if match:
                mob['stats'] = {
                    'level': int(match.group(1)),
                    'hitroll_penalty': int(match.group(2)),
                    'armor': int(match.group(3)),
                    'hp': {
                        'dice_count': int(match.group(4)),
                        'dice_size': int(match.group(5)),
                        'bonus': int(match.group(6))
                    },
                    'damage': {
                        'dice_count': int(match.group(7)),
                        'dice_size': int(match.group(8)),
                        'bonus': int(match.group(9))
                    }
                }

        # Parse gold line: XdY+Z exp
        if idx < len(lines):
            gold_line = lines[idx]
            idx += 1

            match = re.match(r'(\d+)d(\d+)\+(\d+)\s+(\d+)', gold_line)
            if match:
                mob['gold'] = {
                    'dice_count': int(match.group(1)),
                    'dice_size': int(match.group(2)),
                    'bonus': int(match.group(3))
                }
                mob['experience'] = int(match.group(4))

        # Parse position line: default_pos start_pos sex
        if idx < len(lines):
            pos_line = lines[idx].split()
            idx += 1

            if len(pos_line) >= 3:
                try:
                    default_pos = int(pos_line[0])
                    start_pos = int(pos_line[1])
                    sex = int(pos_line[2])

                    mob['position'] = {
                        'default': POSITIONS[default_pos] if default_pos < len(POSITIONS) else str(default_pos),
                        'start': POSITIONS[start_pos] if start_pos < len(POSITIONS) else str(start_pos)
                    }
                    mob['sex'] = GENDERS[sex] if sex < len(GENDERS) else str(sex)
                except (ValueError, IndexError):
                    mob['position'] = {'default': 'kStanding', 'start': 'kStanding'}
                    mob['sex'] = 'kMale'

        # Parse E-spec attributes
        mob['attributes'] = {}
        mob['skills'] = []
        mob['triggers'] = []

        while idx < len(lines):
            line = lines[idx].strip()
            idx += 1

            # Stop at next mob definition
            if line.startswith('#') and line[1:].isdigit():
                break

            if line.startswith('Str:'):
                mob['attributes']['strength'] = int(line.split(':')[1].strip())
            elif line.startswith('Dex:'):
                mob['attributes']['dexterity'] = int(line.split(':')[1].strip())
            elif line.startswith('Int:'):
                mob['attributes']['intelligence'] = int(line.split(':')[1].strip())
            elif line.startswith('Wis:'):
                mob['attributes']['wisdom'] = int(line.split(':')[1].strip())
            elif line.startswith('Con:'):
                mob['attributes']['constitution'] = int(line.split(':')[1].strip())
            elif line.startswith('Cha:'):
                mob['attributes']['charisma'] = int(line.split(':')[1].strip())
            elif line.startswith('Size:'):
                mob['size'] = int(line.split(':')[1].strip())
            elif line.startswith('Class:'):
                mob['mob_class'] = int(line.split(':')[1].strip())
            elif line.startswith('Race:'):
                mob['race'] = int(line.split(':')[1].strip())
            elif line.startswith('Height:'):
                mob['height'] = int(line.split(':')[1].strip())
            elif line.startswith('Weight:'):
                mob['weight'] = int(line.split(':')[1].strip())
            elif line.startswith('Skill:'):
                parts = line.split()
                if len(parts) >= 3:
                    mob['skills'].append({
                        'skill_id': int(parts[1]),
                        'value': int(parts[2])
                    })
            elif line.startswith('T '):
                try:
                    trig_vnum = int(line.split()[1])
                    mob['triggers'].append(trig_vnum)
                except (ValueError, IndexError):
                    pass
            # 'E' and 'S' are end markers but triggers come after them, so don't break

        mobs.append(mob)

    return mobs


def mob_to_yaml(mob):
    """Convert mob dictionary to YAML string."""
    lines = []
    lines.append(f"# Mob #{mob['vnum']}")
    lines.append(f"vnum: {mob['vnum']}")
    lines.append("")

    # Names
    if 'names' in mob:
        lines.append("names:")
        for key, value in mob['names'].items():
            lines.append(f"  {key}: \"{value}\"")
    lines.append("")

    # Descriptions
    if 'descriptions' in mob:
        lines.append("descriptions:")
        for key, value in mob['descriptions'].items():
            if '\n' in value:
                lines.append(f"  {key}: |")
                for desc_line in value.split('\n'):
                    lines.append(f"    {desc_line}")
            else:
                lines.append(f"  {key}: \"{value}\"")
    lines.append("")

    # Action flags
    if mob.get('action_flags'):
        lines.append("action_flags:")
        for flag in mob['action_flags']:
            lines.append(f"  - {flag}")

    # Affect flags
    if mob.get('affect_flags'):
        lines.append("affect_flags:")
        for flag in mob['affect_flags']:
            lines.append(f"  - {flag}")

    # Alignment
    if 'alignment' in mob:
        lines.append(f"alignment: {mob['alignment']}")

    # Mob type
    if 'mob_type' in mob:
        lines.append(f"mob_type: {mob['mob_type']}")
    lines.append("")

    # Stats
    if 'stats' in mob:
        lines.append("stats:")
        stats = mob['stats']
        lines.append(f"  level: {stats.get('level', 1)}")
        lines.append(f"  hitroll_penalty: {stats.get('hitroll_penalty', 20)}")
        lines.append(f"  armor: {stats.get('armor', 100)}")
        if 'hp' in stats:
            lines.append("  hp:")
            lines.append(f"    dice_count: {stats['hp']['dice_count']}")
            lines.append(f"    dice_size: {stats['hp']['dice_size']}")
            lines.append(f"    bonus: {stats['hp']['bonus']}")
        if 'damage' in stats:
            lines.append("  damage:")
            lines.append(f"    dice_count: {stats['damage']['dice_count']}")
            lines.append(f"    dice_size: {stats['damage']['dice_size']}")
            lines.append(f"    bonus: {stats['damage']['bonus']}")
    lines.append("")

    # Gold
    if 'gold' in mob:
        lines.append("gold:")
        lines.append(f"  dice_count: {mob['gold']['dice_count']}")
        lines.append(f"  dice_size: {mob['gold']['dice_size']}")
        lines.append(f"  bonus: {mob['gold']['bonus']}")

    # Experience
    if 'experience' in mob:
        lines.append(f"experience: {mob['experience']}")
    lines.append("")

    # Position
    if 'position' in mob:
        lines.append("position:")
        lines.append(f"  default: {mob['position']['default']}")
        lines.append(f"  start: {mob['position']['start']}")

    # Sex
    if 'sex' in mob:
        lines.append(f"sex: {mob['sex']}")
    lines.append("")

    # Attributes
    if mob.get('attributes'):
        lines.append("attributes:")
        for key, value in mob['attributes'].items():
            lines.append(f"  {key}: {value}")
    lines.append("")

    # Additional fields
    if 'size' in mob:
        lines.append(f"size: {mob['size']}")
    if 'mob_class' in mob:
        lines.append(f"mob_class: {mob['mob_class']}")
    if 'race' in mob:
        lines.append(f"race: {mob['race']}")
    if 'height' in mob:
        lines.append(f"height: {mob['height']}")
    if 'weight' in mob:
        lines.append(f"weight: {mob['weight']}")
    lines.append("")

    # Skills
    if mob.get('skills'):
        lines.append("skills:")
        for skill in mob['skills']:
            lines.append(f"  - skill_id: {skill['skill_id']}")
            lines.append(f"    value: {skill['value']}")

    # Triggers
    if mob.get('triggers'):
        lines.append("triggers:")
        for trig in mob['triggers']:
            trig_name = get_trigger_name(trig)
            comment = f"  # {trig_name}" if trig_name else ""
            lines.append(f"  - {trig}{comment}")

    return '\n'.join(lines)


def parse_obj_file(filepath):
    """Parse an object file and return list of object dictionaries."""
    objs = []

    try:
        with open(filepath, 'r', encoding='koi8-r') as f:
            content = f.read()
    except:
        try:
            with open(filepath, 'r', encoding='cp1251') as f:
                content = f.read()
        except:
            with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
                content = f.read()

    # Split by #vnum
    obj_blocks = re.split(r'^#(\d+)\s*$', content, flags=re.MULTILINE)

    i = 1
    while i < len(obj_blocks) - 1:
        vnum = int(obj_blocks[i])
        block = obj_blocks[i + 1]
        i += 2

        lines = block.strip().split('\n')
        if len(lines) < 10:
            continue

        obj = {'vnum': vnum}

        # Parse names (7 lines ending with ~)
        idx = 0
        names = []
        while idx < len(lines) and len(names) < 7:
            line = lines[idx].rstrip()
            if line.endswith('~'):
                names.append(line[:-1])
            else:
                names.append(line)
            idx += 1

        if len(names) >= 7:
            obj['names'] = {
                'aliases': names[0],
                'nominative': names[1],
                'genitive': names[2],
                'dative': names[3],
                'accusative': names[4],
                'instrumental': names[5],
                'prepositional': names[6]
            }

        # Parse descriptions (room and action)
        room_desc = []
        while idx < len(lines):
            line = lines[idx]
            if line.rstrip().endswith('~'):
                room_desc.append(line.rstrip()[:-1])
                idx += 1
                break
            room_desc.append(line)
            idx += 1

        action_desc = []
        while idx < len(lines):
            line = lines[idx]
            if line.rstrip() == '~':
                idx += 1
                break
            if line.rstrip().endswith('~'):
                action_desc.append(line.rstrip()[:-1])
                idx += 1
                break
            action_desc.append(line)
            idx += 1

        obj['descriptions'] = {
            'room': '\n'.join(room_desc).strip(),
            'action': '\n'.join(action_desc).strip()
        }

        # Parse spec_param max_dur cur_dur material
        if idx < len(lines):
            parts = lines[idx].split()
            idx += 1
            if len(parts) >= 4:
                obj['spec_param'] = int(parts[0])
                obj['durability'] = {
                    'maximum': int(parts[1]),
                    'current': int(parts[2])
                }
                obj['material'] = int(parts[3])

        # Parse sex timer spell level
        if idx < len(lines):
            parts = lines[idx].split()
            idx += 1
            if len(parts) >= 4:
                sex = int(parts[0])
                obj['sex'] = GENDERS[sex] if sex < len(GENDERS) else str(sex)
                obj['timer'] = int(parts[1])
                obj['spell'] = int(parts[2])
                obj['level'] = int(parts[3])

        # Parse affect_flags anti_flags no_flags
        if idx < len(lines):
            parts = lines[idx].split()
            idx += 1
            # These are numeric bitvectors
            obj['affect_flags_raw'] = parts[0] if len(parts) > 0 else "0"
            obj['anti_flags_raw'] = parts[1] if len(parts) > 1 else "0"
            obj['no_flags_raw'] = parts[2] if len(parts) > 2 else "0"

        # Parse type extra_flags wear_flags
        if idx < len(lines):
            parts = lines[idx].split()
            idx += 1
            if len(parts) >= 1:
                obj_type = int(parts[0])
                obj['type'] = OBJ_TYPES[obj_type] if obj_type < len(OBJ_TYPES) else str(obj_type)
            if len(parts) >= 2:
                obj['extra_flags'] = parse_ascii_flags(parts[1], EXTRA_FLAGS)
            if len(parts) >= 3:
                obj['wear_flags'] = parse_ascii_flags(parts[2], WEAR_FLAGS)

        # Parse values
        if idx < len(lines):
            parts = lines[idx].split()
            idx += 1
            obj['values'] = {
                'value0': int(parts[0]) if len(parts) > 0 else 0,
                'value1': int(parts[1]) if len(parts) > 1 else 0,
                'value2': int(parts[2]) if len(parts) > 2 else 0,
                'value3': int(parts[3]) if len(parts) > 3 else 0
            }

        # Parse weight cost rent_off rent_on
        if idx < len(lines):
            parts = lines[idx].split()
            idx += 1
            if len(parts) >= 4:
                obj['weight'] = int(parts[0])
                obj['cost'] = int(parts[1])
                obj['rent'] = {
                    'off': int(parts[2]),
                    'on': int(parts[3])
                }

        # Parse extras
        obj['max_in_world'] = -1
        obj['applies'] = []
        obj['extra_descriptions'] = []
        obj['triggers'] = []

        while idx < len(lines):
            line = lines[idx].strip()
            idx += 1

            if line == 'E' or line == 'S' or line.startswith('#'):
                break

            if line.startswith('M '):
                obj['max_in_world'] = int(line.split()[1])
            elif line.startswith('A'):
                # Apply: next two lines are location and modifier
                if idx < len(lines):
                    loc_line = lines[idx].strip()
                    idx += 1
                    parts = loc_line.split()
                    if len(parts) >= 2:
                        loc = int(parts[0])
                        mod = int(parts[1])
                        loc_name = APPLY_TYPES[loc] if loc < len(APPLY_TYPES) else str(loc)
                        obj['applies'].append({
                            'location': loc_name,
                            'modifier': mod
                        })
            elif line.startswith('E'):
                # Extra description
                if idx + 1 < len(lines):
                    keywords = lines[idx].rstrip('~')
                    idx += 1
                    desc_lines = []
                    while idx < len(lines):
                        desc_line = lines[idx]
                        idx += 1
                        if desc_line.rstrip() == '~':
                            break
                        if desc_line.rstrip().endswith('~'):
                            desc_lines.append(desc_line.rstrip()[:-1])
                            break
                        desc_lines.append(desc_line)
                    obj['extra_descriptions'].append({
                        'keywords': keywords,
                        'description': '\n'.join(desc_lines)
                    })
            elif line.startswith('T '):
                trig_vnum = int(line.split()[1])
                obj['triggers'].append(trig_vnum)

        objs.append(obj)

    return objs


def obj_to_yaml(obj):
    """Convert object dictionary to YAML string."""
    lines = []
    lines.append(f"# Object #{obj['vnum']}")
    lines.append(f"vnum: {obj['vnum']}")
    lines.append("")

    # Names
    if 'names' in obj:
        lines.append("names:")
        for key, value in obj['names'].items():
            lines.append(f"  {key}: \"{value}\"")
    lines.append("")

    # Descriptions
    if 'descriptions' in obj:
        lines.append("descriptions:")
        for key, value in obj['descriptions'].items():
            if '\n' in value:
                lines.append(f"  {key}: |")
                for desc_line in value.split('\n'):
                    lines.append(f"    {desc_line}")
            else:
                lines.append(f"  {key}: \"{value}\"")
    lines.append("")

    # Type
    if 'type' in obj:
        lines.append(f"type: {obj['type']}")

    # Spec param
    if 'spec_param' in obj:
        lines.append(f"spec_param: {obj['spec_param']}")

    # Durability
    if 'durability' in obj:
        lines.append("durability:")
        lines.append(f"  maximum: {obj['durability']['maximum']}")
        lines.append(f"  current: {obj['durability']['current']}")

    # Material
    if 'material' in obj:
        lines.append(f"material: {obj['material']}")

    # Sex, timer, spell, level
    if 'sex' in obj:
        lines.append(f"sex: {obj['sex']}")
    if 'timer' in obj:
        lines.append(f"timer: {obj['timer']}")
    if 'spell' in obj:
        lines.append(f"spell: {obj['spell']}")
    if 'level' in obj:
        lines.append(f"level: {obj['level']}")
    lines.append("")

    # Flags
    if obj.get('extra_flags'):
        lines.append("extra_flags:")
        for flag in obj['extra_flags']:
            lines.append(f"  - {flag}")

    if obj.get('wear_flags'):
        lines.append("wear_flags:")
        for flag in obj['wear_flags']:
            lines.append(f"  - {flag}")
    lines.append("")

    # Values
    if 'values' in obj:
        lines.append("values:")
        for key, value in obj['values'].items():
            lines.append(f"  {key}: {value}")
    lines.append("")

    # Weight, cost, rent
    if 'weight' in obj:
        lines.append(f"weight: {obj['weight']}")
    if 'cost' in obj:
        lines.append(f"cost: {obj['cost']}")
    if 'rent' in obj:
        lines.append("rent:")
        lines.append(f"  off: {obj['rent']['off']}")
        lines.append(f"  on: {obj['rent']['on']}")
    lines.append("")

    # Max in world
    if obj.get('max_in_world', -1) != -1:
        lines.append(f"max_in_world: {obj['max_in_world']}")

    # Applies
    if obj.get('applies'):
        lines.append("applies:")
        for apply in obj['applies']:
            lines.append(f"  - location: {apply['location']}")
            lines.append(f"    modifier: {apply['modifier']}")

    # Extra descriptions
    if obj.get('extra_descriptions'):
        lines.append("extra_descriptions:")
        for ed in obj['extra_descriptions']:
            lines.append(f"  - keywords: \"{ed['keywords']}\"")
            if '\n' in ed['description']:
                lines.append("    description: |")
                for desc_line in ed['description'].split('\n'):
                    lines.append(f"      {desc_line}")
            else:
                lines.append(f"    description: \"{ed['description']}\"")

    # Triggers
    if obj.get('triggers'):
        lines.append("triggers:")
        for trig in obj['triggers']:
            trig_name = get_trigger_name(trig)
            comment = f"  # {trig_name}" if trig_name else ""
            lines.append(f"  - {trig}{comment}")

    return '\n'.join(lines)


def parse_wld_file(filepath):
    """Parse a room/world file and return list of room dictionaries."""
    rooms = []

    try:
        with open(filepath, 'r', encoding='koi8-r') as f:
            content = f.read()
    except:
        try:
            with open(filepath, 'r', encoding='cp1251') as f:
                content = f.read()
        except:
            with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
                content = f.read()

    # Split by #vnum
    room_blocks = re.split(r'^#(\d+)\s*$', content, flags=re.MULTILINE)

    i = 1
    while i < len(room_blocks) - 1:
        vnum = int(room_blocks[i])
        block = room_blocks[i + 1]
        i += 2

        lines = block.strip().split('\n')
        if len(lines) < 3:
            continue

        room = {'vnum': vnum}

        idx = 0

        # Parse name (ending with ~)
        # Name may span multiple lines - read until ~ is found, join with space
        # This also handles edge case where ~ is on separate line
        name_parts = []
        while idx < len(lines):
            line = lines[idx]
            idx += 1
            if line.rstrip() == '~':
                # Standalone ~ terminates name
                break
            if line.rstrip().endswith('~'):
                name_parts.append(line.rstrip()[:-1].strip())
                break
            name_parts.append(line.strip())
        room['name'] = ' '.join(name_parts)

        # Parse description (until ~)
        desc_lines = []
        while idx < len(lines):
            line = lines[idx]
            idx += 1
            if line.rstrip() == '~':
                break
            if line.rstrip().endswith('~'):
                desc_lines.append(line.rstrip()[:-1])
                break
            desc_lines.append(line)
        room['description'] = '\n'.join(desc_lines)

        # Parse zone room_flags sector
        if idx < len(lines):
            parts = lines[idx].split()
            idx += 1
            if len(parts) >= 3:
                try:
                    room['zone'] = int(parts[0])
                except ValueError:
                    room['zone'] = 0
                room['room_flags'] = parse_ascii_flags(parts[1], ROOM_FLAGS)
                try:
                    sector = int(parts[2])
                    room['sector'] = SECTORS[sector] if sector < len(SECTORS) else str(sector)
                except ValueError:
                    room['sector'] = 'kInside'

        # Parse exits and other data
        room['exits'] = []
        room['extra_descriptions'] = []
        room['triggers'] = []

        DIRECTIONS = ['kNorth', 'kEast', 'kSouth', 'kWest', 'kUp', 'kDown']

        while idx < len(lines):
            line = lines[idx].strip()
            idx += 1

            if line == 'S' or line.startswith('#'):
                break

            # Exit
            if line.startswith('D'):
                dir_num = int(line[1:])
                exit_data = {
                    'direction': DIRECTIONS[dir_num] if dir_num < len(DIRECTIONS) else str(dir_num)
                }

                # Exit description
                if idx < len(lines):
                    desc = lines[idx].rstrip('~')
                    idx += 1
                    exit_data['description'] = desc

                # Exit keywords
                if idx < len(lines):
                    keywords = lines[idx].rstrip('~')
                    idx += 1
                    exit_data['keywords'] = keywords

                # Exit info: flags key to_room exit_info
                if idx < len(lines):
                    parts = lines[idx].split()
                    idx += 1
                    if len(parts) >= 3:
                        try:
                            exit_data['exit_flags'] = int(parts[0])
                            exit_data['key'] = int(parts[1])
                            exit_data['to_room'] = int(parts[2])
                            if len(parts) >= 4:
                                exit_data['lock_complexity'] = int(parts[3])
                        except ValueError:
                            exit_data['exit_flags'] = 0
                            exit_data['key'] = -1
                            exit_data['to_room'] = -1

                room['exits'].append(exit_data)

            # Extra description
            elif line == 'E':
                if idx < len(lines):
                    keywords = lines[idx].rstrip('~')
                    idx += 1
                    desc_lines = []
                    while idx < len(lines):
                        desc_line = lines[idx]
                        idx += 1
                        if desc_line.rstrip() == '~':
                            break
                        if desc_line.rstrip().endswith('~'):
                            desc_lines.append(desc_line.rstrip()[:-1])
                            break
                        desc_lines.append(desc_line)
                    room['extra_descriptions'].append({
                        'keywords': keywords,
                        'description': '\n'.join(desc_lines)
                    })

            # Trigger
            elif line.startswith('T '):
                trig_vnum = int(line.split()[1])
                room['triggers'].append(trig_vnum)

        rooms.append(room)

    return rooms


def room_to_yaml(room):
    """Convert room dictionary to YAML string."""
    lines = []
    lines.append(f"# Room #{room['vnum']}")
    lines.append(f"vnum: {room['vnum']}")

    if 'zone' in room:
        lines.append(f"zone: {room['zone']}")
    lines.append("")

    # Name
    lines.append(f"name: \"{room['name']}\"")
    lines.append("")

    # Description
    if room.get('description'):
        if '\n' in room['description']:
            lines.append("description: |")
            for desc_line in room['description'].split('\n'):
                lines.append(f"  {desc_line}")
        else:
            lines.append(f"description: \"{room['description']}\"")
    lines.append("")

    # Room flags
    if room.get('room_flags'):
        lines.append("room_flags:")
        for flag in room['room_flags']:
            lines.append(f"  - {flag}")

    # Sector
    if 'sector' in room:
        lines.append(f"sector: {room['sector']}")
    lines.append("")

    # Exits
    if room.get('exits'):
        lines.append("exits:")
        for exit in room['exits']:
            lines.append(f"  - direction: {exit['direction']}")
            if exit.get('description'):
                lines.append(f"    description: \"{exit['description']}\"")
            if exit.get('keywords'):
                lines.append(f"    keywords: \"{exit['keywords']}\"")
            if 'exit_flags' in exit:
                lines.append(f"    exit_flags: {exit['exit_flags']}")
            if 'key' in exit:
                lines.append(f"    key: {exit['key']}")
            if 'to_room' in exit:
                room_name = get_room_name(exit['to_room'])
                comment = f"  # {room_name}" if room_name else ""
                lines.append(f"    to_room: {exit['to_room']}{comment}")
            if 'lock_complexity' in exit:
                lines.append(f"    lock_complexity: {exit['lock_complexity']}")
    lines.append("")

    # Extra descriptions
    if room.get('extra_descriptions'):
        lines.append("extra_descriptions:")
        for ed in room['extra_descriptions']:
            lines.append(f"  - keywords: \"{ed['keywords']}\"")
            if '\n' in ed['description']:
                lines.append("    description: |")
                for desc_line in ed['description'].split('\n'):
                    lines.append(f"      {desc_line}")
            else:
                lines.append(f"    description: \"{ed['description']}\"")

    # Triggers
    if room.get('triggers'):
        lines.append("triggers:")
        for trig in room['triggers']:
            trig_name = get_trigger_name(trig)
            comment = f"  # {trig_name}" if trig_name else ""
            lines.append(f"  - {trig}{comment}")

    return '\n'.join(lines)


def parse_trg_file(filepath):
    """Parse a trigger file and return list of trigger dictionaries."""
    triggers = []

    try:
        with open(filepath, 'r', encoding='koi8-r') as f:
            content = f.read()
    except:
        try:
            with open(filepath, 'r', encoding='cp1251') as f:
                content = f.read()
        except:
            with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
                content = f.read()

    # Split by #vnum
    trg_blocks = re.split(r'^#(\d+)\s*$', content, flags=re.MULTILINE)

    i = 1
    while i < len(trg_blocks) - 1:
        vnum = int(trg_blocks[i])
        block = trg_blocks[i + 1]
        i += 2

        lines = block.strip().split('\n')
        if len(lines) < 3:
            continue

        trigger = {'vnum': vnum}
        idx = 0

        # Parse name (may span multiple lines, ends with ~)
        name_parts = []
        while idx < len(lines):
            line = lines[idx]
            idx += 1
            if line.rstrip() == '~':
                break
            if line.rstrip().endswith('~'):
                name_parts.append(line.rstrip()[:-1].strip())
                break
            name_parts.append(line.strip())
        trigger['name'] = ' '.join(name_parts)

        # Parse type line: attach_type trigger_types narg add_flag
        if idx < len(lines):
            type_line = lines[idx]
            parts = type_line.split()
            idx += 1
            if len(parts) >= 3:
                try:
                    attach_type = int(parts[0])
                    trigger['attach_type'] = TRIGGER_ATTACH_TYPES[attach_type] if attach_type < len(TRIGGER_ATTACH_TYPES) else str(attach_type)
                except ValueError:
                    trigger['attach_type'] = parts[0]

                # Parse trigger type letters
                trigger_letters = parts[1]
                trigger['trigger_types'] = []
                for letter in trigger_letters:
                    if letter in TRIGGER_TYPES:
                        trigger['trigger_types'].append(TRIGGER_TYPES[letter])

                try:
                    trigger['narg'] = int(parts[2])
                except ValueError:
                    trigger['narg'] = 0

                if len(parts) >= 4:
                    try:
                        trigger['add_flag'] = int(parts[3])
                    except ValueError:
                        trigger['add_flag'] = 0

        # Parse arglist (ends with ~)
        arglist_parts = []
        while idx < len(lines):
            line = lines[idx]
            idx += 1
            if line.rstrip() == '~':
                break
            if line.rstrip().endswith('~'):
                arglist_parts.append(line.rstrip()[:-1])
                break
            arglist_parts.append(line)
        trigger['arglist'] = ' '.join(arglist_parts)

        # Parse script (ends with ~)
        script_lines = []
        while idx < len(lines):
            line = lines[idx]
            idx += 1
            if line.rstrip() == '~':
                break
            if line.rstrip().endswith('~'):
                script_lines.append(line.rstrip()[:-1])
                break
            script_lines.append(line)
        trigger['script'] = '\n'.join(script_lines)

        triggers.append(trigger)

    return triggers


def trg_to_yaml(trigger):
    """Convert trigger dictionary to YAML string."""
    lines = []
    lines.append(f"# Trigger #{trigger['vnum']}")
    lines.append(f"vnum: {trigger['vnum']}")
    lines.append(f"name: \"{trigger['name']}\"")
    lines.append("")

    if 'attach_type' in trigger:
        lines.append(f"attach_type: {trigger['attach_type']}")

    if trigger.get('trigger_types'):
        lines.append("trigger_types:")
        for tt in trigger['trigger_types']:
            lines.append(f"  - {tt}")

    if 'narg' in trigger:
        lines.append(f"narg: {trigger['narg']}")

    if 'add_flag' in trigger:
        lines.append(f"add_flag: {trigger['add_flag']}")

    if trigger.get('arglist'):
        lines.append(f"arglist: \"{trigger['arglist']}\"")
    lines.append("")

    # Script as multiline
    if trigger.get('script'):
        lines.append("script: |")
        for script_line in trigger['script'].split('\n'):
            lines.append(f"  {script_line}")

    return '\n'.join(lines)


def parse_zon_file(filepath):
    """Parse a zone file and return zone dictionary."""
    try:
        with open(filepath, 'r', encoding='koi8-r') as f:
            content = f.read()
    except:
        try:
            with open(filepath, 'r', encoding='cp1251') as f:
                content = f.read()
        except:
            with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
                content = f.read()

    lines = content.strip().split('\n')
    if len(lines) < 3:
        return None

    idx = 0

    # Skip comment lines at start
    while idx < len(lines) and (lines[idx].startswith('*') or lines[idx].startswith(';')):
        idx += 1

    # Zone vnum
    if idx >= len(lines) or not lines[idx].startswith('#'):
        return None

    try:
        zone_vnum = int(lines[idx][1:].split()[0])
    except ValueError:
        return None
    idx += 1

    zone = {'vnum': zone_vnum}

    # Zone name (ends with ~)
    name_parts = []
    while idx < len(lines):
        line = lines[idx]
        idx += 1
        if line.rstrip() == '~':
            break
        if line.rstrip().endswith('~'):
            name_parts.append(line.rstrip()[:-1].strip())
            break
        name_parts.append(line.strip())
    zone['name'] = ' '.join(name_parts)

    # Builder info (optional, lines starting with ^, &, !, $)
    zone['builders'] = {}
    builder_prefixes = {'^': 'author', '&': 'coauthor', '!': 'tester', '$': 'description'}
    while idx < len(lines):
        line = lines[idx]
        if line and line[0] in builder_prefixes:
            key = builder_prefixes[line[0]]
            value = line[1:].rstrip('~').strip()
            zone['builders'][key] = value
            idx += 1
        else:
            break

    # Zone info line: #level type options top_room OR top_room lifespan reset_mode reset_idle
    if idx < len(lines):
        info_line = lines[idx].strip()
        idx += 1

        if info_line.startswith('#'):
            # New format: #level type options top_room
            parts = info_line[1:].split()
            if len(parts) >= 4:
                try:
                    zone['level'] = int(parts[0])
                    zone['zone_type'] = int(parts[1])
                    zone['options'] = int(parts[2])
                    zone['top_room'] = int(parts[3])
                except ValueError:
                    pass
        else:
            # Old format: top_room lifespan reset_mode reset_idle comment
            parts = info_line.split()
            if len(parts) >= 4:
                try:
                    zone['top_room'] = int(parts[0])
                    zone['lifespan'] = int(parts[1])
                    zone['reset_mode'] = int(parts[2])
                    zone['reset_idle'] = int(parts[3])
                except ValueError:
                    pass

    # Reset commands
    zone['commands'] = []
    while idx < len(lines):
        line = lines[idx].strip()
        idx += 1

        if line == 'S' or line == '$':
            break

        if not line or line.startswith('*') or line.startswith(';'):
            continue

        # Parse command
        parts = line.split()
        if not parts:
            continue

        cmd_char = parts[0]
        if cmd_char not in ZONE_CMD_TYPES:
            continue

        cmd = {'type': ZONE_CMD_TYPES[cmd_char]}

        # Extract comment (in parentheses at end)
        comment_match = re.search(r'\(([^)]+)\)\s*$', line)
        if comment_match:
            cmd['comment'] = comment_match.group(1)

        # Parse command-specific args
        try:
            if cmd_char == 'M':  # Load mob: if_flag mob_vnum max_world room_vnum max_room
                if len(parts) >= 6:
                    cmd['if_flag'] = int(parts[1])
                    cmd['mob_vnum'] = int(parts[2])
                    cmd['max_world'] = int(parts[3])
                    cmd['room_vnum'] = int(parts[4])
                    cmd['max_room'] = int(parts[5])

            elif cmd_char == 'O':  # Load obj: if_flag obj_vnum max room_vnum load_prob
                if len(parts) >= 6:
                    cmd['if_flag'] = int(parts[1])
                    cmd['obj_vnum'] = int(parts[2])
                    cmd['max'] = int(parts[3])
                    cmd['room_vnum'] = int(parts[4])
                    cmd['load_prob'] = int(parts[5])

            elif cmd_char == 'G':  # Give to mob: if_flag obj_vnum max load_prob max_wear
                if len(parts) >= 4:
                    cmd['if_flag'] = int(parts[1])
                    cmd['obj_vnum'] = int(parts[2])
                    cmd['max'] = int(parts[3])
                    if len(parts) >= 5:
                        cmd['load_prob'] = int(parts[4])
                    if len(parts) >= 6:
                        cmd['max_wear'] = int(parts[5])

            elif cmd_char == 'E':  # Equip mob: if_flag obj_vnum max wear_pos
                if len(parts) >= 5:
                    cmd['if_flag'] = int(parts[1])
                    cmd['obj_vnum'] = int(parts[2])
                    cmd['max'] = int(parts[3])
                    cmd['wear_pos'] = parts[4]

            elif cmd_char == 'P':  # Put in container: if_flag obj_vnum max container_vnum load_prob
                if len(parts) >= 6:
                    cmd['if_flag'] = int(parts[1])
                    cmd['obj_vnum'] = int(parts[2])
                    cmd['max'] = int(parts[3])
                    cmd['container_vnum'] = int(parts[4])
                    cmd['load_prob'] = int(parts[5])

            elif cmd_char == 'D':  # Set door: if_flag room_vnum direction state key
                if len(parts) >= 5:
                    cmd['if_flag'] = int(parts[1])
                    cmd['room_vnum'] = int(parts[2])
                    direction = int(parts[3])
                    cmd['direction'] = ZONE_DIRECTIONS[direction] if direction < len(ZONE_DIRECTIONS) else str(direction)
                    cmd['state'] = int(parts[4])
                    if len(parts) >= 6:
                        cmd['key'] = int(parts[5])

            elif cmd_char == 'R':  # Remove obj: if_flag room_vnum obj_vnum
                if len(parts) >= 4:
                    cmd['if_flag'] = int(parts[1])
                    cmd['room_vnum'] = int(parts[2])
                    cmd['obj_vnum'] = int(parts[3])

            elif cmd_char == 'T':  # Attach trigger: if_flag trigger_type entity_vnum trigger_vnum
                if len(parts) >= 5:
                    cmd['if_flag'] = int(parts[1])
                    cmd['trigger_type'] = int(parts[2])
                    cmd['entity_vnum'] = int(parts[3])
                    cmd['trigger_vnum'] = int(parts[4])

            elif cmd_char == 'V':  # Set variable: if_flag type context vnum var_name var_value
                if len(parts) >= 6:
                    cmd['if_flag'] = int(parts[1])
                    cmd['var_type'] = int(parts[2])
                    cmd['context'] = int(parts[3])
                    cmd['vnum'] = int(parts[4])
                    cmd['var_name'] = parts[5]
                    if len(parts) >= 7:
                        cmd['var_value'] = ' '.join(parts[6:]).split('(')[0].strip()

            elif cmd_char == 'Q':  # Special load: if_flag obj_vnum max_world room_vnum load_prob
                if len(parts) >= 6:
                    cmd['if_flag'] = int(parts[1])
                    cmd['obj_vnum'] = int(parts[2])
                    cmd['max_world'] = int(parts[3])
                    cmd['room_vnum'] = int(parts[4])
                    cmd['load_prob'] = int(parts[5])

        except (ValueError, IndexError):
            # Keep partial command
            pass

        zone['commands'].append(cmd)

    return zone


def zon_to_yaml(zone):
    """Convert zone dictionary to YAML string."""
    lines = []
    lines.append(f"# Zone #{zone['vnum']}")
    lines.append(f"vnum: {zone['vnum']}")
    lines.append(f"name: \"{zone['name']}\"")
    lines.append("")

    # Builders
    if zone.get('builders'):
        lines.append("builders:")
        for key, value in zone['builders'].items():
            lines.append(f"  {key}: \"{value}\"")
        lines.append("")

    # Zone info
    if 'level' in zone:
        lines.append(f"level: {zone['level']}")
    if 'zone_type' in zone:
        lines.append(f"zone_type: {zone['zone_type']}")
    if 'options' in zone:
        lines.append(f"options: {zone['options']}")
    if 'top_room' in zone:
        lines.append(f"top_room: {zone['top_room']}")
    if 'lifespan' in zone:
        lines.append(f"lifespan: {zone['lifespan']}")
    if 'reset_mode' in zone:
        lines.append(f"reset_mode: {zone['reset_mode']}")
    if 'reset_idle' in zone:
        lines.append(f"reset_idle: {zone['reset_idle']}")
    lines.append("")

    # Commands
    if zone.get('commands'):
        lines.append("commands:")
        for cmd in zone['commands']:
            cmd_type = cmd.get('type', 'UNKNOWN')
            lines.append(f"  - type: {cmd_type}")

            if 'if_flag' in cmd:
                lines.append(f"    if_flag: {cmd['if_flag']}")

            # Command-specific fields with name comments
            if cmd_type == 'LOAD_MOB':
                if 'mob_vnum' in cmd:
                    name = get_mob_name(cmd['mob_vnum'])
                    comment = f"  # {name}" if name else ""
                    lines.append(f"    mob_vnum: {cmd['mob_vnum']}{comment}")
                if 'max_world' in cmd:
                    lines.append(f"    max_world: {cmd['max_world']}")
                if 'room_vnum' in cmd:
                    name = get_room_name(cmd['room_vnum'])
                    comment = f"  # {name}" if name else ""
                    lines.append(f"    room_vnum: {cmd['room_vnum']}{comment}")
                if 'max_room' in cmd:
                    lines.append(f"    max_room: {cmd['max_room']}")

            elif cmd_type == 'LOAD_OBJ':
                if 'obj_vnum' in cmd:
                    name = get_obj_name(cmd['obj_vnum'])
                    comment = f"  # {name}" if name else ""
                    lines.append(f"    obj_vnum: {cmd['obj_vnum']}{comment}")
                if 'max' in cmd:
                    lines.append(f"    max: {cmd['max']}")
                if 'room_vnum' in cmd:
                    name = get_room_name(cmd['room_vnum'])
                    comment = f"  # {name}" if name else ""
                    lines.append(f"    room_vnum: {cmd['room_vnum']}{comment}")
                if 'load_prob' in cmd:
                    lines.append(f"    load_prob: {cmd['load_prob']}")

            elif cmd_type == 'GIVE_OBJ':
                if 'obj_vnum' in cmd:
                    name = get_obj_name(cmd['obj_vnum'])
                    comment = f"  # {name}" if name else ""
                    lines.append(f"    obj_vnum: {cmd['obj_vnum']}{comment}")
                if 'max' in cmd:
                    lines.append(f"    max: {cmd['max']}")
                if 'load_prob' in cmd:
                    lines.append(f"    load_prob: {cmd['load_prob']}")
                if 'max_wear' in cmd:
                    lines.append(f"    max_wear: {cmd['max_wear']}")

            elif cmd_type == 'EQUIP_MOB':
                if 'obj_vnum' in cmd:
                    name = get_obj_name(cmd['obj_vnum'])
                    comment = f"  # {name}" if name else ""
                    lines.append(f"    obj_vnum: {cmd['obj_vnum']}{comment}")
                if 'max' in cmd:
                    lines.append(f"    max: {cmd['max']}")
                if 'wear_pos' in cmd:
                    lines.append(f"    wear_pos: {cmd['wear_pos']}")

            elif cmd_type == 'PUT_IN_OBJ':
                if 'obj_vnum' in cmd:
                    name = get_obj_name(cmd['obj_vnum'])
                    comment = f"  # {name}" if name else ""
                    lines.append(f"    obj_vnum: {cmd['obj_vnum']}{comment}")
                if 'max' in cmd:
                    lines.append(f"    max: {cmd['max']}")
                if 'container_vnum' in cmd:
                    name = get_obj_name(cmd['container_vnum'])
                    comment = f"  # {name}" if name else ""
                    lines.append(f"    container_vnum: {cmd['container_vnum']}{comment}")
                if 'load_prob' in cmd:
                    lines.append(f"    load_prob: {cmd['load_prob']}")

            elif cmd_type == 'SET_DOOR':
                if 'room_vnum' in cmd:
                    name = get_room_name(cmd['room_vnum'])
                    comment = f"  # {name}" if name else ""
                    lines.append(f"    room_vnum: {cmd['room_vnum']}{comment}")
                if 'direction' in cmd:
                    lines.append(f"    direction: {cmd['direction']}")
                if 'state' in cmd:
                    lines.append(f"    state: {cmd['state']}")
                if 'key' in cmd:
                    lines.append(f"    key: {cmd['key']}")

            elif cmd_type == 'REMOVE_OBJ':
                if 'room_vnum' in cmd:
                    name = get_room_name(cmd['room_vnum'])
                    comment = f"  # {name}" if name else ""
                    lines.append(f"    room_vnum: {cmd['room_vnum']}{comment}")
                if 'obj_vnum' in cmd:
                    name = get_obj_name(cmd['obj_vnum'])
                    comment = f"  # {name}" if name else ""
                    lines.append(f"    obj_vnum: {cmd['obj_vnum']}{comment}")

            elif cmd_type == 'ATTACH_TRIGGER':
                if 'trigger_type' in cmd:
                    lines.append(f"    trigger_type: {cmd['trigger_type']}")
                if 'entity_vnum' in cmd:
                    lines.append(f"    entity_vnum: {cmd['entity_vnum']}")
                if 'trigger_vnum' in cmd:
                    name = get_trigger_name(cmd['trigger_vnum'])
                    comment = f"  # {name}" if name else ""
                    lines.append(f"    trigger_vnum: {cmd['trigger_vnum']}{comment}")

            elif cmd_type == 'SET_VARIABLE':
                if 'var_type' in cmd:
                    lines.append(f"    var_type: {cmd['var_type']}")
                if 'context' in cmd:
                    lines.append(f"    context: {cmd['context']}")
                if 'vnum' in cmd:
                    lines.append(f"    vnum: {cmd['vnum']}")
                if 'var_name' in cmd:
                    lines.append(f"    var_name: \"{cmd['var_name']}\"")
                if 'var_value' in cmd:
                    lines.append(f"    var_value: \"{cmd['var_value']}\"")

            elif cmd_type == 'SPEC_LOAD':
                if 'obj_vnum' in cmd:
                    name = get_obj_name(cmd['obj_vnum'])
                    comment = f"  # {name}" if name else ""
                    lines.append(f"    obj_vnum: {cmd['obj_vnum']}{comment}")
                if 'max_world' in cmd:
                    lines.append(f"    max_world: {cmd['max_world']}")
                if 'room_vnum' in cmd:
                    name = get_room_name(cmd['room_vnum'])
                    comment = f"  # {name}" if name else ""
                    lines.append(f"    room_vnum: {cmd['room_vnum']}{comment}")
                if 'load_prob' in cmd:
                    lines.append(f"    load_prob: {cmd['load_prob']}")

            if 'comment' in cmd:
                lines.append(f"    # {cmd['comment']}")

    return '\n'.join(lines)


def convert_file(input_path, output_path, file_type):
    """Convert a single file from old format to YAML."""
    print(f"Converting {input_path} -> {output_path}")

    # Create output directory if needed
    os.makedirs(os.path.dirname(output_path), exist_ok=True)

    if file_type == 'mob':
        entities = parse_mob_file(input_path)
        for entity in entities:
            yaml_content = mob_to_yaml(entity)
            entity_path = os.path.join(os.path.dirname(output_path), f"{entity['vnum']}.yaml")
            with open(entity_path, 'w', encoding='koi8-r') as f:
                f.write(yaml_content)
            print(f"  Created {entity_path}")

    elif file_type == 'obj':
        entities = parse_obj_file(input_path)
        for entity in entities:
            yaml_content = obj_to_yaml(entity)
            entity_path = os.path.join(os.path.dirname(output_path), f"{entity['vnum']}.yaml")
            with open(entity_path, 'w', encoding='koi8-r') as f:
                f.write(yaml_content)
            print(f"  Created {entity_path}")

    elif file_type == 'wld':
        entities = parse_wld_file(input_path)
        for entity in entities:
            yaml_content = room_to_yaml(entity)
            entity_path = os.path.join(os.path.dirname(output_path), f"{entity['vnum']}.yaml")
            with open(entity_path, 'w', encoding='koi8-r') as f:
                f.write(yaml_content)
            print(f"  Created {entity_path}")

    elif file_type == 'zon':
        zone = parse_zon_file(input_path)
        if zone:
            yaml_content = zon_to_yaml(zone)
            entity_path = os.path.join(os.path.dirname(output_path), f"{zone['vnum']}.yaml")
            with open(entity_path, 'w', encoding='koi8-r') as f:
                f.write(yaml_content)
            print(f"  Created {entity_path}")

    elif file_type == 'trg':
        entities = parse_trg_file(input_path)
        for entity in entities:
            yaml_content = trg_to_yaml(entity)
            entity_path = os.path.join(os.path.dirname(output_path), f"{entity['vnum']}.yaml")
            with open(entity_path, 'w', encoding='koi8-r') as f:
                f.write(yaml_content)
            print(f"  Created {entity_path}")

    else:
        print(f"  Skipping unsupported file type: {file_type}")


def convert_directory(input_dir, output_dir):
    """Convert all world files in a directory."""
    input_path = Path(input_dir)
    output_path = Path(output_dir)

    # Build name registries first for cross-references
    print("Building name registries...")
    build_name_registries(input_dir)

    # Process mob files
    print("\nConverting mob files...")
    for mob_file in input_path.glob('mob/*.mob'):
        out_dir = output_path / 'mob'
        convert_file(str(mob_file), str(out_dir / 'dummy.yaml'), 'mob')

    # Process obj files
    print("\nConverting obj files...")
    for obj_file in input_path.glob('obj/*.obj'):
        out_dir = output_path / 'obj'
        convert_file(str(obj_file), str(out_dir / 'dummy.yaml'), 'obj')

    # Process wld files
    print("\nConverting wld files...")
    for wld_file in input_path.glob('wld/*.wld'):
        out_dir = output_path / 'wld'
        convert_file(str(wld_file), str(out_dir / 'dummy.yaml'), 'wld')

    # Process zon files
    print("\nConverting zon files...")
    for zon_file in input_path.glob('zon/*.zon'):
        out_dir = output_path / 'zon'
        convert_file(str(zon_file), str(out_dir / 'dummy.yaml'), 'zon')

    # Process trg files
    print("\nConverting trg files...")
    for trg_file in input_path.glob('trg/*.trg'):
        out_dir = output_path / 'trg'
        convert_file(str(trg_file), str(out_dir / 'dummy.yaml'), 'trg')


def main():
    parser = argparse.ArgumentParser(description='Convert MUD world files to YAML format')
    parser.add_argument('--input', '-i', required=True, help='Input file or directory')
    parser.add_argument('--output', '-o', required=True, help='Output file or directory')
    parser.add_argument('--type', '-t', default='all', choices=['mob', 'obj', 'wld', 'zon', 'trg', 'all'],
                        help='File type to convert')

    args = parser.parse_args()

    input_path = Path(args.input)
    output_path = Path(args.output)

    if input_path.is_dir():
        convert_directory(str(input_path), str(output_path))
    else:
        # Determine file type from extension
        file_type = args.type
        if file_type == 'all':
            ext = input_path.suffix.lower()
            if ext == '.mob':
                file_type = 'mob'
            elif ext == '.obj':
                file_type = 'obj'
            elif ext == '.wld':
                file_type = 'wld'
            elif ext == '.zon':
                file_type = 'zon'
            elif ext == '.trg':
                file_type = 'trg'
            else:
                print(f"Unknown file type: {ext}")
                sys.exit(1)

        convert_file(str(input_path), str(output_path), file_type)

    print("Conversion complete!")


if __name__ == '__main__':
    main()

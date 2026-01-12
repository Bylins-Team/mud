# Identify Export Tool

Standalone utility for exporting all Bylins MUD item data and identify command results to a SQLite database.

## Overview

The identify-export tool processes all game objects from `.obj` files and generates a comprehensive SQLite database containing:

- **Normalized item data** - All object properties (vnum, type, material, weight, cost, etc.) stored in relational tables
- **Identify simulation results** - Full output of the `identify` (опознать) command at 9 different skill levels for every item
- **Human-readable format** - All enums, flags, and binary masks converted to readable Russian text
- **UTF-8 encoding** - All text properly converted from KOI8-R for external use

This database is designed for use by external tools, websites, mobile apps, or game documentation systems.

## Building

### Prerequisites

- CMake 3.10+
- C++20 compiler (GCC 9+, Clang 10+, MSVC 2019+)
- SQLite3 development libraries
- iconv library (for encoding conversion)

### Build Instructions

The tool is integrated into the main Bylins MUD build system but **disabled by default**.

To build:

```bash
cd /home/kvirund/repos/mud/build
cmake -DBUILD_IDENTIFY_EXPORT=ON ..
make identify-export
```

The binary will be placed in the `build/` directory alongside the main `circle` executable.

## Usage

The tool has three modes of operation:

### Mode 1: Export from .obj Files to SQLite Database

```bash
./identify-export --lib-path <path> --output <database-file>
```

The `--lib-path` parameter is flexible and accepts:
- Path to `lib` directory: `../lib.template` → will use `../lib.template/world/obj`
- Path to `world` directory: `../lib.template/world` → will use `../lib.template/world/obj`
- Path to `obj` directory: `../lib.template/world/obj` → will use directly

**Examples:**

All these commands are equivalent:
```bash
./identify-export --lib-path ../lib.template --output items.db
./identify-export --lib-path ../lib.template/world --output items.db
./identify-export --lib-path ../lib.template/world/obj --output items.db
```

Export from production library (just specify world directory):
```bash
./identify-export --lib-path ../lib/world --output bylins_items.db
```

### Mode 2: Export Identify Results to Markdown (from existing database)

```bash
./identify-export --input-db <database-file> --export-markdown <output.md> [--skill-levels <levels>]
```

**Examples:**

Export with default settings (skill level 100):
```bash
./identify-export --input-db items.db --export-markdown items.md
```

Export specific skill levels:
```bash
./identify-export --input-db items.db --export-markdown items.md --skill-levels 0,30,60,100
```

Export immortal-level details:
```bash
./identify-export --input-db items.db --export-markdown items.md --skill-levels 400
```

### Mode 3: Combined Mode (create database + export Markdown in one command)

```bash
./identify-export --lib-path <path> --output <database-file> --export-markdown <output.md> [--skill-levels <levels>]
```

This mode combines database creation and Markdown export in a single command, saving time when you need both outputs.

**Examples:**

Create database and export to Markdown with default settings:
```bash
./identify-export --lib-path ../lib/world --output items.db --export-markdown items.md
```

Create database and export specific skill levels:
```bash
./identify-export --lib-path ../lib/world --output items.db --export-markdown items.md --skill-levels 0,60,100
```

### What Gets Exported

The tool processes all `.obj` files from:
- `<lib-path>/world/obj/*.obj`
- Subdirectories are not scanned

For each object, the tool:
1. Parses all object properties (vnum, type, material, flags, affects, etc.)
2. Simulates the `identify` command at 9 skill levels: **0, 20, 30, 40, 60, 75, 90, 100, 400**
3. Captures the full Russian text output for each skill level
4. Converts all text from KOI8-R to UTF-8
5. Saves everything to a normalized SQLite database

### Performance

- Processing ~200 objects: **< 5 seconds**
- Processing ~1000 objects: **< 30 seconds**
- Uses SQLite transactions and prepared statements for optimal performance

## Database Schema

The database uses a **normalized relational schema** with 18 tables and 4 views.

### Core Tables

#### `items` (Main Item Data)
Stores scalar properties for each item.

| Column | Type | Description |
|--------|------|-------------|
| vnum | INTEGER PRIMARY KEY | Virtual number (unique item ID) |
| short_description | TEXT NOT NULL | Brief item name shown in inventory |
| name_nominative | TEXT | Именительный падеж (кто? что?) |
| name_genitive | TEXT | Родительный падеж (кого? чего?) |
| name_dative | TEXT | Дательный падеж (кому? чему?) |
| name_accusative | TEXT | Винительный падеж (кого? что?) |
| name_instrumental | TEXT | Творительный падеж (кем? чем?) |
| name_prepositional | TEXT | Предложный падеж (о ком? о чём?) |
| description | TEXT | Long description when item is in room |
| action_description | TEXT | Action text (for certain items) |
| type | TEXT NOT NULL | Human-readable: "оружие", "доспех", "зелье" |
| material | TEXT | Human-readable: "железо", "дерево", "сталь" |
| weight | INTEGER | Weight in game units |
| cost | INTEGER | Base cost in gold |
| rent_off | INTEGER | Rent cost when offline |
| rent_on | INTEGER | Rent cost when online |
| max_durability | INTEGER | Maximum durability |
| current_durability | INTEGER | Current durability |
| level | INTEGER | Minimum level to use |
| sex | TEXT | "мужской", "женский", "средний" |
| minimum_remorts | INTEGER | Minimum remorts required |
| source_file | TEXT | Which .obj file it came from |
| created_at | TIMESTAMP | When exported |

#### `item_aliases` (Keywords)
Space-separated aliases split into individual rows.

| Column | Type | Description |
|--------|------|-------------|
| id | INTEGER PRIMARY KEY | Auto-increment |
| vnum | INTEGER FK | References items(vnum) |
| alias | TEXT NOT NULL | Single keyword for matching |

**Indexes**: `idx_alias_vnum` (vnum), `idx_alias_text` (alias)

#### `item_extra_flags` (Special Flags)
Item special properties like "светится" (glows), "невидимый" (invisible).

| Column | Type | Description |
|--------|------|-------------|
| id | INTEGER PRIMARY KEY | Auto-increment |
| vnum | INTEGER FK | References items(vnum) |
| flag | TEXT NOT NULL | Human-readable flag name |

**Indexes**: `idx_extra_flag_vnum` (vnum), `idx_extra_flag` (flag)

#### `item_wear_flags` (Wear Positions)
Where item can be worn: "на пальце", "на шею", "на тело", "можно взять".

#### `item_anti_flags` (Class Restrictions)
Who CANNOT use: "анти-воин", "анти-маг", "анти-вор".

#### `item_no_flags` (Usage Restrictions)
Gender/alignment restrictions: "мужчина", "женщина".

#### `item_affect_flags` (Weapon Abilities)
Special weapon/armor abilities: "полет", "невидимость", "защита от тьмы".

#### `item_affects` (Stat Modifiers)
Character stat bonuses/penalties.

| Column | Type | Description |
|--------|------|-------------|
| id | INTEGER PRIMARY KEY | Auto-increment |
| vnum | INTEGER FK | References items(vnum) |
| apply_type | TEXT NOT NULL | "сила", "ловкость", "hp", "мана" |
| modifier | INTEGER NOT NULL | Positive or negative value |

**Indexes**: `idx_affect_vnum` (vnum), `idx_affect_type` (apply_type)

#### `item_skills` (Skill Bonuses)
Skill bonuses/penalties (not commonly used).

| Column | Type | Description |
|--------|------|-------------|
| id | INTEGER PRIMARY KEY | Auto-increment |
| vnum | INTEGER FK | References items(vnum) |
| skill_name | TEXT NOT NULL | Human-readable skill name |
| modifier | INTEGER NOT NULL | Can be negative |

#### `identify_results` (Identify Command Output)
**This is the core feature** - stores full identify output at each skill level.

| Column | Type | Description |
|--------|------|-------------|
| id | INTEGER PRIMARY KEY | Auto-increment |
| vnum | INTEGER FK | References items(vnum) |
| skill_level | INTEGER NOT NULL | 0, 20, 30, 40, 60, 75, 90, 100, or 400 |
| output_text | TEXT NOT NULL | Full captured output (UTF-8) |

**Constraints**: UNIQUE(vnum, skill_level)
**Indexes**: `idx_identify_vnum` (vnum), `idx_identify_level` (skill_level)

### Type-Specific Value Tables

#### `item_weapon_values` (Weapons)
```sql
vnum INTEGER PRIMARY KEY FK
damage_dice_num INTEGER    -- Number of dice
damage_dice_size INTEGER   -- Dice size
avg_damage REAL           -- Calculated average
```

#### `item_armor_values` (Armor)
```sql
vnum INTEGER PRIMARY KEY FK
ac INTEGER        -- Armor class
armor INTEGER     -- Armor value
```

#### `item_potion_values` (Potions/Scrolls)
```sql
id INTEGER PRIMARY KEY
vnum INTEGER FK
spell_num INTEGER NOT NULL    -- Spell slot (1, 2, or 3)
spell_name TEXT NOT NULL      -- Human-readable spell name
spell_level INTEGER           -- Spell level
```

Other type-specific tables: `item_container_values`, `item_book_values`, `item_wand_staff_values`, `item_light_values`, `item_ingredient_values`.

### Views (Aggregated Data)

#### `v_items_full`
Items with all flags as comma-separated strings (similar to original flat schema).

```sql
SELECT * FROM v_items_full WHERE vnum = 100;
-- Returns item with: aliases, extra_flags, wear_flags, anti_flags, no_flags, affect_flags
```

#### `v_weapons`
All weapons with damage information.

```sql
SELECT short_description, damage_dice, avg_damage
FROM v_weapons
ORDER BY avg_damage DESC
LIMIT 10;
```

#### `v_armor`
All armor with AC/armor values.

```sql
SELECT short_description, ac, armor
FROM v_armor
WHERE ac > 10;
```

#### `v_item_affects_summary`
Affects aggregated into comma-separated summary per item.

#### `v_identify_results`
Identify results with item names and types for convenient browsing.

```sql
-- Get identify results with item names
SELECT vnum, short_description, type, skill_level, output_text
FROM v_identify_results
WHERE vnum = 100
ORDER BY skill_level;

-- Find all items with their identify text at skill level 100
SELECT short_description, output_text
FROM v_identify_results
WHERE skill_level = 100
ORDER BY vnum;
```

## Exporting to Markdown

The tool can export identify results to a Markdown file for easy viewing and documentation.

### Basic Export

Export with maximum detail (skill level 100 by default):

```bash
./identify-export --input-db items.db --export-markdown items.md
```

### Custom Skill Levels

Export specific skill levels:

```bash
# Export only basic info and full details
./identify-export --input-db items.db --export-markdown items.md --skill-levels 0,100

# Export progression: minimal, medium, maximum
./identify-export --input-db items.db --export-markdown items.md --skill-levels 30,60,100

# Export immortal-level details
./identify-export --input-db items.db --export-markdown items.md --skill-levels 400
```

The generated Markdown file will contain:
- Table of contents with item vnums and names
- Formatted identify output for each item at requested skill levels
- Easy to read in any Markdown viewer or convert to HTML/PDF

## Sample Queries

### Get All Identify Results for an Item

```sql
SELECT skill_level, output_text
FROM identify_results
WHERE vnum = 100
ORDER BY skill_level;
```

### Find Items with Specific Flag

```sql
SELECT i.vnum, i.short_description
FROM items i
JOIN item_extra_flags e ON i.vnum = e.vnum
WHERE e.flag = 'магический';
```

### Find Best Weapons

```sql
SELECT short_description, damage_dice, avg_damage, cost
FROM v_weapons
ORDER BY avg_damage DESC
LIMIT 20;
```

### Find Items with Strength Bonus

```sql
SELECT i.short_description, a.modifier
FROM items i
JOIN item_affects a ON i.vnum = a.vnum
WHERE a.apply_type = 'сила'
ORDER BY a.modifier DESC;
```

### Search by Keyword

```sql
SELECT DISTINCT i.vnum, i.short_description
FROM items i
JOIN item_aliases a ON i.vnum = a.vnum
WHERE a.alias LIKE 'меч%';
```

### Get All Armor for Warriors

```sql
SELECT i.short_description, a.ac, a.armor
FROM items i
JOIN item_armor_values a ON i.vnum = a.vnum
LEFT JOIN item_anti_flags af ON i.vnum = af.vnum AND af.flag = 'анти-воин'
WHERE af.flag IS NULL  -- Not anti-warrior
ORDER BY a.armor DESC;
```

### Full Item Details with All Related Data

```sql
-- Get item with all its properties
SELECT * FROM v_items_full WHERE vnum = 100;

-- Get all affects
SELECT apply_type, modifier FROM item_affects WHERE vnum = 100;

-- Get identify at specific skill level
SELECT output_text FROM identify_results WHERE vnum = 100 AND skill_level = 100;
```

## Identify Skill Levels Explained

The `identify` (опознать) command reveals different information based on skill level:

| Skill Level | Information Revealed |
|-------------|---------------------|
| **0** | Item name and type only |
| **20** | Weight, cost, rent |
| **30** | Material, durability |
| **40** | Restrictions (no_flags) |
| **50** | Anti-flags (class restrictions), minimum remorts |
| **60** | Extra flags (special properties) |
| **75** | Type-specific values (weapon damage, armor AC, spell effects) |
| **90** | Weapon affects (special abilities) |
| **100** | Stat modifiers (strength, dexterity, hp, mana, etc.) |
| **400** | Extended information (immortal level) |

The tool generates results for levels: **0, 20, 30, 40, 60, 75, 90, 100, 400** to cover all information tiers.

## Technical Details

### Architecture

- **Standalone tool** - Links against `circle.library` to reuse MUD parsing and display logic
- **Identify simulation** - `SimulateIdentify()` function reproduces `mort_show_obj_values()` logic
- **No MUD boot required** - Works directly with parsed object data, doesn't need full game initialization
- **Message capture** - Uses thread-local `MessageCapture` to intercept `SendMsgToChar()` output
- **Encoding conversion** - All text converted from KOI8-R to UTF-8 using `iconv`

### Key Files

```
tools/identify-export/
├── CMakeLists.txt              # Build configuration
├── README.md                   # This file
├── src/
│   ├── main.cpp               # Entry point
│   ├── exporter.cpp/.h        # Main exporter class
│   ├── identify_simulation.cpp/.h  # Identify logic simulation
│   ├── db_writer.cpp/.h       # SQLite database writer
│   └── encoding.cpp/.h        # KOI8-R to UTF-8 conversion
```

### Dependencies on MUD Code

The tool links against `circle.library` and uses:
- Object file parser from `src/engine/boot/boot_data_files.cpp`
- Constant arrays: `item_types[]`, `material_name[]`, `apply_types[]`
- Flag processing from `src/structs/flag_data.h`
- `MessageCapture` from `src/utils/message_capture.cpp`

## Maintenance

### Re-exporting After Data Changes

When game data files (.obj files) are updated, simply re-run the tool:

```bash
./identify-export --lib-path ../lib --output items_updated.db
```

The tool always creates a fresh database (overwrites existing file).

### Adding New Object Types

If new object types are added to the MUD:

1. Update `identify_simulation.cpp` to handle new type in `SimulateIdentify()`
2. Add new type-specific value table in `db_writer.cpp` schema
3. Extract type-specific values in `exporter.cpp`
4. Rebuild and test

### Verifying Export

Use the included verification script:

```bash
cd build
python3 check_identify.py
```

This displays:
- Total item count
- Results count by skill level
- Sample identify output for vnum 100

## Troubleshooting

### "Failed to open database"
- Check write permissions in output directory
- Ensure SQLite3 is installed

### "No objects found"
- Verify `--lib-path` points to correct directory
- Check that directory contains `world/obj/*.obj` files

### "UTF-8 decode error"
- This should not happen - encoding conversion is automatic
- If it occurs, check that source .obj files are in KOI8-R encoding

### Empty identify_results table
- Verify objects have valid type/material values
- Check logs for simulation errors

## Integration Examples

### Web Application

```python
import sqlite3

conn = sqlite3.connect('items.db')
cursor = conn.cursor()

# Get item details
cursor.execute("""
    SELECT short_description, type, weight, cost
    FROM items WHERE vnum = ?
""", (vnum,))

# Get identify text for skill level 100
cursor.execute("""
    SELECT output_text FROM identify_results
    WHERE vnum = ? AND skill_level = 100
""", (vnum,))
```

### Mobile App

The normalized schema makes it easy to query specific aspects:

```sql
-- Get all flags efficiently
SELECT flag FROM item_extra_flags WHERE vnum = ?;

-- Search by multiple criteria
SELECT DISTINCT i.vnum, i.short_description
FROM items i
JOIN item_affects a ON i.vnum = a.vnum
WHERE i.type = 'оружие'
  AND a.apply_type = 'сила'
  AND a.modifier > 0;
```

## License

This tool is part of Bylins MUD and follows the same license as the main project.

## Contributing

When making changes:

1. Follow C++20 style (tabs, braces on new line)
2. Test with multiple .obj file sets
3. Verify UTF-8 encoding correctness
4. Update this README if schema changes

## See Also

- Main MUD documentation: `/home/kvirund/repos/mud/CLAUDE.md`
- Object file format: `lib.template/world/obj/*.obj`
- Identify command: `src/gameplay/skills/identify.cpp`
- Identify display logic: `src/gameplay/magic/spells.cpp` (mort_show_obj_values)

---

**Version**: 1.0
**Last Updated**: 2026-01-11
**Status**: Production Ready

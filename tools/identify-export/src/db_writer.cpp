#include "db_writer.h"
#include <iostream>
#include <cstring>

DatabaseWriter::DatabaseWriter()
	: db_(nullptr)
	, stmt_insert_item_(nullptr)
	, stmt_insert_alias_(nullptr)
	, stmt_insert_extra_flag_(nullptr)
	, stmt_insert_wear_flag_(nullptr)
	, stmt_insert_anti_flag_(nullptr)
	, stmt_insert_no_flag_(nullptr)
	, stmt_insert_affect_flag_(nullptr)
	, stmt_insert_affect_(nullptr)
	, stmt_insert_skill_(nullptr)
	, stmt_insert_identify_(nullptr)
	, stmt_insert_weapon_(nullptr)
	, stmt_insert_armor_(nullptr)
	, stmt_insert_potion_(nullptr)
	, stmt_insert_container_(nullptr)
	, stmt_insert_book_(nullptr)
	, stmt_insert_wand_staff_(nullptr)
	, stmt_insert_light_(nullptr)
	, stmt_insert_ingredient_(nullptr)
{
}

DatabaseWriter::~DatabaseWriter()
{
	Close();
}

bool DatabaseWriter::Open(const std::string& db_path)
{
	int rc = sqlite3_open(db_path.c_str(), &db_);
	if (rc != SQLITE_OK)
	{
		last_error_ = std::string("Cannot open database: ") + sqlite3_errmsg(db_);
		return false;
	}
	return true;
}

void DatabaseWriter::Close()
{
	FinalizeStatements();

	if (db_)
	{
		sqlite3_close(db_);
		db_ = nullptr;
	}
}

bool DatabaseWriter::ExecuteSQL(const char* sql)
{
	char* errmsg = nullptr;
	int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &errmsg);

	if (rc != SQLITE_OK)
	{
		last_error_ = std::string("SQL error: ") + (errmsg ? errmsg : "unknown");
		if (errmsg)
		{
			sqlite3_free(errmsg);
		}
		return false;
	}

	return true;
}

bool DatabaseWriter::CreateTables()
{
	// Таблица items (основные данные предметов)
	const char* sql_items = R"(
		CREATE TABLE IF NOT EXISTS items (
			vnum INTEGER PRIMARY KEY,
			short_description TEXT NOT NULL,
			name_nominative TEXT,
			name_genitive TEXT,
			name_dative TEXT,
			name_accusative TEXT,
			name_instrumental TEXT,
			name_prepositional TEXT,
			description TEXT,
			action_description TEXT,
			type TEXT NOT NULL,
			material TEXT,
			weight INTEGER,
			cost INTEGER,
			rent_off INTEGER,
			rent_on INTEGER,
			max_durability INTEGER,
			current_durability INTEGER,
			level INTEGER,
			sex TEXT,
			minimum_remorts INTEGER,
			source_file TEXT,
			created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
		)
	)";

	if (!ExecuteSQL(sql_items))
	{
		return false;
	}

	// Таблица item_aliases
	const char* sql_aliases = R"(
		CREATE TABLE IF NOT EXISTS item_aliases (
			id INTEGER PRIMARY KEY AUTOINCREMENT,
			vnum INTEGER NOT NULL,
			alias TEXT NOT NULL,
			FOREIGN KEY (vnum) REFERENCES items(vnum)
		)
	)";

	if (!ExecuteSQL(sql_aliases))
	{
		return false;
	}

	// Таблица item_extra_flags
	const char* sql_extra_flags = R"(
		CREATE TABLE IF NOT EXISTS item_extra_flags (
			id INTEGER PRIMARY KEY AUTOINCREMENT,
			vnum INTEGER NOT NULL,
			flag TEXT NOT NULL,
			FOREIGN KEY (vnum) REFERENCES items(vnum)
		)
	)";

	if (!ExecuteSQL(sql_extra_flags))
	{
		return false;
	}

	// Таблица item_wear_flags
	const char* sql_wear_flags = R"(
		CREATE TABLE IF NOT EXISTS item_wear_flags (
			id INTEGER PRIMARY KEY AUTOINCREMENT,
			vnum INTEGER NOT NULL,
			flag TEXT NOT NULL,
			FOREIGN KEY (vnum) REFERENCES items(vnum)
		)
	)";

	if (!ExecuteSQL(sql_wear_flags))
	{
		return false;
	}

	// Таблица item_anti_flags
	const char* sql_anti_flags = R"(
		CREATE TABLE IF NOT EXISTS item_anti_flags (
			id INTEGER PRIMARY KEY AUTOINCREMENT,
			vnum INTEGER NOT NULL,
			flag TEXT NOT NULL,
			FOREIGN KEY (vnum) REFERENCES items(vnum)
		)
	)";

	if (!ExecuteSQL(sql_anti_flags))
	{
		return false;
	}

	// Таблица item_no_flags
	const char* sql_no_flags = R"(
		CREATE TABLE IF NOT EXISTS item_no_flags (
			id INTEGER PRIMARY KEY AUTOINCREMENT,
			vnum INTEGER NOT NULL,
			flag TEXT NOT NULL,
			FOREIGN KEY (vnum) REFERENCES items(vnum)
		)
	)";

	if (!ExecuteSQL(sql_no_flags))
	{
		return false;
	}

	// Таблица item_affect_flags
	const char* sql_affect_flags = R"(
		CREATE TABLE IF NOT EXISTS item_affect_flags (
			id INTEGER PRIMARY KEY AUTOINCREMENT,
			vnum INTEGER NOT NULL,
			flag TEXT NOT NULL,
			FOREIGN KEY (vnum) REFERENCES items(vnum)
		)
	)";

	if (!ExecuteSQL(sql_affect_flags))
	{
		return false;
	}

	// Таблица item_affects
	const char* sql_affects = R"(
		CREATE TABLE IF NOT EXISTS item_affects (
			id INTEGER PRIMARY KEY AUTOINCREMENT,
			vnum INTEGER NOT NULL,
			apply_type TEXT NOT NULL,
			modifier INTEGER NOT NULL,
			FOREIGN KEY (vnum) REFERENCES items(vnum)
		)
	)";

	if (!ExecuteSQL(sql_affects))
	{
		return false;
	}

	// Таблица item_skills
	const char* sql_skills = R"(
		CREATE TABLE IF NOT EXISTS item_skills (
			id INTEGER PRIMARY KEY AUTOINCREMENT,
			vnum INTEGER NOT NULL,
			skill_name TEXT NOT NULL,
			modifier INTEGER NOT NULL,
			FOREIGN KEY (vnum) REFERENCES items(vnum)
		)
	)";

	if (!ExecuteSQL(sql_skills))
	{
		return false;
	}

	// Таблица identify_results
	const char* sql_identify = R"(
		CREATE TABLE IF NOT EXISTS identify_results (
			id INTEGER PRIMARY KEY AUTOINCREMENT,
			vnum INTEGER NOT NULL,
			skill_level INTEGER NOT NULL,
			output_text TEXT NOT NULL,
			FOREIGN KEY (vnum) REFERENCES items(vnum),
			UNIQUE(vnum, skill_level)
		)
	)";

	if (!ExecuteSQL(sql_identify))
	{
		return false;
	}

	// Таблица item_weapon_values
	const char* sql_weapon = R"(
		CREATE TABLE IF NOT EXISTS item_weapon_values (
			vnum INTEGER PRIMARY KEY,
			damage_dice_num INTEGER,
			damage_dice_size INTEGER,
			avg_damage REAL,
			FOREIGN KEY (vnum) REFERENCES items(vnum)
		)
	)";

	if (!ExecuteSQL(sql_weapon))
	{
		return false;
	}

	// Таблица item_armor_values
	const char* sql_armor = R"(
		CREATE TABLE IF NOT EXISTS item_armor_values (
			vnum INTEGER PRIMARY KEY,
			ac INTEGER,
			armor INTEGER,
			FOREIGN KEY (vnum) REFERENCES items(vnum)
		)
	)";

	if (!ExecuteSQL(sql_armor))
	{
		return false;
	}

	// Таблица item_potion_values
	const char* sql_potion = R"(
		CREATE TABLE IF NOT EXISTS item_potion_values (
			id INTEGER PRIMARY KEY AUTOINCREMENT,
			vnum INTEGER NOT NULL,
			spell_num INTEGER NOT NULL,
			spell_name TEXT NOT NULL,
			spell_level INTEGER,
			FOREIGN KEY (vnum) REFERENCES items(vnum)
		)
	)";

	if (!ExecuteSQL(sql_potion))
	{
		return false;
	}

	// Таблица item_container_values
	const char* sql_container = R"(
		CREATE TABLE IF NOT EXISTS item_container_values (
			vnum INTEGER PRIMARY KEY,
			capacity INTEGER,
			is_corpse BOOLEAN,
			FOREIGN KEY (vnum) REFERENCES items(vnum)
		)
	)";

	if (!ExecuteSQL(sql_container))
	{
		return false;
	}

	// Таблица item_book_values
	const char* sql_book = R"(
		CREATE TABLE IF NOT EXISTS item_book_values (
			vnum INTEGER PRIMARY KEY,
			spell_name TEXT,
			skill_name TEXT,
			FOREIGN KEY (vnum) REFERENCES items(vnum)
		)
	)";

	if (!ExecuteSQL(sql_book))
	{
		return false;
	}

	// Таблица item_wand_staff_values
	const char* sql_wand_staff = R"(
		CREATE TABLE IF NOT EXISTS item_wand_staff_values (
			vnum INTEGER PRIMARY KEY,
			spell_name TEXT NOT NULL,
			spell_level INTEGER,
			charges_max INTEGER,
			charges_remaining INTEGER,
			FOREIGN KEY (vnum) REFERENCES items(vnum)
		)
	)";

	if (!ExecuteSQL(sql_wand_staff))
	{
		return false;
	}

	// Таблица item_light_values
	const char* sql_light = R"(
		CREATE TABLE IF NOT EXISTS item_light_values (
			vnum INTEGER PRIMARY KEY,
			hours INTEGER,
			FOREIGN KEY (vnum) REFERENCES items(vnum)
		)
	)";

	if (!ExecuteSQL(sql_light))
	{
		return false;
	}

	// Таблица item_ingredient_values
	const char* sql_ingredient = R"(
		CREATE TABLE IF NOT EXISTS item_ingredient_values (
			vnum INTEGER PRIMARY KEY,
			potion_proto INTEGER,
			FOREIGN KEY (vnum) REFERENCES items(vnum)
		)
	)";

	if (!ExecuteSQL(sql_ingredient))
	{
		return false;
	}

	return true;
}

bool DatabaseWriter::CreateIndexes()
{
	std::cout << "Creating indexes...\n";

	const char* indexes[] = {
		"CREATE INDEX IF NOT EXISTS idx_alias_vnum ON item_aliases(vnum)",
		"CREATE INDEX IF NOT EXISTS idx_alias_text ON item_aliases(alias)",
		"CREATE INDEX IF NOT EXISTS idx_extra_flag_vnum ON item_extra_flags(vnum)",
		"CREATE INDEX IF NOT EXISTS idx_extra_flag ON item_extra_flags(flag)",
		"CREATE INDEX IF NOT EXISTS idx_wear_flag_vnum ON item_wear_flags(vnum)",
		"CREATE INDEX IF NOT EXISTS idx_anti_flag_vnum ON item_anti_flags(vnum)",
		"CREATE INDEX IF NOT EXISTS idx_no_flag_vnum ON item_no_flags(vnum)",
		"CREATE INDEX IF NOT EXISTS idx_affect_flag_vnum ON item_affect_flags(vnum)",
		"CREATE INDEX IF NOT EXISTS idx_affect_vnum ON item_affects(vnum)",
		"CREATE INDEX IF NOT EXISTS idx_affect_type ON item_affects(apply_type)",
		"CREATE INDEX IF NOT EXISTS idx_skill_vnum ON item_skills(vnum)",
		"CREATE INDEX IF NOT EXISTS idx_identify_vnum ON identify_results(vnum)",
		"CREATE INDEX IF NOT EXISTS idx_identify_level ON identify_results(skill_level)",
		"CREATE INDEX IF NOT EXISTS idx_potion_vnum ON item_potion_values(vnum)",
		nullptr
	};

	for (int i = 0; indexes[i] != nullptr; ++i)
	{
		if (!ExecuteSQL(indexes[i]))
		{
			return false;
		}
	}

	return true;
}

bool DatabaseWriter::CreateViews()
{
	std::cout << "Creating views...\n";

	// View: v_items_full (с агрегированными флагами)
	const char* sql_view_items_full = R"(
		CREATE VIEW IF NOT EXISTS v_items_full AS
		SELECT
			i.*,
			(SELECT GROUP_CONCAT(alias, ' ') FROM item_aliases WHERE vnum = i.vnum) as aliases,
			(SELECT GROUP_CONCAT(flag, ', ') FROM item_extra_flags WHERE vnum = i.vnum) as extra_flags,
			(SELECT GROUP_CONCAT(flag, ', ') FROM item_wear_flags WHERE vnum = i.vnum) as wear_flags,
			(SELECT GROUP_CONCAT(flag, ', ') FROM item_anti_flags WHERE vnum = i.vnum) as anti_flags,
			(SELECT GROUP_CONCAT(flag, ', ') FROM item_no_flags WHERE vnum = i.vnum) as no_flags,
			(SELECT GROUP_CONCAT(flag, ', ') FROM item_affect_flags WHERE vnum = i.vnum) as affect_flags
		FROM items i
	)";

	if (!ExecuteSQL(sql_view_items_full))
	{
		return false;
	}

	// View: v_weapons
	const char* sql_view_weapons = R"(
		CREATE VIEW IF NOT EXISTS v_weapons AS
		SELECT
			i.*,
			w.damage_dice_num,
			w.damage_dice_size,
			w.avg_damage,
			w.damage_dice_num || 'd' || w.damage_dice_size as damage_dice
		FROM items i
		JOIN item_weapon_values w ON i.vnum = w.vnum
	)";

	if (!ExecuteSQL(sql_view_weapons))
	{
		return false;
	}

	// View: v_armor
	const char* sql_view_armor = R"(
		CREATE VIEW IF NOT EXISTS v_armor AS
		SELECT
			i.*,
			a.ac,
			a.armor
		FROM items i
		JOIN item_armor_values a ON i.vnum = a.vnum
	)";

	if (!ExecuteSQL(sql_view_armor))
	{
		return false;
	}

	// View: v_item_affects_summary
	const char* sql_view_affects = R"(
		CREATE VIEW IF NOT EXISTS v_item_affects_summary AS
		SELECT
			vnum,
			GROUP_CONCAT(apply_type || ' ' ||
						 CASE WHEN modifier > 0 THEN '+' ELSE '' END ||
						 modifier, ', ') as affects_summary
		FROM item_affects
		GROUP BY vnum
	)";

	if (!ExecuteSQL(sql_view_affects))
	{
		return false;
	}

	// View: v_identify_results (identify results with item names)
	const char* sql_view_identify = R"(
		CREATE VIEW IF NOT EXISTS v_identify_results AS
		SELECT
			ir.id,
			ir.vnum,
			i.short_description,
			i.type,
			ir.skill_level,
			ir.output_text
		FROM identify_results ir
		JOIN items i ON ir.vnum = i.vnum
	)";

	if (!ExecuteSQL(sql_view_identify))
	{
		return false;
	}

	return true;
}

bool DatabaseWriter::BeginTransaction()
{
	return ExecuteSQL("BEGIN TRANSACTION");
}

bool DatabaseWriter::CommitTransaction()
{
	return ExecuteSQL("COMMIT");
}

bool DatabaseWriter::RollbackTransaction()
{
	return ExecuteSQL("ROLLBACK");
}

bool DatabaseWriter::DeleteItemRelatedData(int vnum)
{
	// Удалить все связанные данные для vnum перед заменой предмета
	const char* delete_queries[] = {
		"DELETE FROM item_aliases WHERE vnum = ?",
		"DELETE FROM item_extra_flags WHERE vnum = ?",
		"DELETE FROM item_wear_flags WHERE vnum = ?",
		"DELETE FROM item_anti_flags WHERE vnum = ?",
		"DELETE FROM item_no_flags WHERE vnum = ?",
		"DELETE FROM item_affect_flags WHERE vnum = ?",
		"DELETE FROM item_affects WHERE vnum = ?",
		"DELETE FROM item_skills WHERE vnum = ?",
		"DELETE FROM identify_results WHERE vnum = ?",
		"DELETE FROM item_weapon_values WHERE vnum = ?",
		"DELETE FROM item_armor_values WHERE vnum = ?",
		"DELETE FROM item_potion_values WHERE vnum = ?",
		"DELETE FROM item_container_values WHERE vnum = ?",
		"DELETE FROM item_book_values WHERE vnum = ?",
		"DELETE FROM item_wand_staff_values WHERE vnum = ?",
		"DELETE FROM item_light_values WHERE vnum = ?",
		"DELETE FROM item_ingredient_values WHERE vnum = ?",
		nullptr
	};

	for (int i = 0; delete_queries[i] != nullptr; ++i)
	{
		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(db_, delete_queries[i], -1, &stmt, nullptr) == SQLITE_OK)
		{
			sqlite3_bind_int(stmt, 1, vnum);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
	}

	return true;
}

std::string DatabaseWriter::GetLastError() const
{
	return last_error_;
}

bool DatabaseWriter::PrepareStatements()
{
	// Подготовить prepared statement для вставки предмета (с заменой при дубликате)
	const char* sql_insert_item = R"(
		INSERT OR REPLACE INTO items (
			vnum, short_description, name_nominative, name_genitive, name_dative,
			name_accusative, name_instrumental, name_prepositional,
			description, action_description, type, material, weight, cost,
			rent_off, rent_on, max_durability, current_durability, level,
			sex, minimum_remorts, source_file
		) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
	)";

	if (sqlite3_prepare_v2(db_, sql_insert_item, -1, &stmt_insert_item_, nullptr) != SQLITE_OK)
	{
		last_error_ = std::string("Failed to prepare insert_item: ") + sqlite3_errmsg(db_);
		return false;
	}

	// Подготовить prepared statement для вставки алиаса
	const char* sql_insert_alias = "INSERT INTO item_aliases (vnum, alias) VALUES (?, ?)";
	if (sqlite3_prepare_v2(db_, sql_insert_alias, -1, &stmt_insert_alias_, nullptr) != SQLITE_OK)
	{
		last_error_ = std::string("Failed to prepare insert_alias: ") + sqlite3_errmsg(db_);
		return false;
	}

	// Подготовить prepared statements для флагов
	const char* sql_insert_extra_flag = "INSERT INTO item_extra_flags (vnum, flag) VALUES (?, ?)";
	if (sqlite3_prepare_v2(db_, sql_insert_extra_flag, -1, &stmt_insert_extra_flag_, nullptr) != SQLITE_OK)
	{
		last_error_ = std::string("Failed to prepare insert_extra_flag: ") + sqlite3_errmsg(db_);
		return false;
	}

	const char* sql_insert_wear_flag = "INSERT INTO item_wear_flags (vnum, flag) VALUES (?, ?)";
	if (sqlite3_prepare_v2(db_, sql_insert_wear_flag, -1, &stmt_insert_wear_flag_, nullptr) != SQLITE_OK)
	{
		last_error_ = std::string("Failed to prepare insert_wear_flag: ") + sqlite3_errmsg(db_);
		return false;
	}

	const char* sql_insert_anti_flag = "INSERT INTO item_anti_flags (vnum, flag) VALUES (?, ?)";
	if (sqlite3_prepare_v2(db_, sql_insert_anti_flag, -1, &stmt_insert_anti_flag_, nullptr) != SQLITE_OK)
	{
		last_error_ = std::string("Failed to prepare insert_anti_flag: ") + sqlite3_errmsg(db_);
		return false;
	}

	const char* sql_insert_no_flag = "INSERT INTO item_no_flags (vnum, flag) VALUES (?, ?)";
	if (sqlite3_prepare_v2(db_, sql_insert_no_flag, -1, &stmt_insert_no_flag_, nullptr) != SQLITE_OK)
	{
		last_error_ = std::string("Failed to prepare insert_no_flag: ") + sqlite3_errmsg(db_);
		return false;
	}

	const char* sql_insert_affect_flag = "INSERT INTO item_affect_flags (vnum, flag) VALUES (?, ?)";
	if (sqlite3_prepare_v2(db_, sql_insert_affect_flag, -1, &stmt_insert_affect_flag_, nullptr) != SQLITE_OK)
	{
		last_error_ = std::string("Failed to prepare insert_affect_flag: ") + sqlite3_errmsg(db_);
		return false;
	}

	// Подготовить prepared statement для аффектов
	const char* sql_insert_affect = "INSERT INTO item_affects (vnum, apply_type, modifier) VALUES (?, ?, ?)";
	if (sqlite3_prepare_v2(db_, sql_insert_affect, -1, &stmt_insert_affect_, nullptr) != SQLITE_OK)
	{
		last_error_ = std::string("Failed to prepare insert_affect: ") + sqlite3_errmsg(db_);
		return false;
	}

	const char* sql_insert_skill = "INSERT INTO item_skills (vnum, skill_name, modifier) VALUES (?, ?, ?)";
	if (sqlite3_prepare_v2(db_, sql_insert_skill, -1, &stmt_insert_skill_, nullptr) != SQLITE_OK)
	{
		last_error_ = std::string("Failed to prepare insert_skill: ") + sqlite3_errmsg(db_);
		return false;
	}

	const char* sql_insert_weapon = "INSERT INTO item_weapon_values (vnum, damage_dice_num, damage_dice_size, avg_damage) VALUES (?, ?, ?, ?)";
	if (sqlite3_prepare_v2(db_, sql_insert_weapon, -1, &stmt_insert_weapon_, nullptr) != SQLITE_OK)
	{
		last_error_ = std::string("Failed to prepare insert_weapon: ") + sqlite3_errmsg(db_);
		return false;
	}

	const char* sql_insert_armor = "INSERT INTO item_armor_values (vnum, ac, armor) VALUES (?, ?, ?)";
	if (sqlite3_prepare_v2(db_, sql_insert_armor, -1, &stmt_insert_armor_, nullptr) != SQLITE_OK)
	{
		last_error_ = std::string("Failed to prepare insert_armor: ") + sqlite3_errmsg(db_);
		return false;
	}

	const char* sql_insert_identify = "INSERT INTO identify_results (vnum, skill_level, output_text) VALUES (?, ?, ?)";
	if (sqlite3_prepare_v2(db_, sql_insert_identify, -1, &stmt_insert_identify_, nullptr) != SQLITE_OK)
	{
		last_error_ = std::string("Failed to prepare insert_identify: ") + sqlite3_errmsg(db_);
		return false;
	}

	return true;
}

void DatabaseWriter::FinalizeStatements()
{
	// Финализировать все подготовленные запросы
	if (stmt_insert_item_) sqlite3_finalize(stmt_insert_item_);
	if (stmt_insert_alias_) sqlite3_finalize(stmt_insert_alias_);
	if (stmt_insert_extra_flag_) sqlite3_finalize(stmt_insert_extra_flag_);
	if (stmt_insert_wear_flag_) sqlite3_finalize(stmt_insert_wear_flag_);
	if (stmt_insert_anti_flag_) sqlite3_finalize(stmt_insert_anti_flag_);
	if (stmt_insert_no_flag_) sqlite3_finalize(stmt_insert_no_flag_);
	if (stmt_insert_affect_flag_) sqlite3_finalize(stmt_insert_affect_flag_);
	if (stmt_insert_affect_) sqlite3_finalize(stmt_insert_affect_);
	if (stmt_insert_skill_) sqlite3_finalize(stmt_insert_skill_);
	if (stmt_insert_identify_) sqlite3_finalize(stmt_insert_identify_);
	if (stmt_insert_weapon_) sqlite3_finalize(stmt_insert_weapon_);
	if (stmt_insert_armor_) sqlite3_finalize(stmt_insert_armor_);
	if (stmt_insert_potion_) sqlite3_finalize(stmt_insert_potion_);
	if (stmt_insert_container_) sqlite3_finalize(stmt_insert_container_);
	if (stmt_insert_book_) sqlite3_finalize(stmt_insert_book_);
	if (stmt_insert_wand_staff_) sqlite3_finalize(stmt_insert_wand_staff_);
	if (stmt_insert_light_) sqlite3_finalize(stmt_insert_light_);
	if (stmt_insert_ingredient_) sqlite3_finalize(stmt_insert_ingredient_);

	stmt_insert_item_ = nullptr;
	stmt_insert_alias_ = nullptr;
	stmt_insert_extra_flag_ = nullptr;
	stmt_insert_wear_flag_ = nullptr;
	stmt_insert_anti_flag_ = nullptr;
	stmt_insert_no_flag_ = nullptr;
	stmt_insert_affect_flag_ = nullptr;
	stmt_insert_affect_ = nullptr;
	stmt_insert_skill_ = nullptr;
	stmt_insert_identify_ = nullptr;
	stmt_insert_weapon_ = nullptr;
	stmt_insert_armor_ = nullptr;
	stmt_insert_potion_ = nullptr;
	stmt_insert_container_ = nullptr;
	stmt_insert_book_ = nullptr;
	stmt_insert_wand_staff_ = nullptr;
	stmt_insert_light_ = nullptr;
	stmt_insert_ingredient_ = nullptr;
}

bool DatabaseWriter::InsertItem(int vnum, const std::string& short_desc, const std::string& type,
								const std::string& material, int weight, int cost, int rent_off, int rent_on,
								int max_dur, int cur_dur, int level, const std::string& sex, int min_remorts,
								const std::string& name_nom, const std::string& name_gen,
								const std::string& name_dat, const std::string& name_acc,
								const std::string& name_ins, const std::string& name_pre,
								const std::string& description, const std::string& action_desc,
								const std::string& source_file)
{
	if (!stmt_insert_item_)
	{
		if (!PrepareStatements())
		{
			return false;
		}
	}

	// Bind параметры
	sqlite3_reset(stmt_insert_item_);
	sqlite3_bind_int(stmt_insert_item_, 1, vnum);
	sqlite3_bind_text(stmt_insert_item_, 2, short_desc.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt_insert_item_, 3, name_nom.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt_insert_item_, 4, name_gen.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt_insert_item_, 5, name_dat.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt_insert_item_, 6, name_acc.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt_insert_item_, 7, name_ins.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt_insert_item_, 8, name_pre.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt_insert_item_, 9, description.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt_insert_item_, 10, action_desc.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt_insert_item_, 11, type.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt_insert_item_, 12, material.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt_insert_item_, 13, weight);
	sqlite3_bind_int(stmt_insert_item_, 14, cost);
	sqlite3_bind_int(stmt_insert_item_, 15, rent_off);
	sqlite3_bind_int(stmt_insert_item_, 16, rent_on);
	sqlite3_bind_int(stmt_insert_item_, 17, max_dur);
	sqlite3_bind_int(stmt_insert_item_, 18, cur_dur);
	sqlite3_bind_int(stmt_insert_item_, 19, level);
	sqlite3_bind_text(stmt_insert_item_, 20, sex.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt_insert_item_, 21, min_remorts);
	sqlite3_bind_text(stmt_insert_item_, 22, source_file.c_str(), -1, SQLITE_TRANSIENT);

	// Выполнить
	int rc = sqlite3_step(stmt_insert_item_);
	if (rc != SQLITE_DONE)
	{
		last_error_ = std::string("Failed to insert item: ") + sqlite3_errmsg(db_);
		return false;
	}

	return true;
}

bool DatabaseWriter::InsertAlias(int vnum, const std::string& alias)
{
	if (!stmt_insert_alias_)
	{
		if (!PrepareStatements())
		{
			return false;
		}
	}

	sqlite3_reset(stmt_insert_alias_);
	sqlite3_bind_int(stmt_insert_alias_, 1, vnum);
	sqlite3_bind_text(stmt_insert_alias_, 2, alias.c_str(), -1, SQLITE_TRANSIENT);

	int rc = sqlite3_step(stmt_insert_alias_);
	if (rc != SQLITE_DONE)
	{
		last_error_ = std::string("Failed to insert alias: ") + sqlite3_errmsg(db_);
		return false;
	}

	return true;
}

bool DatabaseWriter::InsertExtraFlag(int vnum, const std::string& flag)
{
	if (!stmt_insert_extra_flag_)
	{
		return false;
	}

	sqlite3_reset(stmt_insert_extra_flag_);
	sqlite3_bind_int(stmt_insert_extra_flag_, 1, vnum);
	sqlite3_bind_text(stmt_insert_extra_flag_, 2, flag.c_str(), -1, SQLITE_TRANSIENT);

	if (sqlite3_step(stmt_insert_extra_flag_) != SQLITE_DONE)
	{
		last_error_ = sqlite3_errmsg(db_);
		return false;
	}

	return true;
}

bool DatabaseWriter::InsertWearFlag(int vnum, const std::string& flag)
{
	if (!stmt_insert_wear_flag_)
	{
		return false;
	}

	sqlite3_reset(stmt_insert_wear_flag_);
	sqlite3_bind_int(stmt_insert_wear_flag_, 1, vnum);
	sqlite3_bind_text(stmt_insert_wear_flag_, 2, flag.c_str(), -1, SQLITE_TRANSIENT);

	if (sqlite3_step(stmt_insert_wear_flag_) != SQLITE_DONE)
	{
		last_error_ = sqlite3_errmsg(db_);
		return false;
	}

	return true;
}

bool DatabaseWriter::InsertAntiFlag(int vnum, const std::string& flag)
{
	if (!stmt_insert_anti_flag_)
	{
		return false;
	}

	sqlite3_reset(stmt_insert_anti_flag_);
	sqlite3_bind_int(stmt_insert_anti_flag_, 1, vnum);
	sqlite3_bind_text(stmt_insert_anti_flag_, 2, flag.c_str(), -1, SQLITE_TRANSIENT);

	if (sqlite3_step(stmt_insert_anti_flag_) != SQLITE_DONE)
	{
		last_error_ = sqlite3_errmsg(db_);
		return false;
	}

	return true;
}

bool DatabaseWriter::InsertNoFlag(int vnum, const std::string& flag)
{
	if (!stmt_insert_no_flag_)
	{
		return false;
	}

	sqlite3_reset(stmt_insert_no_flag_);
	sqlite3_bind_int(stmt_insert_no_flag_, 1, vnum);
	sqlite3_bind_text(stmt_insert_no_flag_, 2, flag.c_str(), -1, SQLITE_TRANSIENT);

	if (sqlite3_step(stmt_insert_no_flag_) != SQLITE_DONE)
	{
		last_error_ = sqlite3_errmsg(db_);
		return false;
	}

	return true;
}

bool DatabaseWriter::InsertAffectFlag(int vnum, const std::string& flag)
{
	if (!stmt_insert_affect_flag_)
	{
		return false;
	}

	sqlite3_reset(stmt_insert_affect_flag_);
	sqlite3_bind_int(stmt_insert_affect_flag_, 1, vnum);
	sqlite3_bind_text(stmt_insert_affect_flag_, 2, flag.c_str(), -1, SQLITE_TRANSIENT);

	if (sqlite3_step(stmt_insert_affect_flag_) != SQLITE_DONE)
	{
		last_error_ = sqlite3_errmsg(db_);
		return false;
	}

	return true;
}

bool DatabaseWriter::InsertAffect(int vnum, const std::string& apply_type, int modifier)
{
	if (!stmt_insert_affect_)
	{
		return false;
	}

	sqlite3_reset(stmt_insert_affect_);
	sqlite3_bind_int(stmt_insert_affect_, 1, vnum);
	sqlite3_bind_text(stmt_insert_affect_, 2, apply_type.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt_insert_affect_, 3, modifier);

	if (sqlite3_step(stmt_insert_affect_) != SQLITE_DONE)
	{
		last_error_ = sqlite3_errmsg(db_);
		return false;
	}

	return true;
}

bool DatabaseWriter::InsertSkill(int vnum, const std::string& skill_name, int modifier)
{
	if (!stmt_insert_skill_)
	{
		return false;
	}

	sqlite3_reset(stmt_insert_skill_);
	sqlite3_bind_int(stmt_insert_skill_, 1, vnum);
	sqlite3_bind_text(stmt_insert_skill_, 2, skill_name.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt_insert_skill_, 3, modifier);

	if (sqlite3_step(stmt_insert_skill_) != SQLITE_DONE)
	{
		last_error_ = sqlite3_errmsg(db_);
		return false;
	}

	return true;
}

bool DatabaseWriter::InsertIdentifyResult(int vnum, int skill_level, const std::string& output_text)
{
	if (!stmt_insert_identify_)
	{
		return false;
	}

	sqlite3_reset(stmt_insert_identify_);
	sqlite3_bind_int(stmt_insert_identify_, 1, vnum);
	sqlite3_bind_int(stmt_insert_identify_, 2, skill_level);
	sqlite3_bind_text(stmt_insert_identify_, 3, output_text.c_str(), -1, SQLITE_TRANSIENT);

	if (sqlite3_step(stmt_insert_identify_) != SQLITE_DONE)
	{
		last_error_ = sqlite3_errmsg(db_);
		return false;
	}

	return true;
}

bool DatabaseWriter::InsertWeaponValues(int vnum, int dice_num, int dice_size, double avg_damage)
{
	if (!stmt_insert_weapon_)
	{
		return false;
	}

	sqlite3_reset(stmt_insert_weapon_);
	sqlite3_bind_int(stmt_insert_weapon_, 1, vnum);
	sqlite3_bind_int(stmt_insert_weapon_, 2, dice_num);
	sqlite3_bind_int(stmt_insert_weapon_, 3, dice_size);
	sqlite3_bind_double(stmt_insert_weapon_, 4, avg_damage);

	if (sqlite3_step(stmt_insert_weapon_) != SQLITE_DONE)
	{
		last_error_ = sqlite3_errmsg(db_);
		return false;
	}

	return true;
}

bool DatabaseWriter::InsertArmorValues(int vnum, int ac, int armor)
{
	if (!stmt_insert_armor_)
	{
		return false;
	}

	sqlite3_reset(stmt_insert_armor_);
	sqlite3_bind_int(stmt_insert_armor_, 1, vnum);
	sqlite3_bind_int(stmt_insert_armor_, 2, ac);
	sqlite3_bind_int(stmt_insert_armor_, 3, armor);

	if (sqlite3_step(stmt_insert_armor_) != SQLITE_DONE)
	{
		last_error_ = sqlite3_errmsg(db_);
		return false;
	}

	return true;
}

bool DatabaseWriter::InsertPotionValue(int vnum, int spell_num, const std::string& spell_name, int spell_level)
{
	// TODO: Реализовать
	(void)vnum; (void)spell_num; (void)spell_name; (void)spell_level;
	return true;
}

bool DatabaseWriter::InsertContainerValues(int vnum, int capacity, bool is_corpse)
{
	// TODO: Реализовать
	(void)vnum; (void)capacity; (void)is_corpse;
	return true;
}

bool DatabaseWriter::InsertBookValues(int vnum, const std::string& spell_name, const std::string& skill_name)
{
	// TODO: Реализовать
	(void)vnum; (void)spell_name; (void)skill_name;
	return true;
}

bool DatabaseWriter::InsertWandStaffValues(int vnum, const std::string& spell_name, int spell_level,
										   int charges_max, int charges_remaining)
{
	// TODO: Реализовать
	(void)vnum; (void)spell_name; (void)spell_level; (void)charges_max; (void)charges_remaining;
	return true;
}

bool DatabaseWriter::InsertLightValues(int vnum, int hours)
{
	// TODO: Реализовать
	(void)vnum; (void)hours;
	return true;
}

bool DatabaseWriter::InsertIngredientValues(int vnum, int potion_proto)
{
	// TODO: Реализовать
	(void)vnum; (void)potion_proto;
	return true;
}

// vim: ts=4 sw=4 tw=0 noet syntax=cpp :

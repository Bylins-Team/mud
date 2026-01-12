#ifndef IDENTIFY_EXPORT_DB_WRITER_H
#define IDENTIFY_EXPORT_DB_WRITER_H

#include <string>
#include <sqlite3.h>

// Класс для записи данных в SQLite базу
class DatabaseWriter
{
public:
	DatabaseWriter();
	~DatabaseWriter();

	// Открыть базу данных
	bool Open(const std::string& db_path);

	// Закрыть базу данных
	void Close();

	// Создать все таблицы
	bool CreateTables();

	// Создать все индексы (после вставки данных для производительности)
	bool CreateIndexes();

	// Создать все views
	bool CreateViews();

	// Управление транзакциями
	bool BeginTransaction();
	bool CommitTransaction();
	bool RollbackTransaction();

	// Удаление данных при замене предмета
	bool DeleteItemRelatedData(int vnum);

	// Вставка данных
	bool InsertItem(int vnum, const std::string& short_desc, const std::string& type,
					const std::string& material, int weight, int cost, int rent_off, int rent_on,
					int max_dur, int cur_dur, int level, const std::string& sex, int min_remorts,
					const std::string& name_nom, const std::string& name_gen,
					const std::string& name_dat, const std::string& name_acc,
					const std::string& name_ins, const std::string& name_pre,
					const std::string& description, const std::string& action_desc,
					const std::string& source_file);

	bool InsertAlias(int vnum, const std::string& alias);
	bool InsertExtraFlag(int vnum, const std::string& flag);
	bool InsertWearFlag(int vnum, const std::string& flag);
	bool InsertAntiFlag(int vnum, const std::string& flag);
	bool InsertNoFlag(int vnum, const std::string& flag);
	bool InsertAffectFlag(int vnum, const std::string& flag);
	bool InsertAffect(int vnum, const std::string& apply_type, int modifier);
	bool InsertSkill(int vnum, const std::string& skill_name, int modifier);
	bool InsertIdentifyResult(int vnum, int skill_level, const std::string& output_text);

	// Вставка значений по типам предметов
	bool InsertWeaponValues(int vnum, int dice_num, int dice_size, double avg_damage);
	bool InsertArmorValues(int vnum, int ac, int armor);
	bool InsertPotionValue(int vnum, int spell_num, const std::string& spell_name, int spell_level);
	bool InsertContainerValues(int vnum, int capacity, bool is_corpse);
	bool InsertBookValues(int vnum, const std::string& spell_name, const std::string& skill_name);
	bool InsertWandStaffValues(int vnum, const std::string& spell_name, int spell_level,
							   int charges_max, int charges_remaining);
	bool InsertLightValues(int vnum, int hours);
	bool InsertIngredientValues(int vnum, int potion_proto);

	// Получить последнее сообщение об ошибке
	std::string GetLastError() const;

private:
	sqlite3* db_;
	std::string last_error_;

	// Подготовленные запросы для производительности
	sqlite3_stmt* stmt_insert_item_;
	sqlite3_stmt* stmt_insert_alias_;
	sqlite3_stmt* stmt_insert_extra_flag_;
	sqlite3_stmt* stmt_insert_wear_flag_;
	sqlite3_stmt* stmt_insert_anti_flag_;
	sqlite3_stmt* stmt_insert_no_flag_;
	sqlite3_stmt* stmt_insert_affect_flag_;
	sqlite3_stmt* stmt_insert_affect_;
	sqlite3_stmt* stmt_insert_skill_;
	sqlite3_stmt* stmt_insert_identify_;
	sqlite3_stmt* stmt_insert_weapon_;
	sqlite3_stmt* stmt_insert_armor_;
	sqlite3_stmt* stmt_insert_potion_;
	sqlite3_stmt* stmt_insert_container_;
	sqlite3_stmt* stmt_insert_book_;
	sqlite3_stmt* stmt_insert_wand_staff_;
	sqlite3_stmt* stmt_insert_light_;
	sqlite3_stmt* stmt_insert_ingredient_;

	// Инициализация подготовленных запросов
	bool PrepareStatements();

	// Очистка подготовленных запросов
	void FinalizeStatements();

	// Выполнить SQL запрос
	bool ExecuteSQL(const char* sql);
};

#endif // IDENTIFY_EXPORT_DB_WRITER_H

// vim: ts=4 sw=4 tw=0 noet syntax=cpp :

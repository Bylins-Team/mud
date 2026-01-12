#ifndef IDENTIFY_EXPORT_EXPORTER_H
#define IDENTIFY_EXPORT_EXPORTER_H

#include <string>
#include <vector>
#include <map>
#include "db_writer.h"

// Структура для хранения аффектов
struct SimpleAffect
{
	int apply_type;  // EApply
	int modifier;
};

// Структура для хранения скиллов
struct SimpleSkill
{
	int skill_id;   // ESkill
	int modifier;
};

// Структура для хранения экстра-описаний
struct SimpleExtraDesc
{
	std::string keyword;
	std::string description;
};

// Простая структура для хранения данных объекта
// (вместо использования CObjectPrototype из MUD)
struct SimpleObject
{
	int vnum;

	// String data
	std::string aliases;
	std::string short_desc;
	std::string name_nominative;    // Именительный
	std::string name_genitive;      // Родительный
	std::string name_dative;        // Дательный
	std::string name_accusative;    // Винительный
	std::string name_instrumental;  // Творительный
	std::string name_prepositional; // Предложный
	std::string description;        // Long description
	std::string action_description;
	std::string source_file;        // .obj file this object came from

	// First numeric line (spec_param, durability, material)
	int spec_param;
	int max_durability;
	int current_durability;
	int material;  // EObjMaterial

	// Second numeric line (sex, timer, spell, level)
	int sex;    // EGender
	int timer;
	int spell;
	int level;

	// Third numeric line (flags as raw strings for parsing)
	std::string affect_flags_str;
	std::string anti_flags_str;
	std::string no_flags_str;

	// Fourth numeric line
	int type;  // EObjType
	std::string extra_flags_str;
	std::string wear_flags_str;

	// Fifth numeric line (type-specific values)
	int value0, value1, value2, value3;

	// Sixth numeric line
	int weight;
	int cost;
	int rent_off;
	int rent_on;

	// Optional sections
	std::vector<SimpleExtraDesc> extra_descriptions;
	std::vector<SimpleAffect> affects;
	std::vector<SimpleSkill> skills;
	int max_in_world;
	int minimum_remorts;

	// Constructor with defaults
	SimpleObject() : vnum(0), spec_param(0), max_durability(0), current_durability(0),
		material(0), sex(0), timer(0), spell(0), level(1), type(0),
		value0(0), value1(0), value2(0), value3(0), weight(0), cost(0),
		rent_off(0), rent_on(0), max_in_world(0), minimum_remorts(0) {}
};

// Основной класс экспортера данных
class IdentifyExporter
{
public:
	IdentifyExporter();
	~IdentifyExporter();

	// Запустить экспорт
	bool Run(const std::string& lib_path, const std::string& output_db);

	// Экспорт identify results в Markdown
	bool ExportToMarkdown(const std::string& db_path, const std::string& output_md,
						  const std::vector<int>& skill_levels);

private:
	DatabaseWriter db_;

	// Загрузка объектов из файлов (простой парсер)
	bool LoadObjects(const std::string& lib_path);

	// Обработка одного объекта
	bool ProcessObject(const SimpleObject& obj, const std::string& source_file);

	// Парсинг одного .obj файла
	bool ParseObjFile(const std::string& file_path, std::vector<SimpleObject>& objects);

	// Статистика
	int total_objects_;
	int processed_objects_;
	int failed_objects_;
	int replaced_objects_;

	// Хранилище загруженных объектов
	std::vector<SimpleObject> objects_;

	// Отслеживание дубликатов
	std::map<int, std::string> vnum_to_source_;
};

#endif // IDENTIFY_EXPORT_EXPORTER_H

// vim: ts=4 sw=4 tw=0 noet syntax=cpp :

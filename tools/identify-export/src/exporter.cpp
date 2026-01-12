#include "exporter.h"
#include "encoding.h"
#include "identify_simulation.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <sqlite3.h>

// Include MUD constants
#include "gameplay/core/constants.h"
#include "gameplay/affects/affect_contants.h"

namespace fs = std::filesystem;

// Declare extern constants from MUD
extern const char *item_types[];
extern const char *material_name[];
extern const char *genders[];
extern const char *extra_bits[];
extern const char *wear_bits[];
extern const char *weapon_affects[];
extern const char *anti_bits[];
extern const char *no_bits[];
extern const char *apply_types[];

// Helper function to safely get string from array (handles out-of-bounds)
static std::string GetConstantName(const char *array[], int index, const char *default_val = "неизвестно")
{
	if (index < 0 || !array[index] || array[index][0] == '\n')
	{
		return default_val;
	}
	return array[index];
}

// Parse ASCII flags into bit positions
// "abc" -> bits 0, 1, 2 set
// "a0b1" -> bit 0 in block 0, bit 1 in block 1
static std::vector<int> ParseASCIIFlags(const std::string& flag_str)
{
	std::vector<int> bit_positions;

	for (size_t i = 0; i < flag_str.length(); ++i)
	{
		char c = flag_str[i];

		// Skip digits (block indicators)
		if (isdigit(c))
		{
			continue;
		}

		int bit_pos = -1;
		if (islower(c))
		{
			bit_pos = c - 'a';  // a=0, b=1, ..., z=25
		}
		else if (isupper(c))
		{
			bit_pos = 26 + (c - 'A');  // A=26, B=27, ..., Z=51
		}

		if (bit_pos >= 0)
		{
			bit_positions.push_back(bit_pos);
		}
	}

	return bit_positions;
}

// Convert bit positions to flag names using constant array
static std::vector<std::string> BitPositionsToNames(const std::vector<int>& bits, const char *names_array[])
{
	std::vector<std::string> result;

	for (int bit : bits)
	{
		std::string name = GetConstantName(names_array, bit, "");
		if (!name.empty() && name != "неизвестно")
		{
			result.push_back(ConvertKOI8RtoUTF8(name));
		}
	}

	return result;
}

IdentifyExporter::IdentifyExporter()
	: total_objects_(0)
	, processed_objects_(0)
	, failed_objects_(0)
	, replaced_objects_(0)
{
}

IdentifyExporter::~IdentifyExporter()
{
}

bool IdentifyExporter::Run(const std::string& lib_path, const std::string& output_db)
{
	std::cout << "Step 1: Opening database...\n";

	// Открыть базу данных
	if (!db_.Open(output_db))
	{
		std::cerr << "Failed to open database: " << db_.GetLastError() << "\n";
		return false;
	}

	std::cout << "Step 2: Creating tables and views...\n";

	// Создать таблицы
	if (!db_.CreateTables())
	{
		std::cerr << "Failed to create tables: " << db_.GetLastError() << "\n";
		return false;
	}

	std::cout << "Step 3: Loading objects from " << lib_path << "...\n";

	// Загрузить объекты
	if (!LoadObjects(lib_path))
	{
		std::cerr << "Failed to load objects\n";
		return false;
	}

	std::cout << "Loaded " << total_objects_ << " objects\n";

	std::cout << "Step 4: Processing objects...\n";

	// Начать транзакцию
	if (!db_.BeginTransaction())
	{
		std::cerr << "Failed to begin transaction: " << db_.GetLastError() << "\n";
		return false;
	}

	// Обработать все объекты
	for (const auto& obj : objects_)
	{
		if (!ProcessObject(obj, obj.source_file))
		{
			std::cerr << "Warning: Failed to process object vnum " << obj.vnum << "\n";
			failed_objects_++;
		}
	}

	// Завершить транзакцию
	if (!db_.CommitTransaction())
	{
		std::cerr << "Failed to commit transaction: " << db_.GetLastError() << "\n";
		return false;
	}

	std::cout << "Step 5: Creating indexes...\n";

	// Создать индексы
	if (!db_.CreateIndexes())
	{
		std::cerr << "Failed to create indexes: " << db_.GetLastError() << "\n";
		return false;
	}

	std::cout << "Step 6: Creating views...\n";

	// Создать views
	if (!db_.CreateViews())
	{
		std::cerr << "Failed to create views: " << db_.GetLastError() << "\n";
		return false;
	}

	// Вывести статистику
	std::cout << "\n=== Export Statistics ===\n";
	std::cout << "Total objects: " << total_objects_ << "\n";
	std::cout << "Processed: " << processed_objects_ << "\n";
	std::cout << "Failed: " << failed_objects_ << "\n";
	if (replaced_objects_ > 0)
	{
		std::cout << "Replaced (duplicates): " << replaced_objects_ << "\n";
	}

	return true;
}

bool IdentifyExporter::LoadObjects(const std::string& lib_path)
{
	// Автоопределение директории с .obj файлами
	std::string obj_dir;

	// Вариант 1: path/world/obj (полный путь к lib)
	std::string path_with_world = lib_path + "/world/obj";
	if (fs::exists(path_with_world) && fs::is_directory(path_with_world))
	{
		obj_dir = path_with_world;
	}
	// Вариант 2: path/obj (путь к world)
	else if (fs::exists(lib_path + "/obj") && fs::is_directory(lib_path + "/obj"))
	{
		obj_dir = lib_path + "/obj";
	}
	// Вариант 3: path (прямой путь к obj)
	else if (fs::exists(lib_path) && fs::is_directory(lib_path))
	{
		obj_dir = lib_path;
	}
	else
	{
		std::cerr << "Cannot find .obj directory in: " << lib_path << "\n";
		std::cerr << "Tried:\n";
		std::cerr << "  - " << path_with_world << "\n";
		std::cerr << "  - " << lib_path + "/obj" << "\n";
		std::cerr << "  - " << lib_path << "\n";
		return false;
	}

	// Перебрать все .obj файлы
	for (const auto& entry : fs::directory_iterator(obj_dir))
	{
		if (entry.is_regular_file() && entry.path().extension() == ".obj")
		{
			std::cout << "  Parsing " << entry.path().filename() << "...\n";

			std::vector<SimpleObject> file_objects;
			if (ParseObjFile(entry.path().string(), file_objects))
			{
				objects_.insert(objects_.end(), file_objects.begin(), file_objects.end());
				total_objects_ += file_objects.size();
			}
			else
			{
				std::cerr << "  Warning: Failed to parse " << entry.path() << "\n";
			}
		}
	}

	return total_objects_ > 0;
}

// Helper function to read a '~'-terminated string
static std::string ReadTildeString(std::ifstream& file)
{
	std::string result;
	std::string line;
	bool done = false;

	while (!done && std::getline(file, line))
	{
		// Remove \r if present
		if (!line.empty() && line.back() == '\r')
		{
			line.pop_back();
		}

		// Check for terminating ~
		if (!line.empty() && line.back() == '~')
		{
			line.pop_back();  // Remove the ~
			result += line;
			done = true;
		}
		else
		{
			result += line + "\n";  // Keep newlines in multi-line strings
		}
	}

	return result;
}

bool IdentifyExporter::ParseObjFile(const std::string& file_path, std::vector<SimpleObject>& objects)
{
	std::ifstream file(file_path);
	if (!file.is_open())
	{
		return false;
	}

	// Извлечь имя файла из пути
	fs::path path(file_path);
	std::string filename = path.filename().string();

	std::string line;

	while (std::getline(file, line))
	{
		// Remove \r if present
		if (!line.empty() && line.back() == '\r')
		{
			line.pop_back();
		}

		// Skip ADAMANT marker
		if (line == "#ADAMANT")
		{
			continue;
		}

		// End of file
		if (line == "$")
		{
			break;
		}

		// Start of new object
		if (!line.empty() && line[0] == '#')
		{
			SimpleObject obj;

			try
			{
				obj.vnum = std::stoi(line.substr(1));

				// Read aliases
				obj.aliases = ReadTildeString(file);

				// Read short description
				obj.short_desc = ReadTildeString(file);
				obj.name_nominative = obj.short_desc;  // Default

				// Read 6 PName cases (genitive through prepositional)
				obj.name_genitive = ReadTildeString(file);
				obj.name_dative = ReadTildeString(file);
				obj.name_accusative = ReadTildeString(file);
				obj.name_instrumental = ReadTildeString(file);
				obj.name_prepositional = ReadTildeString(file);

				// Read long description
				obj.description = ReadTildeString(file);

				// Read action description
				obj.action_description = ReadTildeString(file);

				// Read first numeric line: spec_param max_dur cur_dur material
				if (!std::getline(file, line))
				{
					std::cerr << "Error: Expected first numeric line for vnum " << obj.vnum << "\n";
					continue;
				}
				if (!line.empty() && line.back() == '\r') line.pop_back();

				char spec_flags[256];
				if (sscanf(line.c_str(), " %s %d %d %d", spec_flags,
						   &obj.max_durability, &obj.current_durability, &obj.material) != 4)
				{
					std::cerr << "Error: Failed to parse first numeric line for vnum " << obj.vnum << "\n";
					continue;
				}
				// Store spec_param flags for later conversion
				// TODO: parse flags

				// Read second numeric line: sex timer spell level
				if (!std::getline(file, line))
				{
					std::cerr << "Error: Expected second numeric line for vnum " << obj.vnum << "\n";
					continue;
				}
				if (!line.empty() && line.back() == '\r') line.pop_back();

				if (sscanf(line.c_str(), " %d %d %d %d",
						   &obj.sex, &obj.timer, &obj.spell, &obj.level) != 4)
				{
					std::cerr << "Error: Failed to parse second numeric line for vnum " << obj.vnum << "\n";
					continue;
				}

				// Read third numeric line: affect_flags anti_flags no_flags
				if (!std::getline(file, line))
				{
					std::cerr << "Error: Expected third numeric line for vnum " << obj.vnum << "\n";
					continue;
				}
				if (!line.empty() && line.back() == '\r') line.pop_back();

				char affect_flags[256], anti_flags[256], no_flags[256];
				if (sscanf(line.c_str(), " %s %s %s", affect_flags, anti_flags, no_flags) != 3)
				{
					std::cerr << "Error: Failed to parse third numeric line for vnum " << obj.vnum << "\n";
					continue;
				}
				obj.affect_flags_str = affect_flags;
				obj.anti_flags_str = anti_flags;
				obj.no_flags_str = no_flags;

				// Read fourth numeric line: type extra_flags wear_flags
				if (!std::getline(file, line))
				{
					std::cerr << "Error: Expected fourth numeric line for vnum " << obj.vnum << "\n";
					continue;
				}
				if (!line.empty() && line.back() == '\r') line.pop_back();

				char extra_flags[256], wear_flags[256];
				if (sscanf(line.c_str(), " %d %s %s", &obj.type, extra_flags, wear_flags) != 3)
				{
					std::cerr << "Error: Failed to parse fourth numeric line for vnum " << obj.vnum << "\n";
					continue;
				}
				obj.extra_flags_str = extra_flags;
				obj.wear_flags_str = wear_flags;

				// Read fifth numeric line: value0 value1 value2 value3
				if (!std::getline(file, line))
				{
					std::cerr << "Error: Expected fifth numeric line for vnum " << obj.vnum << "\n";
					continue;
				}
				if (!line.empty() && line.back() == '\r') line.pop_back();

				char val0_str[256];
				if (sscanf(line.c_str(), " %s %d %d %d", val0_str,
						   &obj.value1, &obj.value2, &obj.value3) != 4)
				{
					std::cerr << "Error: Failed to parse fifth numeric line for vnum " << obj.vnum << "\n";
					continue;
				}
				// Try to parse value0 as integer, if fails it might be flags
				obj.value0 = std::atoi(val0_str);

				// Read sixth numeric line: weight cost rent_off rent_on
				if (!std::getline(file, line))
				{
					std::cerr << "Error: Expected sixth numeric line for vnum " << obj.vnum << "\n";
					continue;
				}
				if (!line.empty() && line.back() == '\r') line.pop_back();

				if (sscanf(line.c_str(), " %d %d %d %d",
						   &obj.weight, &obj.cost, &obj.rent_off, &obj.rent_on) != 4)
				{
					std::cerr << "Error: Failed to parse sixth numeric line for vnum " << obj.vnum << "\n";
					continue;
				}

				// Read optional sections (E, A, T, M, R, S, V, $, #)
				while (std::getline(file, line))
				{
					// Remove \r if present
					if (!line.empty() && line.back() == '\r')
					{
						line.pop_back();
					}

					if (line.empty())
					{
						continue;
					}

					char section_type = line[0];

					if (section_type == 'E')
					{
						// Extra description
						SimpleExtraDesc extra;
						extra.keyword = ReadTildeString(file);
						extra.description = ReadTildeString(file);
						obj.extra_descriptions.push_back(extra);
					}
					else if (section_type == 'A')
					{
						// Affect
						if (!std::getline(file, line))
						{
							break;
						}
						if (!line.empty() && line.back() == '\r') line.pop_back();

						SimpleAffect affect;
						if (sscanf(line.c_str(), " %d %d", &affect.apply_type, &affect.modifier) == 2)
						{
							obj.affects.push_back(affect);
						}
					}
					else if (section_type == 'T')
					{
						// Trigger - skip for now
						// TODO: implement trigger parsing if needed
					}
					else if (section_type == 'M')
					{
						// Max in world
						obj.max_in_world = std::atoi(line.c_str() + 1);
					}
					else if (section_type == 'R')
					{
						// Minimum remorts
						obj.minimum_remorts = std::atoi(line.c_str() + 1);
					}
					else if (section_type == 'S')
					{
						// Skill
						if (!std::getline(file, line))
						{
							break;
						}
						if (!line.empty() && line.back() == '\r') line.pop_back();

						SimpleSkill skill;
						if (sscanf(line.c_str(), " %d %d", &skill.skill_id, &skill.modifier) == 2)
						{
							obj.skills.push_back(skill);
						}
					}
					else if (section_type == 'V')
					{
						// Values from zone - skip for now
						// TODO: implement if needed
					}
					else if (section_type == '$' || section_type == '#')
					{
						// End of object or start of next object
						// Put the line back for next iteration
						file.seekg(-static_cast<int>(line.length()) - 1, std::ios::cur);
						break;
					}
					else
					{
						std::cerr << "Warning: Unknown section type '" << section_type
								  << "' in vnum " << obj.vnum << "\n";
					}
				}

				// Set source file name
				obj.source_file = filename;

				// Add completed object
				objects.push_back(obj);
			}
			catch (const std::exception& e)
			{
				std::cerr << "Error parsing object: " << e.what() << "\n";
				continue;
			}
		}
	}

	return !objects.empty();
}

bool IdentifyExporter::ProcessObject(const SimpleObject& obj, const std::string& source_file)
{
	// Проверить на дубликат vnum
	auto it = vnum_to_source_.find(obj.vnum);
	if (it != vnum_to_source_.end())
	{
		// Дубликат найден - заменяем предыдущую версию
		std::cout << "Warning: Duplicate vnum " << obj.vnum << " - replacing version from '"
				  << it->second << "' with version from '" << source_file << "'\n";

		// Удалить все связанные данные перед заменой
		db_.DeleteItemRelatedData(obj.vnum);
		replaced_objects_++;
	}

	// Запомнить source_file для этого vnum
	vnum_to_source_[obj.vnum] = source_file;

	// Конвертировать все строковые поля в UTF-8
	std::string short_desc_utf8 = ConvertKOI8RtoUTF8(obj.short_desc);
	std::string aliases_utf8 = ConvertKOI8RtoUTF8(obj.aliases);
	std::string name_nom_utf8 = ConvertKOI8RtoUTF8(obj.name_nominative);
	std::string name_gen_utf8 = ConvertKOI8RtoUTF8(obj.name_genitive);
	std::string name_dat_utf8 = ConvertKOI8RtoUTF8(obj.name_dative);
	std::string name_acc_utf8 = ConvertKOI8RtoUTF8(obj.name_accusative);
	std::string name_ins_utf8 = ConvertKOI8RtoUTF8(obj.name_instrumental);
	std::string name_pre_utf8 = ConvertKOI8RtoUTF8(obj.name_prepositional);
	std::string description_utf8 = ConvertKOI8RtoUTF8(obj.description);
	std::string action_desc_utf8 = ConvertKOI8RtoUTF8(obj.action_description);

	// Преобразовать enum значения в строки используя константы MUD
	std::string type_name = ConvertKOI8RtoUTF8(GetConstantName(item_types, obj.type));
	std::string material_str = ConvertKOI8RtoUTF8(GetConstantName(material_name, obj.material));
	std::string sex_name = ConvertKOI8RtoUTF8(GetConstantName(genders, obj.sex));

	// Вставить базовые данные предмета (INSERT OR REPLACE)
	if (!db_.InsertItem(obj.vnum, short_desc_utf8, type_name, material_str,
						obj.weight, obj.cost, obj.rent_off, obj.rent_on,
						obj.max_durability, obj.current_durability, obj.level,
						sex_name, obj.minimum_remorts,
						name_nom_utf8, name_gen_utf8, name_dat_utf8,
						name_acc_utf8, name_ins_utf8, name_pre_utf8,
						description_utf8, action_desc_utf8, source_file))
	{
		std::cerr << "Error inserting item " << obj.vnum << ": " << db_.GetLastError() << "\n";
		return false;
	}

	// Вставить алиасы (разделить по пробелам)
	std::istringstream iss(aliases_utf8);
	std::string alias;
	while (iss >> alias)
	{
		if (!db_.InsertAlias(obj.vnum, alias))
		{
			// Не критично, продолжаем
		}
	}

	// Распарсить и вставить флаги
	// Extra flags
	auto extra_bits_pos = ParseASCIIFlags(obj.extra_flags_str);
	auto extra_names = BitPositionsToNames(extra_bits_pos, extra_bits);
	for (const auto& flag_name : extra_names)
	{
		db_.InsertExtraFlag(obj.vnum, flag_name);
	}

	// Wear flags
	auto wear_bits_pos = ParseASCIIFlags(obj.wear_flags_str);
	auto wear_names = BitPositionsToNames(wear_bits_pos, wear_bits);
	for (const auto& flag_name : wear_names)
	{
		db_.InsertWearFlag(obj.vnum, flag_name);
	}

	// Anti flags
	auto anti_bits_pos = ParseASCIIFlags(obj.anti_flags_str);
	auto anti_names = BitPositionsToNames(anti_bits_pos, anti_bits);
	for (const auto& flag_name : anti_names)
	{
		db_.InsertAntiFlag(obj.vnum, flag_name);
	}

	// No flags
	auto no_bits_pos = ParseASCIIFlags(obj.no_flags_str);
	auto no_names = BitPositionsToNames(no_bits_pos, no_bits);
	for (const auto& flag_name : no_names)
	{
		db_.InsertNoFlag(obj.vnum, flag_name);
	}

	// Affect flags (weapon affects)
	auto affect_bits_pos = ParseASCIIFlags(obj.affect_flags_str);
	auto affect_names = BitPositionsToNames(affect_bits_pos, weapon_affects);
	for (const auto& flag_name : affect_names)
	{
		db_.InsertAffectFlag(obj.vnum, flag_name);
	}

	// Вставить аффекты
	for (const auto& affect : obj.affects)
	{
		std::string apply_type_name = ConvertKOI8RtoUTF8(GetConstantName(apply_types, affect.apply_type));
		if (!apply_type_name.empty() && apply_type_name != "неизвестно")
		{
			db_.InsertAffect(obj.vnum, apply_type_name, affect.modifier);
		}
	}

	// Вставить скиллы
	for (const auto& skill : obj.skills)
	{
		// TODO: Найти массив имён скиллов в MUD
		// Пока пропускаем - нужно найти правильный массив
		std::string skill_name = "";
		if (!skill_name.empty())
		{
			db_.InsertSkill(obj.vnum, skill_name, skill.modifier);
		}
	}

	// Вставить type-specific значения в зависимости от типа
	// Проверить тип и вставить в соответствующую таблицу
	if (type_name == "ОРУЖИЕ")
	{
		// Для оружия: value1 = число костей, value2 = размер кости
		if (obj.value1 > 0 && obj.value2 > 0)
		{
			double avg_damage = (obj.value1 * (obj.value2 + 1)) / 2.0;
			db_.InsertWeaponValues(obj.vnum, obj.value1, obj.value2, avg_damage);
		}
	}
	else if (type_name == "БРОНЯ" || type_name == "ЛЕГКИЙ ДОСПЕХ" ||
	         type_name == "СРЕДНИЙ ДОСПЕХ" || type_name == "ТЯЖЕЛЫЙ ДОСПЕХ")
	{
		// Для доспехов: value0 = AC, value1 = armor
		db_.InsertArmorValues(obj.vnum, obj.value0, obj.value1);
	}

	// Симуляция команды identify для разных уровней навыка
	const int skill_levels[] = {0, 20, 30, 40, 60, 75, 90, 100, 400};
	for (int skill_level : skill_levels)
	{
		std::string identify_text = SimulateIdentify(obj, skill_level);
		if (!identify_text.empty())
		{
			// Convert KOI8-R to UTF-8 before saving to database
			std::string identify_utf8 = ConvertKOI8RtoUTF8(identify_text);
			db_.InsertIdentifyResult(obj.vnum, skill_level, identify_utf8);
		}
	}

	processed_objects_++;
	return true;
}

bool IdentifyExporter::ExportToMarkdown(const std::string& db_path,
										const std::string& output_md,
										const std::vector<int>& skill_levels)
{
	// Открыть базу данных для чтения
	sqlite3* db = nullptr;
	if (sqlite3_open_v2(db_path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK)
	{
		std::cerr << "Error: Cannot open database " << db_path << "\n";
		return false;
	}

	// Открыть выходной Markdown файл
	std::ofstream out(output_md);
	if (!out.is_open())
	{
		std::cerr << "Error: Cannot create output file " << output_md << "\n";
		sqlite3_close(db);
		return false;
	}

	// Написать заголовок
	out << "# Результаты опознания предметов\n\n";
	out << "Сгенерировано из базы данных: `" << db_path << "`\n\n";

	// Уровни навыка для экспорта
	out << "Уровни навыка: ";
	for (size_t i = 0; i < skill_levels.size(); ++i)
	{
		out << skill_levels[i];
		if (i < skill_levels.size() - 1)
			out << ", ";
	}
	out << "\n\n";
	out << "---\n\n";

	// Получить все предметы
	const char* sql_items = "SELECT vnum, short_description, type FROM items ORDER BY vnum";
	sqlite3_stmt* stmt_items = nullptr;

	if (sqlite3_prepare_v2(db, sql_items, -1, &stmt_items, nullptr) != SQLITE_OK)
	{
		std::cerr << "Error preparing items query: " << sqlite3_errmsg(db) << "\n";
		sqlite3_close(db);
		return false;
	}

	int item_count = 0;

	// Для каждого предмета
	while (sqlite3_step(stmt_items) == SQLITE_ROW)
	{
		int vnum = sqlite3_column_int(stmt_items, 0);
		const char* short_desc = reinterpret_cast<const char*>(sqlite3_column_text(stmt_items, 1));
		const char* type = reinterpret_cast<const char*>(sqlite3_column_text(stmt_items, 2));

		// Написать заголовок предмета
		out << "## [" << vnum << "] " << short_desc << "\n\n";
		out << "**Тип:** " << type << "\n\n";

		// Получить identify results для этого vnum на заданных уровнях
		const char* sql_identify = "SELECT skill_level, output_text FROM identify_results WHERE vnum = ? AND skill_level IN (";

		// Построить IN clause
		std::ostringstream in_clause;
		for (size_t i = 0; i < skill_levels.size(); ++i)
		{
			in_clause << "?";
			if (i < skill_levels.size() - 1)
				in_clause << ",";
		}
		std::string sql_full = std::string(sql_identify) + in_clause.str() + ") ORDER BY skill_level";

		sqlite3_stmt* stmt_identify = nullptr;
		if (sqlite3_prepare_v2(db, sql_full.c_str(), -1, &stmt_identify, nullptr) == SQLITE_OK)
		{
			// Bind parameters
			sqlite3_bind_int(stmt_identify, 1, vnum);
			for (size_t i = 0; i < skill_levels.size(); ++i)
			{
				sqlite3_bind_int(stmt_identify, i + 2, skill_levels[i]);
			}

			// Для каждого уровня навыка
			while (sqlite3_step(stmt_identify) == SQLITE_ROW)
			{
				int skill_level = sqlite3_column_int(stmt_identify, 0);
				const char* output_text = reinterpret_cast<const char*>(sqlite3_column_text(stmt_identify, 1));

				// Написать результат опознания
				out << "### Уровень навыка: " << skill_level << "\n\n";
				out << "```\n";
				out << output_text;
				out << "\n```\n\n";
			}

			sqlite3_finalize(stmt_identify);
		}

		out << "---\n\n";
		item_count++;
	}

	sqlite3_finalize(stmt_items);
	sqlite3_close(db);
	out.close();

	std::cout << "\nMarkdown export completed successfully!\n";
	std::cout << "Exported " << item_count << " items to " << output_md << "\n";

	return true;
}

// vim: ts=4 sw=4 tw=0 noet syntax=cpp :

#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <sstream>
#include "exporter.h"

void PrintUsage(const char* program_name)
{
	std::cout << "Usage:\n";
	std::cout << "  1) Export from .obj files to SQLite:\n";
	std::cout << "     " << program_name << " --lib-path <path> --output <db_file>\n\n";
	std::cout << "  2) Export identify results to Markdown:\n";
	std::cout << "     " << program_name << " --input-db <db_file> --export-markdown <md_file> [--skill-levels <levels>]\n\n";
	std::cout << "  3) Combined mode (create database + export Markdown):\n";
	std::cout << "     " << program_name << " --lib-path <path> --output <db_file> --export-markdown <md_file> [--skill-levels <levels>]\n";
	std::cout << "\nOptions:\n";
	std::cout << "  --lib-path <path>         Path to lib, world, or obj directory\n";
	std::cout << "                            Accepts: ../lib OR ../lib/world OR ../lib/world/obj\n";
	std::cout << "  --output <db_file>        Output SQLite database file (e.g., items.db)\n";
	std::cout << "  --export-markdown <file>  Export identify results to Markdown file\n";
	std::cout << "  --input-db <db_file>      Input database for Markdown export (standalone mode)\n";
	std::cout << "  --skill-levels <levels>   Comma-separated skill levels (default: 100)\n";
	std::cout << "                            Examples: 100, 0,20,100, 400\n";
	std::cout << "\nExamples:\n";
	std::cout << "  # Create database from .obj files:\n";
	std::cout << "  " << program_name << " --lib-path ../../lib.template/world --output items.db\n\n";
	std::cout << "  # Create database + export Markdown in one command:\n";
	std::cout << "  " << program_name << " --lib-path ../../lib/world --output items.db --export-markdown items.md\n\n";
	std::cout << "  # Export to Markdown from existing database:\n";
	std::cout << "  " << program_name << " --input-db items.db --export-markdown items.md --skill-levels 0,100\n";
}

int main(int argc, char* argv[])
{
	// Парсинг аргументов командной строки
	std::string lib_path;
	std::string output_db;
	std::string export_markdown;
	std::string input_db;
	std::string skill_levels_str;

	for (int i = 1; i < argc; ++i)
	{
		if (std::strcmp(argv[i], "--lib-path") == 0 && i + 1 < argc)
		{
			lib_path = argv[++i];
		}
		else if (std::strcmp(argv[i], "--output") == 0 && i + 1 < argc)
		{
			output_db = argv[++i];
		}
		else if (std::strcmp(argv[i], "--export-markdown") == 0 && i + 1 < argc)
		{
			export_markdown = argv[++i];
		}
		else if (std::strcmp(argv[i], "--input-db") == 0 && i + 1 < argc)
		{
			input_db = argv[++i];
		}
		else if (std::strcmp(argv[i], "--skill-levels") == 0 && i + 1 < argc)
		{
			skill_levels_str = argv[++i];
		}
		else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0)
		{
			PrintUsage(argv[0]);
			return 0;
		}
		else
		{
			std::cerr << "Unknown argument: " << argv[i] << "\n\n";
			PrintUsage(argv[0]);
			return 1;
		}
	}

	// Parse skill levels
	std::vector<int> skill_levels;
	if (!skill_levels_str.empty())
	{
		// Парсинг строки "20,30,100" -> вектор {20, 30, 100}
		std::istringstream iss(skill_levels_str);
		std::string token;
		while (std::getline(iss, token, ','))
		{
			try
			{
				int level = std::stoi(token);
				skill_levels.push_back(level);
			}
			catch (...)
			{
				std::cerr << "Warning: Invalid skill level: " << token << "\n";
			}
		}
	}
	else
	{
		// По умолчанию - максимальная детализация (400 или 100)
		skill_levels = {100};
	}

	// Определить режим работы
	bool has_lib_path = !lib_path.empty();
	bool has_output_db = !output_db.empty();
	bool has_export_markdown = !export_markdown.empty();
	bool has_input_db = !input_db.empty();

	// Режим 1: Создание базы данных (обязательно lib-path и output)
	bool create_database_mode = has_lib_path && has_output_db;

	// Режим 2: Только экспорт Markdown (input-db и export-markdown)
	bool markdown_only_mode = !has_lib_path && has_input_db && has_export_markdown;

	// Режим 3: Комбинированный (lib-path, output, export-markdown)
	bool combined_mode = has_lib_path && has_output_db && has_export_markdown;

	if (!create_database_mode && !markdown_only_mode)
	{
		std::cerr << "Error: Invalid parameters combination\n\n";
		PrintUsage(argv[0]);
		return 1;
	}

	// Если создаём базу данных
	if (create_database_mode)
	{
		std::cout << "Identify Exporter for Bylins MUD\n";
		std::cout << "================================\n\n";
		std::cout << "Lib path: " << lib_path << "\n";
		std::cout << "Output database: " << output_db << "\n\n";

		IdentifyExporter exporter;
		if (!exporter.Run(lib_path, output_db))
		{
			std::cerr << "\nDatabase export failed!\n";
			return 1;
		}

		std::cout << "\nDatabase export completed successfully!\n";

		// Если также нужен Markdown экспорт
		if (combined_mode)
		{
			std::cout << "\n--- Markdown Export ---\n\n";
			std::cout << "Output markdown: " << export_markdown << "\n";
			std::cout << "Skill levels: ";
			for (size_t i = 0; i < skill_levels.size(); ++i)
			{
				std::cout << skill_levels[i];
				if (i < skill_levels.size() - 1)
					std::cout << ", ";
			}
			std::cout << "\n\n";

			if (!exporter.ExportToMarkdown(output_db, export_markdown, skill_levels))
			{
				std::cerr << "\nMarkdown export failed!\n";
				return 1;
			}

			std::cout << "\nAll exports completed successfully!\n";
		}

		return 0;
	}

	// Режим только Markdown экспорт
	if (markdown_only_mode)
	{
		std::cout << "Markdown Export Mode\n";
		std::cout << "====================\n\n";
		std::cout << "Input database: " << input_db << "\n";
		std::cout << "Output markdown: " << export_markdown << "\n";
		std::cout << "Skill levels: ";
		for (size_t i = 0; i < skill_levels.size(); ++i)
		{
			std::cout << skill_levels[i];
			if (i < skill_levels.size() - 1)
				std::cout << ", ";
		}
		std::cout << "\n\n";

		IdentifyExporter exporter;
		if (!exporter.ExportToMarkdown(input_db, export_markdown, skill_levels))
		{
			std::cerr << "\nMarkdown export failed!\n";
			return 1;
		}

		return 0;
	}

	return 0;
}

// vim: ts=4 sw=4 tw=0 noet syntax=cpp :
#include "identify_simulation.h"
#include "encoding.h"
#include "gameplay/core/constants.h"
#include "gameplay/affects/affect_contants.h"
#include <sstream>
#include <cstdio>

// Extern constants from MUD
extern const char *item_types[];
extern const char *material_name[];
extern const char *apply_types[];

// Симуляция команды опознать (identify) для разных уровней навыка
// Воспроизводит логику mort_show_obj_values без создания полноценного ObjData

std::string SimulateIdentify(const SimpleObject& obj, int fullness)
{
	std::ostringstream out;
	char buf[4096];

	// Все уровни: название и тип
	out << "Вы узнали следующее:\r\n";
	sprintf(buf, "Предмет \"%s\", тип : ", obj.short_desc.c_str());

	// Get type name (item_types is null-terminated array)
	if (obj.type >= 0 && obj.type < 100 && item_types[obj.type])
	{
		const char* type_name = item_types[obj.type];
		strcat(buf, type_name);
	}
	else
	{
		strcat(buf, "НЕИЗВЕСТЕН");
	}
	strcat(buf, "\r\n");
	out << buf;

	if (fullness < 20)
		return out.str();

	// fullness >= 20: вес, цена, рента
	sprintf(buf, "Вес: %d, Цена: %d, Рента: %d(%d)\r\n",
		obj.weight, obj.cost, obj.rent_off, obj.rent_on);
	out << buf;

	if (fullness < 30)
		return out.str();

	// fullness >= 30: материал, прочность (material_name is null-terminated array)
	if (obj.material >= 0 && obj.material < 100 && material_name[obj.material])
	{
		const char* mat_name = material_name[obj.material];
		sprintf(buf, "Материал : %s, Макс.прочность : %d, Тек.прочность : %d\r\n",
			mat_name, obj.max_durability, obj.current_durability);
	}
	else
	{
		sprintf(buf, "Материал : НЕИЗВЕСТЕН, Макс.прочность : %d, Тек.прочность : %d\r\n",
			obj.max_durability, obj.current_durability);
	}
	out << buf;

	if (fullness < 40)
		return out.str();

	// fullness >= 40: ограничения (no_flags)
	out << "Ограничения : ";
	// TODO: Parse and display no_flags from obj.no_flags_str
	out << "(нет данных)\r\n";

	if (fullness < 50)
		return out.str();

	// fullness >= 50: запрещено (anti_flags)
	out << "Недоступно : ";
	// TODO: Parse and display anti_flags from obj.anti_flags_str
	out << "(нет данных)\r\n";

	// Minimum remorts
	if (obj.minimum_remorts > 0)
	{
		sprintf(buf, "Минимальное количество перевоплощений : %d\r\n", obj.minimum_remorts);
		out << buf;
	}

	if (fullness < 60)
		return out.str();

	// fullness >= 60: доп. свойства (extra_flags)
	out << "Дополнительные свойства: ";
	// TODO: Parse and display extra_flags from obj.extra_flags_str
	out << "(нет данных)\r\n";

	if (fullness < 75)
		return out.str();

	// fullness >= 75: type-specific values
	switch (obj.type)
	{
		case EObjType::kWeapon:
		{
			int drndice = obj.value1;
			int drsdice = obj.value2;
			sprintf(buf, "Наносимые повреждения '%dD%d' в среднем %.1f.\r\n",
				drndice, drsdice, ((drsdice + 1) * drndice / 2.0));
			out << buf;
			break;
		}

		case EObjType::kArmor:
		case EObjType::kLightArmor:
		case EObjType::kMediumArmor:
		case EObjType::kHeavyArmor:
		{
			int ac = obj.value0;
			int armor = obj.value1;
			sprintf(buf, "Защита (AC) : %d\r\n", ac);
			out << buf;
			sprintf(buf, "Броня       : %d\r\n", armor);
			out << buf;
			break;
		}

		default:
			// Другие типы не реализованы в упрощённой версии
			break;
	}

	if (fullness < 90)
		return out.str();

	// fullness >= 90: weapon affects
	out << "Доп. свойства оружия: ";
	// TODO: Parse and display weapon affects
	out << "(нет данных)\r\n";

	if (fullness < 100)
		return out.str();

	// fullness >= 100: affects (stat modifiers)
	if (!obj.affects.empty())
	{
		out << "Аффекты:\r\n";
		for (const auto& aff : obj.affects)
		{
			const char* apply_name = "неизвестно";
			if (aff.apply_type >= 0 && aff.apply_type < 100 && apply_types[aff.apply_type])
			{
				apply_name = apply_types[aff.apply_type];
			}
			sprintf(buf, "  %s : %s%d\r\n",
				apply_name,
				aff.modifier > 0 ? "+" : "",
				aff.modifier);
			out << buf;
		}
	}

	return out.str();
}

// vim: ts=4 sw=4 tw=0 noet syntax=cpp :

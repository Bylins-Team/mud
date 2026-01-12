/**
 \file yaml_loaders.cpp
 \authors Created by Claude Code
 \date 2026.01.12
 \brief п═п╣п╟п╩п╦п╥п╟я├п╦я▐ я┐п╫п╦я└п╦я├п╦я─п╬п╡п╟п╫п╫п╬п╧ я│п╦я│я┌п╣п╪я▀ п╥п╟пЁя─я┐п╥п╨п╦ п╪п╦я─п╟ п╦п╥ YAML.
*/

#include "yaml_loaders.h"
#include "engine/entities/char_data.h"
#include "engine/entities/obj_data.h"
#include "engine/entities/room_data.h"
#include "engine/entities/zone.h"
#include "engine/db/world_objects.h"
#include "engine/db/db.h"
#include "engine/scripting/dg_scripts.h"
#include "gameplay/core/constants.h"
#include "gameplay/affects/affect_contants.h"
#include "utils/logger.h"
#include <fstream>
#include <sstream>

// External constant arrays from constants.cpp
extern const char *position_types[];
extern const char *genders[];
extern const char *item_types[];
extern const char *material_name[];
extern const char *extra_bits[];
extern const char *wear_bits[];
extern const char *anti_bits[];
extern const char *no_bits[];
extern const char *weapon_affects[];
extern const char *apply_types[];
extern const char *room_bits[];
extern const char *sector_types[];
extern const char *exit_bits[];
extern const char *equipment_types[];
extern const char *action_bits[];
extern const char *affected_bits[];

namespace YamlLoader {

/**
 * п≈п╟пЁя─я┐п╥п╦я┌я▄ YAML я└п╟п╧п╩ я│ п╬п╠я─п╟п╠п╬я┌п╨п╬п╧ п╬я┬п╦п╠п╬п╨.
 */
YAML::Node LoadYamlFile(const std::string& filename) {
	try {
		return YAML::LoadFile(filename);
	} catch (const YAML::Exception& e) {
		log("SYSERR: Failed to load YAML file '%s': %s", filename.c_str(), e.what());
		return YAML::Node();
	}
}

/**
 * п▒п╣п╥п╬п©п╟я│п╫п╬п╣ п©п╬п╩я┐я┤п╣п╫п╦п╣ п╥п╫п╟я┤п╣п╫п╦я▐ п╦п╥ YAML.
 */
template<typename T>
T GetYamlValue(const YAML::Node& node, const std::string& key, const T& default_value) {
	if (!node || !node[key]) {
		return default_value;
	}
	try {
		return node[key].as<T>();
	} catch (const YAML::Exception& e) {
		log("SYSERR: Failed to parse YAML key '%s': %s", key.c_str(), e.what());
		return default_value;
	}
}

// п╞п╡п╫п╟я▐ п╦п╫я│я┌п╟п╫я├п╦п╟я├п╦я▐ я┬п╟п╠п╩п╬п╫п╬п╡ п╢п╩я▐ п╬я│п╫п╬п╡п╫я▀я┘ я┌п╦п©п╬п╡
template int GetYamlValue<int>(const YAML::Node&, const std::string&, const int&);
template std::string GetYamlValue<std::string>(const YAML::Node&, const std::string&, const std::string&);
template bool GetYamlValue<bool>(const YAML::Node&, const std::string&, const bool&);

/**
 * п÷п╬п╩я┐я┤п╦я┌я▄ я│п©п╦я│п╬п╨ я│я┌я─п╬п╨ п╦п╥ YAML.
 */
std::vector<std::string> GetYamlStringList(const YAML::Node& node, const std::string& key) {
	std::vector<std::string> result;

	if (!node || !node[key] || !node[key].IsSequence()) {
		return result;
	}

	try {
		for (const auto& item : node[key]) {
			result.push_back(item.as<std::string>());
		}
	} catch (const YAML::Exception& e) {
		log("SYSERR: Failed to parse YAML string list '%s': %s", key.c_str(), e.what());
	}

	return result;
}

/**
 * п≈п╟пЁя─я┐п╥п╦я┌я▄ п╪п╬п╠п╟ п╦п╥ YAML я└п╟п╧п╩п╟.
 */
CharData* LoadMobile(const std::string& filename, const ConstantsMapper& mapper) {
	YAML::Node doc = LoadYamlFile(filename);
	if (!doc) {
		return nullptr;
	}

	// п║п╬п╥п╢п╟я┌я▄ CharData
	CharData* mob = new CharData();
	if (!mob) {
		log("SYSERR: Failed to allocate CharData for '%s'", filename.c_str());
		return nullptr;
	}

	// Vnum
	mob->set_rnum(GetYamlValue<int>(doc, "vnum", -1));

	// п≤п╪п╣п╫п╟ п╡ 7 п©п╟п╢п╣п╤п╟я┘
	YAML::Node names = doc["names"];
	if (names) {
		mob->SetCharAliases(GetYamlValue<std::string>(names, "aliases", ""));
		mob->set_npc_name(GetYamlValue<std::string>(names, "nominative", ""));
		mob->player_data.PNames[ECase::kGen] = GetYamlValue<std::string>(names, "genitive", "");
		mob->player_data.PNames[ECase::kDat] = GetYamlValue<std::string>(names, "dative", "");
		mob->player_data.PNames[ECase::kAcc] = GetYamlValue<std::string>(names, "accusative", "");
		mob->player_data.PNames[ECase::kIns] = GetYamlValue<std::string>(names, "instrumental", "");
		mob->player_data.PNames[ECase::kPre] = GetYamlValue<std::string>(names, "prepositional", "");
	}

	// п·п©п╦я│п╟п╫п╦я▐
	YAML::Node descriptions = doc["descriptions"];
	if (descriptions) {
		mob->player_data.long_descr = GetYamlValue<std::string>(descriptions, "short", "");
		mob->player_data.description = GetYamlValue<std::string>(descriptions, "long", "");
	}

	// Action flags (MOB_x)
	std::vector<std::string> action_flags = GetYamlStringList(doc, "action_flags");
	if (!action_flags.empty()) {
		std::vector<Bitvector> flags = mapper.NamesToBitvector(action_flags, action_bits, FlagData::kPlanesNumber);
		for (size_t plane = 0; plane < FlagData::kPlanesNumber; ++plane) {
			mob->char_specials.saved.act.set_plane(plane, flags[plane]);
		}
	}

	// Affect flags (AFF_x)
	std::vector<std::string> affect_flags = GetYamlStringList(doc, "affect_flags");
	if (!affect_flags.empty()) {
		std::vector<Bitvector> flags = mapper.NamesToBitvector(affect_flags, affected_bits, FlagData::kPlanesNumber);
		for (size_t plane = 0; plane < FlagData::kPlanesNumber; ++plane) {
			AFF_FLAGS(mob).set_plane(plane, flags[plane]);
		}
	}

	// Alignment
	mob->char_specials.saved.alignment = GetYamlValue<int>(doc, "alignment", 0);

	// п▒п╟п╥п╬п╡я▀п╣ я┘п╟я─п╟п╨я┌п╣я─п╦я│я┌п╦п╨п╦
	YAML::Node stats = doc["stats"];
	if (stats) {
		mob->set_level(GetYamlValue<int>(stats, "level", 1));
		int hitroll_penalty = GetYamlValue<int>(stats, "hitroll_penalty", 20);
		mob->real_abils.hitroll = 20 - hitroll_penalty;
		mob->real_abils.armor = GetYamlValue<int>(stats, "armor", 100);

		// HP: dice_count d dice_size + bonus
		YAML::Node hp = stats["hp"];
		if (hp) {
			mob->mem_queue.total = GetYamlValue<int>(hp, "dice_count", 0);
			mob->mem_queue.stored = GetYamlValue<int>(hp, "dice_size", 0);
			mob->set_hit(GetYamlValue<int>(hp, "bonus", 0));
			mob->set_max_hit(0); // п╓п╩п╟пЁ я┤я┌п╬ HP я█я┌п╬ xdy+z
		}

		// пёя─п╬п╫: dice_count d dice_size + bonus
		YAML::Node damage = stats["damage"];
		if (damage) {
			mob->mob_specials.damnodice = GetYamlValue<int>(damage, "dice_count", 0);
			mob->mob_specials.damsizedice = GetYamlValue<int>(damage, "dice_size", 0);
			mob->real_abils.damroll = GetYamlValue<int>(damage, "bonus", 0);
		}
	}

	// п≈п╬п╩п╬я┌п╬: dice_count d dice_size + bonus
	YAML::Node gold = doc["gold"];
	if (gold) {
		int bonus = GetYamlValue<int>(gold, "bonus", 0);
		mob->set_gold(bonus, false);
		GET_GOLD_NoDs(mob) = GetYamlValue<int>(gold, "dice_count", 0);
		GET_GOLD_SiDs(mob) = GetYamlValue<int>(gold, "dice_size", 0);
	}

	// п·п©я▀я┌
	mob->set_exp(GetYamlValue<int>(doc, "experience", 0));

	// п÷п╬п╥п╦я├п╦я▐ п╦ п©п╬п╩
	YAML::Node position = doc["position"];
	if (position) {
		std::string default_pos = GetYamlValue<std::string>(position, "default", "kStanding");
		int pos_index = mapper.FindNameIndex(default_pos, position_types);
		if (pos_index >= 0) {
			mob->SetPosition(static_cast<EPosition>(pos_index));
		}

		std::string start_pos = GetYamlValue<std::string>(position, "start", "kStanding");
		pos_index = mapper.FindNameIndex(start_pos, position_types);
		if (pos_index >= 0) {
			mob->mob_specials.default_pos = static_cast<EPosition>(pos_index);
		}
	}

	// п÷п╬п╩
	std::string sex = GetYamlValue<std::string>(doc, "sex", "kMale");
	int sex_index = mapper.FindNameIndex(sex, genders);
	if (sex_index >= 0) {
		mob->set_sex(static_cast<EGender>(sex_index));
	}

	// п═п╟я│я┬п╦я─п╣п╫п╫я▀п╣ я┘п╟я─п╟п╨я┌п╣я─п╦я│я┌п╦п╨п╦ (E-spec)
	YAML::Node attributes = doc["attributes"];
	if (attributes) {
		mob->set_str(GetYamlValue<int>(attributes, "strength", 10));
		mob->set_dex(GetYamlValue<int>(attributes, "dexterity", 10));
		mob->set_int(GetYamlValue<int>(attributes, "intelligence", 10));
		mob->set_wis(GetYamlValue<int>(attributes, "wisdom", 10));
		mob->set_con(GetYamlValue<int>(attributes, "constitution", 10));
		mob->set_cha(GetYamlValue<int>(attributes, "charisma", 10));
	}

	// п╓п╦п╥п╦я┤п╣я│п╨п╦п╣ п©п╟я─п╟п╪п╣я┌я─я▀
	mob->real_abils.size = GetYamlValue<int>(doc, "size", 50);
	mob->player_data.height = GetYamlValue<int>(doc, "height", 170);
	mob->player_data.weight = GetYamlValue<int>(doc, "weight", 170);

	// п п╩п╟я│я│ п╦ я─п╟я│п╟
	mob->set_class(static_cast<ECharClass>(GetYamlValue<int>(doc, "mob_class", 0)));
	mob->player_data.Race = static_cast<ENpcRace>(GetYamlValue<int>(doc, "race", 0));

	// п═п╣п╪п╬я─я┌
	mob->set_remort(GetYamlValue<int>(doc, "remort", 0));

	// п²п╟п╡я▀п╨п╦
	YAML::Node skills = doc["skills"];
	if (skills && skills.IsSequence()) {
		for (const auto& skill_node : skills) {
			int skill_id = GetYamlValue<int>(skill_node, "skill_id", -1);
			int value = GetYamlValue<int>(skill_node, "value", 0);
			if (skill_id >= 0) {
				mob->set_skill(static_cast<ESkill>(skill_id), value);
			}
		}
	}

	// п╒я─п╦пЁпЁп╣я─я▀
	std::vector<std::string> triggers = GetYamlStringList(doc, "triggers");
	// TODO: п÷я─п╦п╨я─п╣п©п╦я┌я▄ я┌я─п╦пЁпЁп╣я─я▀

	log("SYSLOG: Loaded mob from YAML: %s (vnum %d)", filename.c_str(), mob->get_rnum());
	return mob;
}

/**
 * п≈п╟пЁя─я┐п╥п╦я┌я▄ п╬п╠я┼п╣п╨я┌ п╦п╥ YAML я└п╟п╧п╩п╟.
 */
ObjData* LoadObject(const std::string& filename, const ConstantsMapper& mapper) {
	YAML::Node doc = LoadYamlFile(filename);
	if (!doc) {
		return nullptr;
	}

	// п÷п╬п╩я┐я┤п╦я┌я▄ vnum
	ObjVnum vnum = GetYamlValue<int>(doc, "vnum", -1);
	if (vnum < 0) {
		log("SYSERR: Invalid vnum in YAML object file '%s'", filename.c_str());
		return nullptr;
	}

	// п║п╬п╥п╢п╟я┌я▄ п©я─п╬я┌п╬я┌п╦п© п╬п╠я┼п╣п╨я┌п╟
	CObjectPrototype proto(vnum);

	// п≤п╪п╣п╫п╟ п╡ 7 п©п╟п╢п╣п╤п╟я┘
	YAML::Node names = doc["names"];
	if (names) {
		proto.set_aliases(GetYamlValue<std::string>(names, "aliases", ""));
		proto.set_short_description(GetYamlValue<std::string>(names, "nominative", ""));
		proto.set_PName(ECase::kNom, GetYamlValue<std::string>(names, "nominative", ""));
		proto.set_PName(ECase::kGen, GetYamlValue<std::string>(names, "genitive", ""));
		proto.set_PName(ECase::kDat, GetYamlValue<std::string>(names, "dative", ""));
		proto.set_PName(ECase::kAcc, GetYamlValue<std::string>(names, "accusative", ""));
		proto.set_PName(ECase::kIns, GetYamlValue<std::string>(names, "instrumental", ""));
		proto.set_PName(ECase::kPre, GetYamlValue<std::string>(names, "prepositional", ""));
	}

	// п·п©п╦я│п╟п╫п╦я▐
	YAML::Node descriptions = doc["descriptions"];
	if (descriptions) {
		proto.set_description(GetYamlValue<std::string>(descriptions, "room", ""));
		proto.set_action_description(GetYamlValue<std::string>(descriptions, "action", ""));
	}

	// п╒п╦п© п╬п╠я┼п╣п╨я┌п╟
	std::string type_str = GetYamlValue<std::string>(doc, "type", "");
	if (!type_str.empty()) {
		int type_index = mapper.FindNameIndex(type_str, item_types);
		if (type_index >= 0) {
			proto.set_type(static_cast<EObjType>(type_index));
		}
	}

	// п÷п╟я─п╟п╪п╣я┌я─я▀ п©я─п╬я┤п╫п╬я│я┌п╦
	YAML::Node durability = doc["durability"];
	if (durability) {
		proto.set_maximum_durability(GetYamlValue<int>(durability, "maximum", 100));
		proto.set_current_durability(GetYamlValue<int>(durability, "current", 100));
	}

	// п°п╟я┌п╣я─п╦п╟п╩
	std::string material_str = GetYamlValue<std::string>(doc, "material", "");
	if (!material_str.empty()) {
		int mat_index = mapper.FindNameIndex(material_str, material_name);
		if (mat_index >= 0) {
			proto.set_material(static_cast<EObjMaterial>(mat_index));
		}
	}

	// п÷п╬п╩
	std::string sex_str = GetYamlValue<std::string>(doc, "sex", "kMale");
	int sex_index = mapper.FindNameIndex(sex_str, genders);
	if (sex_index >= 0) {
		proto.set_sex(static_cast<EGender>(sex_index));
	}

	// п╒п╟п╧п╪п╣я─, я┐я─п╬п╡п╣п╫я▄, п╥п╟п╨п╩п╦п╫п╟п╫п╦п╣
	proto.set_timer(GetYamlValue<int>(doc, "timer", CObjectPrototype::DEFAULT_TIMER));
	proto.set_level(GetYamlValue<int>(doc, "level", 0));
	proto.set_spell(GetYamlValue<int>(doc, "spell", -1));
	proto.set_spec_param(GetYamlValue<int>(doc, "spec_param", -1));

	// Extra flags (extra_bits)
	std::vector<std::string> extra_flags_list = GetYamlStringList(doc, "extra_flags");
	if (!extra_flags_list.empty()) {
		std::vector<Bitvector> flags = mapper.NamesToBitvector(extra_flags_list, extra_bits, FlagData::kPlanesNumber);
		FlagData extra_flags;
		for (size_t plane = 0; plane < FlagData::kPlanesNumber; ++plane) {
			extra_flags.set_plane(plane, flags[plane]);
		}
		proto.set_extra_flags(extra_flags);
	}

	// Wear flags (wear_bits)
	std::vector<std::string> wear_flags_list = GetYamlStringList(doc, "wear_flags");
	if (!wear_flags_list.empty()) {
		CObjectPrototype::wear_flags_t wear = 0;
		for (const auto& flag_name : wear_flags_list) {
			int flag_index = mapper.FindNameIndex(flag_name, wear_bits);
			if (flag_index >= 0) {
				wear |= (1 << flag_index);
			}
		}
		proto.set_wear_flags(wear);
	}

	// Anti flags (anti_bits)
	std::vector<std::string> anti_flags_list = GetYamlStringList(doc, "anti_flags");
	if (!anti_flags_list.empty()) {
		std::vector<Bitvector> flags = mapper.NamesToBitvector(anti_flags_list, anti_bits, FlagData::kPlanesNumber);
		FlagData anti_flags;
		for (size_t plane = 0; plane < FlagData::kPlanesNumber; ++plane) {
			anti_flags.set_plane(plane, flags[plane]);
		}
		proto.set_anti_flags(anti_flags);
	}

	// No flags (no_bits)
	std::vector<std::string> no_flags_list = GetYamlStringList(doc, "no_flags");
	if (!no_flags_list.empty()) {
		std::vector<Bitvector> flags = mapper.NamesToBitvector(no_flags_list, no_bits, FlagData::kPlanesNumber);
		FlagData no_flags;
		for (size_t plane = 0; plane < FlagData::kPlanesNumber; ++plane) {
			no_flags.set_plane(plane, flags[plane]);
		}
		proto.set_no_flags(no_flags);
	}

	// Weapon affects (weapon_affects)
	std::vector<std::string> waffect_flags_list = GetYamlStringList(doc, "weapon_affects");
	if (!waffect_flags_list.empty()) {
		std::vector<Bitvector> flags = mapper.NamesToBitvector(waffect_flags_list, weapon_affects, FlagData::kPlanesNumber);
		FlagData waffect_flags;
		for (size_t plane = 0; plane < FlagData::kPlanesNumber; ++plane) {
			waffect_flags.set_plane(plane, flags[plane]);
		}
		proto.SetWeaponAffectFlags(waffect_flags);
	}

	// Values (4 п╥п╫п╟я┤п╣п╫п╦я▐, я│п╪я▀я│п╩ п╥п╟п╡п╦я│п╦я┌ п╬я┌ я┌п╦п©п╟)
	YAML::Node values = doc["values"];
	if (values) {
		proto.set_val(0, GetYamlValue<int>(values, "value0", 0));
		proto.set_val(1, GetYamlValue<int>(values, "value1", 0));
		proto.set_val(2, GetYamlValue<int>(values, "value2", 0));
		proto.set_val(3, GetYamlValue<int>(values, "value3", 0));
	}

	// п▓п╣я│, я├п╣п╫п╟, я─п╣п╫я┌п╟
	proto.set_weight(GetYamlValue<int>(doc, "weight", 1));
	proto.set_cost(GetYamlValue<int>(doc, "cost", 0));
	YAML::Node rent = doc["rent"];
	if (rent) {
		proto.set_rent_off(GetYamlValue<int>(rent, "off", 0));
		proto.set_rent_on(GetYamlValue<int>(rent, "on", 0));
	}

	// Max in world
	proto.set_max_in_world(GetYamlValue<int>(doc, "max_in_world", -1));

	// Minimum remorts
	proto.set_minimum_remorts(GetYamlValue<int>(doc, "minimum_remorts", 0));

	// Applies (п©я─п╦п╪п╣п╫я▐п╣п╪я▀п╣ я█я└я└п╣п╨я┌я▀)
	YAML::Node applies = doc["applies"];
	if (applies && applies.IsSequence()) {
		size_t apply_index = 0;
		for (const auto& apply_node : applies) {
			if (apply_index >= kMaxObjAffect) {
				break;
			}
			std::string location_str = GetYamlValue<std::string>(apply_node, "location", "");
			int modifier = GetYamlValue<int>(apply_node, "modifier", 0);
			if (!location_str.empty()) {
				int loc_index = mapper.FindNameIndex(location_str, apply_types);
				if (loc_index >= 0) {
					proto.set_affected(apply_index, static_cast<EApply>(loc_index), modifier);
					++apply_index;
				}
			}
		}
	}

	// Extra descriptions
	YAML::Node extra_descs = doc["extra_descriptions"];
	if (extra_descs && extra_descs.IsSequence()) {
		for (const auto& ed_node : extra_descs) {
			std::string keywords = GetYamlValue<std::string>(ed_node, "keywords", "");
			std::string description = GetYamlValue<std::string>(ed_node, "description", "");
			if (!keywords.empty()) {
				proto.set_ex_description(keywords.c_str(), description.c_str());
			}
		}
	}

	// Triggers
	YAML::Node triggers = doc["triggers"];
	if (triggers && triggers.IsSequence()) {
		for (const auto& trig_node : triggers) {
			try {
				ObjVnum trig_vnum = trig_node.as<int>();
				proto.add_proto_script(trig_vnum);
			} catch (const YAML::Exception&) {
				// п≤пЁп╫п╬я─п╦я─я┐п╣п╪ п╬я┬п╦п╠п╨п╦ п©п╟я─я│п╦п╫пЁп╟ я┌я─п╦пЁпЁп╣я─п╬п╡
			}
		}
	}

	// Skills
	YAML::Node skills = doc["skills"];
	if (skills && skills.IsSequence()) {
		for (const auto& skill_node : skills) {
			int skill_id = GetYamlValue<int>(skill_node, "skill_id", -1);
			int value = GetYamlValue<int>(skill_node, "value", 0);
			if (skill_id >= 0) {
				proto.set_skill(static_cast<ESkill>(skill_id), value);
			}
		}
	}

	// п║п╬п╥п╢п╟я┌я▄ п╬п╠я┼п╣п╨я┌ п╦п╥ п©я─п╬я┌п╬я┌п╦п©п╟
	ObjData* obj = new ObjData(proto);
	if (!obj) {
		log("SYSERR: Failed to allocate ObjData for '%s'", filename.c_str());
		return nullptr;
	}

	log("SYSLOG: Loaded object from YAML: %s (vnum %d)", filename.c_str(), vnum);
	return obj;
}

/**
 * п≈п╟пЁя─я┐п╥п╦я┌я▄ п╨п╬п╪п╫п╟я┌я┐ п╦п╥ YAML я└п╟п╧п╩п╟.
 */
RoomData* LoadRoom(const std::string& filename, const ConstantsMapper& mapper) {
	YAML::Node doc = LoadYamlFile(filename);
	if (!doc) {
		return nullptr;
	}

	// п÷п╬п╩я┐я┤п╦я┌я▄ vnum
	RoomVnum vnum = GetYamlValue<int>(doc, "vnum", -1);
	if (vnum < 0) {
		log("SYSERR: Invalid vnum in YAML room file '%s'", filename.c_str());
		return nullptr;
	}

	// п║п╬п╥п╢п╟я┌я▄ п╨п╬п╪п╫п╟я┌я┐
	RoomData* room = new RoomData();
	if (!room) {
		log("SYSERR: Failed to allocate RoomData for '%s'", filename.c_str());
		return nullptr;
	}

	// п▒п╟п╥п╬п╡я▀п╣ п©п╬п╩я▐
	room->vnum = vnum;
	room->zone_rn = GetYamlValue<int>(doc, "zone", 0);

	// п≤п╫п╦я├п╦п╟п╩п╦п╥п╦я─п╬п╡п╟я┌я▄ п©п╬п╩я▐ п╨п╟п╨ п╡ legacy loader
	room->func = nullptr;
	room->contents = nullptr;
	room->track = nullptr;
	room->light = 0;
	room->fires = 0;
	room->gdark = 0;
	room->glight = 0;
	room->proto_script.reset(new ObjData::triggers_list_t());

	// п²п╟п╥п╡п╟п╫п╦п╣
	std::string name_str = GetYamlValue<std::string>(doc, "name", "");
	if (!name_str.empty()) {
		room->set_name(name_str);
	}

	// п║п╣п╨я┌п╬я─ (я┌п╦п© п╪п╣я│я┌п╫п╬я│я┌п╦)
	std::string sector_str = GetYamlValue<std::string>(doc, "sector", "");
	if (!sector_str.empty()) {
		int sector_index = mapper.FindNameIndex(sector_str, sector_types);
		if (sector_index >= 0) {
			room->sector_type = sector_index;
		}
	}

	// п╓п╩п╟пЁп╦ п╨п╬п╪п╫п╟я┌я▀ (room_bits)
	std::vector<std::string> room_flags_list = GetYamlStringList(doc, "room_flags");
	if (!room_flags_list.empty()) {
		std::vector<Bitvector> flags = mapper.NamesToBitvector(room_flags_list, room_bits, FlagData::kPlanesNumber);
		FlagData room_flags;
		for (size_t plane = 0; plane < FlagData::kPlanesNumber; ++plane) {
			room_flags.set_plane(plane, flags[plane]);
		}
		room->write_flags(room_flags);
	}

	// п▓я▀я┘п╬п╢я▀
	YAML::Node exits = doc["exits"];
	if (exits && exits.IsSequence()) {
		for (const auto& exit_node : exits) {
			std::string dir_str = GetYamlValue<std::string>(exit_node, "direction", "");
			if (dir_str.empty()) {
				continue;
			}

			// п·п©я─п╣п╢п╣п╩п╦я┌я▄ п╫п╟п©я─п╟п╡п╩п╣п╫п╦п╣
			int dir = -1;
			if (dir_str == "kNorth" || dir_str == "north" || dir_str == "0") dir = EDirection::kNorth;
			else if (dir_str == "kEast" || dir_str == "east" || dir_str == "1") dir = EDirection::kEast;
			else if (dir_str == "kSouth" || dir_str == "south" || dir_str == "2") dir = EDirection::kSouth;
			else if (dir_str == "kWest" || dir_str == "west" || dir_str == "3") dir = EDirection::kWest;
			else if (dir_str == "kUp" || dir_str == "up" || dir_str == "4") dir = EDirection::kUp;
			else if (dir_str == "kDown" || dir_str == "down" || dir_str == "5") dir = EDirection::kDown;

			if (dir < 0 || dir >= EDirection::kMaxDirNum) {
				log("SYSERR: Invalid direction '%s' in room %d", dir_str.c_str(), vnum);
				continue;
			}

			// п║п╬п╥п╢п╟я┌я▄ п╡я▀я┘п╬п╢
			auto exit_data = std::make_shared<ExitData>();

			// п·п©п╦я│п╟п╫п╦п╣
			std::string desc = GetYamlValue<std::string>(exit_node, "description", "");
			exit_data->general_description = desc;

			// п п╩я▌я┤п╣п╡я▀п╣ я│п╩п╬п╡п╟
			std::string keywords = GetYamlValue<std::string>(exit_node, "keywords", "");
			if (!keywords.empty()) {
				exit_data->set_keywords(keywords);
			}

			// п я┐п╢п╟ п╡п╣п╢я▒я┌
			int to_room = GetYamlValue<int>(exit_node, "to_room", -1);
			exit_data->to_room(to_room);

			// п п╩я▌я┤
			exit_data->key = GetYamlValue<int>(exit_node, "key", -1);

			// п║п╩п╬п╤п╫п╬я│я┌я▄ п╥п╟п╪п╨п╟
			exit_data->lock_complexity = GetYamlValue<int>(exit_node, "lock_complexity", 0);

			// п╓п╩п╟пЁп╦ п╡я▀я┘п╬п╢п╟ - п╪п╬п╤п╣я┌ п╠я▀я┌я▄ exit_flags (я┤п╦я│п╩п╬) п╦п╩п╦ flags (я│п©п╦я│п╬п╨)
			if (exit_node["exit_flags"]) {
				exit_data->exit_info = GetYamlValue<int>(exit_node, "exit_flags", 0);
			} else {
				std::vector<std::string> exit_flags_list = GetYamlStringList(exit_node, "flags");
				if (!exit_flags_list.empty()) {
					byte exit_flags = 0;
					for (const auto& flag_name : exit_flags_list) {
						int flag_index = mapper.FindNameIndex(flag_name, exit_bits);
						if (flag_index >= 0) {
							exit_flags |= (1 << flag_index);
						}
					}
					exit_data->exit_info = exit_flags;
				}
			}

			room->dir_option[dir] = exit_data;
			// п╒п╟п╨п╤п╣ я┐я│я┌п╟п╫п╬п╡п╦я┌я▄ п©я─п╬я┌п╬я┌п╦п©
			room->dir_option_proto[dir] = std::make_shared<ExitData>(*exit_data);
		}
	}

	// Extra descriptions
	YAML::Node extra_descs = doc["extra_descriptions"];
	if (extra_descs && extra_descs.IsSequence()) {
		ExtraDescription* head = nullptr;
		ExtraDescription* current = nullptr;
		for (const auto& ed_node : extra_descs) {
			std::string keywords = GetYamlValue<std::string>(ed_node, "keywords", "");
			std::string description = GetYamlValue<std::string>(ed_node, "description", "");
			if (!keywords.empty()) {
				ExtraDescription* ed = new ExtraDescription();
				ed->keyword = str_dup(keywords.c_str());
				ed->description = str_dup(description.c_str());
				ed->next.reset();
				if (!head) {
					head = ed;
					current = ed;
				} else {
					current->next.reset(ed);
					current = ed;
				}
			}
		}
		if (head) {
			room->ex_description.reset(head);
		}
	}

	// п╒я─п╦пЁпЁп╣я─я▀ (proto_script я┐п╤п╣ п╦п╫п╦я├п╦п╟п╩п╦п╥п╦я─п╬п╡п╟п╫ п╡я▀я┬п╣)
	YAML::Node triggers = doc["triggers"];
	if (triggers && triggers.IsSequence()) {
		for (const auto& trig_node : triggers) {
			try {
				ObjVnum trig_vnum = trig_node.as<int>();
				room->proto_script->push_back(trig_vnum);
			} catch (const YAML::Exception&) {
				// п≤пЁп╫п╬я─п╦я─я┐п╣п╪ п╬я┬п╦п╠п╨п╦ п©п╟я─я│п╦п╫пЁп╟ я┌я─п╦пЁпЁп╣я─п╬п╡
			}
		}
	}

	log("SYSLOG: Loaded room from YAML: %s (vnum %d)", filename.c_str(), vnum);
	return room;
}

/**
 * п≈п╟пЁя─я┐п╥п╦я┌я▄ п╥п╬п╫я┐ п╦п╥ YAML я└п╟п╧п╩п╟.
 */
ZoneData* LoadZone(const std::string& filename, const ConstantsMapper& mapper) {
	YAML::Node doc = LoadYamlFile(filename);
	if (!doc) {
		return nullptr;
	}

	// п÷п╬п╩я┐я┤п╦я┌я▄ vnum
	ZoneVnum vnum = GetYamlValue<int>(doc, "vnum", -1);
	if (vnum < 0) {
		log("SYSERR: Invalid vnum in YAML zone file '%s'", filename.c_str());
		return nullptr;
	}

	// п║п╬п╥п╢п╟я┌я▄ п╥п╬п╫я┐
	ZoneData* zone = new ZoneData();
	if (!zone) {
		log("SYSERR: Failed to allocate ZoneData for '%s'", filename.c_str());
		return nullptr;
	}

	// п▒п╟п╥п╬п╡я▀п╣ п©п╬п╩я▐
	zone->vnum = vnum;
	zone->name = GetYamlValue<std::string>(doc, "name", "");
	zone->comment = GetYamlValue<std::string>(doc, "comment", "");
	zone->author = GetYamlValue<std::string>(doc, "author", "");
	zone->description = GetYamlValue<std::string>(doc, "description", "");
	zone->location = GetYamlValue<std::string>(doc, "location", "");

	// п°п╣я┌п╟п╢п╟п╫п╫я▀п╣
	YAML::Node metadata = doc["metadata"];
	if (metadata) {
		zone->level = GetYamlValue<int>(metadata, "level", 1);
		zone->type = GetYamlValue<int>(metadata, "type", 0);
		zone->group = GetYamlValue<int>(metadata, "group", 0);
		zone->mob_level = GetYamlValue<int>(metadata, "mob_level", 0);
		zone->is_town = GetYamlValue<bool>(metadata, "is_town", false);
		zone->under_construction = GetYamlValue<bool>(metadata, "under_construction", false);
	}

	// п÷п╟я─п╟п╪п╣я┌я─я▀ я─п╣я│п©п╟я┐п╫п╟
	YAML::Node reset = doc["reset"];
	if (reset) {
		zone->top = GetYamlValue<int>(reset, "top_room", 0);
		zone->lifespan = GetYamlValue<int>(reset, "lifespan", 10);
		zone->reset_mode = GetYamlValue<int>(reset, "reset_mode", 2);
		zone->reset_idle = GetYamlValue<bool>(reset, "reset_idle", false);
	}

	// п п╬п╪п╟п╫п╢я▀ я─п╣я│п©п╟я┐п╫п╟
	YAML::Node commands = doc["commands"];
	if (commands && commands.IsSequence()) {
		// п÷п╬п╢я│я┤п╦я┌п╟я┌я▄ п╨п╬п╩п╦я┤п╣я│я┌п╡п╬ п╨п╬п╪п╟п╫п╢
		size_t cmd_count = commands.size() + 1; // +1 п╢п╩я▐ я┌п╣я─п╪п╦п╫п╟я┌п╬я─п╟ 'S'
		zone->cmd = new struct reset_com[cmd_count];
		if (!zone->cmd) {
			log("SYSERR: Failed to allocate reset commands for zone %d", vnum);
			delete zone;
			return nullptr;
		}

		size_t cmd_index = 0;
		for (const auto& cmd_node : commands) {
			std::string type_str = GetYamlValue<std::string>(cmd_node, "type", "");
			if (type_str.empty()) {
				continue;
			}

			struct reset_com& cmd = zone->cmd[cmd_index];
			cmd.line = static_cast<int>(cmd_index);
			cmd.if_flag = GetYamlValue<int>(cmd_node, "if_flag", 0);
			cmd.sarg1 = nullptr;
			cmd.sarg2 = nullptr;

			// п·п©я─п╣п╢п╣п╩п╦я┌я▄ я┌п╦п© п╨п╬п╪п╟п╫п╢я▀
			if (type_str == "LOAD_MOB" || type_str == "M") {
				cmd.command = 'M';
				cmd.arg1 = GetYamlValue<int>(cmd_node, "mob_vnum", 0);
				cmd.arg2 = GetYamlValue<int>(cmd_node, "max_in_world", 1);
				cmd.arg3 = GetYamlValue<int>(cmd_node, "room_vnum", 0);
				cmd.arg4 = GetYamlValue<int>(cmd_node, "max_in_room", 1);
			} else if (type_str == "LOAD_OBJ" || type_str == "O") {
				cmd.command = 'O';
				cmd.arg1 = GetYamlValue<int>(cmd_node, "obj_vnum", 0);
				cmd.arg2 = GetYamlValue<int>(cmd_node, "max", 1);
				cmd.arg3 = GetYamlValue<int>(cmd_node, "room_vnum", 0);
				cmd.arg4 = GetYamlValue<int>(cmd_node, "load_prob", 100);
			} else if (type_str == "GIVE_OBJ" || type_str == "G") {
				cmd.command = 'G';
				cmd.arg1 = GetYamlValue<int>(cmd_node, "obj_vnum", 0);
				cmd.arg2 = GetYamlValue<int>(cmd_node, "max", 1);
				cmd.arg3 = 0;
				cmd.arg4 = GetYamlValue<int>(cmd_node, "load_prob", 100);
			} else if (type_str == "EQUIP_MOB" || type_str == "E") {
				cmd.command = 'E';
				cmd.arg1 = GetYamlValue<int>(cmd_node, "obj_vnum", 0);
				cmd.arg2 = GetYamlValue<int>(cmd_node, "max", 1);
				// wear_pos п╪п╬п╤п╣я┌ п╠я▀я┌я▄ я│я┌я─п╬п╨п╬п╧ п╦п╩п╦ я┤п╦я│п╩п╬п╪
				std::string wear_str = GetYamlValue<std::string>(cmd_node, "wear_pos", "");
				if (!wear_str.empty()) {
					int wear_index = mapper.FindNameIndex(wear_str, equipment_types);
					cmd.arg3 = wear_index >= 0 ? wear_index : 0;
				} else {
					cmd.arg3 = GetYamlValue<int>(cmd_node, "wear_pos", 0);
				}
				cmd.arg4 = GetYamlValue<int>(cmd_node, "load_prob", 100);
			} else if (type_str == "PUT_IN_CONTAINER" || type_str == "P") {
				cmd.command = 'P';
				cmd.arg1 = GetYamlValue<int>(cmd_node, "obj_vnum", 0);
				cmd.arg2 = GetYamlValue<int>(cmd_node, "max", 1);
				cmd.arg3 = GetYamlValue<int>(cmd_node, "container_vnum", 0);
				cmd.arg4 = GetYamlValue<int>(cmd_node, "load_prob", 100);
			} else if (type_str == "SET_DOOR" || type_str == "D") {
				cmd.command = 'D';
				cmd.arg1 = GetYamlValue<int>(cmd_node, "room_vnum", 0);
				// direction п╪п╬п╤п╣я┌ п╠я▀я┌я▄ я│я┌я─п╬п╨п╬п╧ п╦п╩п╦ я┤п╦я│п╩п╬п╪
				std::string dir_str = GetYamlValue<std::string>(cmd_node, "direction", "");
				if (!dir_str.empty()) {
					if (dir_str == "kNorth" || dir_str == "north") cmd.arg2 = 0;
					else if (dir_str == "kEast" || dir_str == "east") cmd.arg2 = 1;
					else if (dir_str == "kSouth" || dir_str == "south") cmd.arg2 = 2;
					else if (dir_str == "kWest" || dir_str == "west") cmd.arg2 = 3;
					else if (dir_str == "kUp" || dir_str == "up") cmd.arg2 = 4;
					else if (dir_str == "kDown" || dir_str == "down") cmd.arg2 = 5;
					else cmd.arg2 = GetYamlValue<int>(cmd_node, "direction", 0);
				} else {
					cmd.arg2 = GetYamlValue<int>(cmd_node, "direction", 0);
				}
				cmd.arg3 = GetYamlValue<int>(cmd_node, "state", 0);
				cmd.arg4 = 0;
			} else if (type_str == "REMOVE_OBJ" || type_str == "R") {
				cmd.command = 'R';
				cmd.arg1 = GetYamlValue<int>(cmd_node, "room_vnum", 0);
				cmd.arg2 = GetYamlValue<int>(cmd_node, "obj_vnum", 0);
				cmd.arg3 = 0;
				cmd.arg4 = 0;
			} else if (type_str == "ATTACH_TRIGGER" || type_str == "T") {
				cmd.command = 'T';
				cmd.arg1 = GetYamlValue<int>(cmd_node, "entity_type", 0); // 0=mob, 1=obj, 2=room
				cmd.arg2 = GetYamlValue<int>(cmd_node, "trigger_vnum", 0);
				cmd.arg3 = GetYamlValue<int>(cmd_node, "entity_vnum", 0);
				cmd.arg4 = 0;
			} else if (type_str == "SET_VARIABLE" || type_str == "V") {
				cmd.command = 'V';
				cmd.arg1 = GetYamlValue<int>(cmd_node, "context_type", 0);
				cmd.arg2 = GetYamlValue<int>(cmd_node, "context", 0);
				cmd.arg3 = GetYamlValue<int>(cmd_node, "trigger_vnum", 0);
				cmd.arg4 = 0;
				std::string var_name = GetYamlValue<std::string>(cmd_node, "var_name", "");
				std::string var_value = GetYamlValue<std::string>(cmd_node, "var_value", "");
				if (!var_name.empty()) {
					cmd.sarg1 = str_dup(var_name.c_str());
				}
				if (!var_value.empty()) {
					cmd.sarg2 = str_dup(var_value.c_str());
				}
			} else {
				log("SYSERR: Unknown zone command type '%s' in zone %d", type_str.c_str(), vnum);
				continue;
			}

			++cmd_index;
		}

		// п╒п╣я─п╪п╦п╫п╟я┌п╬я─
		zone->cmd[cmd_index].command = 'S';
		zone->cmd[cmd_index].if_flag = 0;
		zone->cmd[cmd_index].arg1 = 0;
		zone->cmd[cmd_index].arg2 = 0;
		zone->cmd[cmd_index].arg3 = 0;
		zone->cmd[cmd_index].arg4 = 0;
		zone->cmd[cmd_index].sarg1 = nullptr;
		zone->cmd[cmd_index].sarg2 = nullptr;
	} else {
		// п÷я┐я│я┌п╬п╧ я│п©п╦я│п╬п╨ п╨п╬п╪п╟п╫п╢
		zone->cmd = new struct reset_com[1];
		zone->cmd[0].command = 'S';
		zone->cmd[0].if_flag = 0;
		zone->cmd[0].arg1 = 0;
		zone->cmd[0].arg2 = 0;
		zone->cmd[0].arg3 = 0;
		zone->cmd[0].arg4 = 0;
		zone->cmd[0].sarg1 = nullptr;
		zone->cmd[0].sarg2 = nullptr;
	}

	log("SYSLOG: Loaded zone from YAML: %s (vnum %d)", filename.c_str(), vnum);
	return zone;
}

/**
 * п≈п╟пЁя─я┐п╥п╦я┌я▄ я┌я─п╦пЁпЁп╣я─ п╦п╥ YAML я└п╟п╧п╩п╟.
 */
Trigger* LoadTrigger(const std::string& filename, const ConstantsMapper& mapper) {
	(void)mapper; // п÷п╬п╨п╟ п╫п╣ п╦я│п©п╬п╩я▄п╥я┐п╣я┌я│я▐

	YAML::Node doc = LoadYamlFile(filename);
	if (!doc) {
		return nullptr;
	}

	// п÷п╬п╩я┐я┤п╦я┌я▄ vnum
	TrgVnum vnum = GetYamlValue<int>(doc, "vnum", -1);
	if (vnum < 0) {
		log("SYSERR: Invalid vnum in YAML trigger file '%s'", filename.c_str());
		return nullptr;
	}

	// п÷п╬п╩я┐я┤п╦я┌я▄ п╠п╟п╥п╬п╡я▀п╣ п©п╬п╩я▐
	std::string name = GetYamlValue<std::string>(doc, "name", "undefined");
	int attach_type = GetYamlValue<int>(doc, "attach_type", 0); // 0=MOB, 1=OBJ, 2=WORLD

	// п╒п╦п© я┌я─п╦пЁпЁп╣я─п╟ - bitvector
	long trigger_type = 0;
	std::vector<std::string> trigger_types = GetYamlStringList(doc, "trigger_types");
	if (!trigger_types.empty()) {
		// п÷я─п╣п╬п╠я─п╟п╥п╬п╡п╟я┌я▄ п╦п╪п╣п╫п╟ я┌п╦п©п╬п╡ п╡ bitvector
		// п╒п╦п©я▀ я┌я─п╦пЁпЁп╣я─п╬п╡ п╥п╟п╡п╦я│я▐я┌ п╬я┌ attach_type
		for (const auto& type_name : trigger_types) {
			// п÷я─п╬я│я┌п╬п╣ я│п╬п©п╬я│я┌п╟п╡п╩п╣п╫п╦п╣ п╢п╩я▐ п╠п╟п╥п╬п╡я▀я┘ я┌п╦п©п╬п╡
			// п▓ я─п╣п╟п╩я▄п╫п╬п╧ я─п╣п╟п╩п╦п╥п╟я├п╦п╦ п╫я┐п╤п╫п╬ п╦я│п©п╬п╩я▄п╥п╬п╡п╟я┌я▄ п╪п╟я│я│п╦п╡я▀ п╨п╬п╫я│я┌п╟п╫я┌
			if (type_name == "kCommand" || type_name == "COMMAND") {
				trigger_type |= (1 << 1); // MTRIG_COMMAND
			} else if (type_name == "kGreet" || type_name == "GREET") {
				trigger_type |= (1 << 0); // MTRIG_GREET
			} else if (type_name == "kEntry" || type_name == "ENTRY") {
				trigger_type |= (1 << 2); // MTRIG_ENTRY
			} else if (type_name == "kRandom" || type_name == "RANDOM") {
				trigger_type |= (1 << 3); // MTRIG_RANDOM
			} else if (type_name == "kSpeech" || type_name == "SPEECH") {
				trigger_type |= (1 << 4); // MTRIG_SPEECH
			} else if (type_name == "kAct" || type_name == "ACT") {
				trigger_type |= (1 << 5); // MTRIG_ACT
			} else if (type_name == "kDeath" || type_name == "DEATH") {
				trigger_type |= (1 << 6); // MTRIG_DEATH
			} else if (type_name == "kFight" || type_name == "FIGHT") {
				trigger_type |= (1 << 7); // MTRIG_FIGHT
			} else if (type_name == "kHitprcnt" || type_name == "HITPRCNT") {
				trigger_type |= (1 << 8); // MTRIG_HITPRCNT
			} else if (type_name == "kBribe" || type_name == "BRIBE") {
				trigger_type |= (1 << 9); // MTRIG_BRIBE
			} else if (type_name == "kLoad" || type_name == "LOAD") {
				trigger_type |= (1 << 10); // MTRIG_LOAD
			} else if (type_name == "kMemory" || type_name == "MEMORY") {
				trigger_type |= (1 << 11); // MTRIG_MEMORY
			} else if (type_name == "kCast" || type_name == "CAST") {
				trigger_type |= (1 << 12); // MTRIG_CAST
			} else if (type_name == "kLeave" || type_name == "LEAVE") {
				trigger_type |= (1 << 13); // MTRIG_LEAVE
			} else if (type_name == "kDoor" || type_name == "DOOR") {
				trigger_type |= (1 << 14); // MTRIG_DOOR
			} else if (type_name == "kTime" || type_name == "TIME") {
				trigger_type |= (1 << 15); // MTRIG_TIME
			} else if (type_name == "kIncome" || type_name == "INCOME") {
				trigger_type |= (1 << 16); // MTRIG_INCOME
			} else if (type_name == "kStart" || type_name == "START") {
				trigger_type |= (1 << 17); // MTRIG_START
			} else if (type_name == "kInOut" || type_name == "INOUT") {
				trigger_type |= (1 << 18); // MTRIG_INOUT
			} else {
				// п÷п╬п©я─п╬п╠п╬п╡п╟я┌я▄ п╨п╟п╨ я┤п╦я│п╩п╬
				try {
					long type_num = std::stol(type_name);
					trigger_type |= type_num;
				} catch (...) {
					log("SYSERR: Unknown trigger type '%s' in trigger %d", type_name.c_str(), vnum);
				}
			}
		}
	} else {
		// п░п╩я▄я┌п╣я─п╫п╟я┌п╦п╡п╫п╬: п©я─п╬я┤п╦я┌п╟я┌я▄ п╨п╟п╨ я┤п╦я│п╩п╬
		trigger_type = GetYamlValue<int>(doc, "trigger_type", 0);
	}

	// п║п╬п╥п╢п╟я┌я▄ я┌я─п╦пЁпЁп╣я─
	Trigger* trig = new Trigger(vnum, name.c_str(), static_cast<byte>(attach_type), trigger_type);
	if (!trig) {
		log("SYSERR: Failed to allocate Trigger for '%s'", filename.c_str());
		return nullptr;
	}

	// п╖п╦я│п╩п╬п╡п╬п╧ п╟я─пЁя┐п╪п╣п╫я┌
	trig->narg = GetYamlValue<int>(doc, "narg", 100);

	// Add flag
	trig->add_flag = GetYamlValue<bool>(doc, "add_flag", false);

	// п░я─пЁя┐п╪п╣п╫я┌ (я│я┌я─п╬п╨п╟)
	trig->arglist = GetYamlValue<std::string>(doc, "arglist", "");

	// п║п╨я─п╦п©я┌ (п╪п╫п╬пЁп╬я│я┌я─п╬я┤п╫я▀п╧ я┌п╣п╨я│я┌)
	std::string script = GetYamlValue<std::string>(doc, "script", "");
	if (!script.empty()) {
		// п═п╟п╥п╠п╦я┌я▄ я│п╨я─п╦п©я┌ п╫п╟ я│я┌я─п╬п╨п╦ п╦ я│п╬п╥п╢п╟я┌я▄ cmdlist
		// п╜я┌п╬ я┐п©я─п╬я┴я▒п╫п╫п╟я▐ я─п╣п╟п╩п╦п╥п╟я├п╦я▐ - п╡ я─п╣п╟п╩я▄п╫п╬я│я┌п╦ п╫я┐п╤п╫п╬ п╦я│п©п╬п╩я▄п╥п╬п╡п╟я┌я▄ parse_trigger
		auto cmdlist = std::make_shared<cmdlist_element::shared_ptr>();
		cmdlist_element::shared_ptr current = nullptr;
		cmdlist_element::shared_ptr head = nullptr;

		std::istringstream stream(script);
		std::string line;
		while (std::getline(stream, line)) {
			auto elem = std::make_shared<cmdlist_element>();
			elem->cmd = str_dup(line.c_str());
			elem->next = nullptr;

			if (!head) {
				head = elem;
				current = elem;
			} else {
				current->next = elem;
				current = elem;
			}
		}

		if (head) {
			*cmdlist = head;
			trig->cmdlist = cmdlist;
		}
	}

	log("SYSLOG: Loaded trigger from YAML: %s (vnum %d)", filename.c_str(), vnum);
	return trig;
}

/**
 * п≈п╟пЁя─я┐п╥п╦я┌я▄ help entry п╦п╥ YAML я└п╟п╧п╩п╟.
 * TODO: п÷п╬п╩п╫п╟я▐ я─п╣п╟п╩п╦п╥п╟я├п╦я▐
 */
bool LoadHelp(const std::string& filename, const ConstantsMapper& mapper) {
	YAML::Node doc = LoadYamlFile(filename);
	if (!doc) {
		return false;
	}

	log("SYSWARN: YamlLoader::LoadHelp() not yet implemented for '%s'", filename.c_str());
	// TODO: п═п╣п╟п╩п╦п╥п╟я├п╦я▐ п©п╟я─я│п╦п╫пЁп╟ help
	return false;
}

/**
 * п≈п╟пЁя─я┐п╥п╦я┌я▄ social п╦п╥ YAML я└п╟п╧п╩п╟.
 * TODO: п÷п╬п╩п╫п╟я▐ я─п╣п╟п╩п╦п╥п╟я├п╦я▐
 */
bool LoadSocial(const std::string& filename, const ConstantsMapper& mapper) {
	YAML::Node doc = LoadYamlFile(filename);
	if (!doc) {
		return false;
	}

	log("SYSWARN: YamlLoader::LoadSocial() not yet implemented for '%s'", filename.c_str());
	// TODO: п═п╣п╟п╩п╦п╥п╟я├п╦я▐ п©п╟я─я│п╦п╫пЁп╟ socials
	return false;
}

} // namespace YamlLoader

// vim: ts=4 sw=4 tw=0 noet syntax=cpp :

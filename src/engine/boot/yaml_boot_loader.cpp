/**
 \file yaml_boot_loader.cpp
 \authors Created by Claude Code
 \date 2026.01.12
 \brief О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ YAML О©╫О©╫О©╫О©╫О©╫О©╫.
*/

#include "yaml_boot_loader.h"
#include "yaml_loaders.h"
#include "engine/db/db.h"
#include "engine/db/global_objects.h"
#include "engine/db/obj_prototypes.h"
#include "utils/logger.h"

#include <thread>
#include <algorithm>
#include <filesystem>

// External globals
extern CharData *mob_proto;
extern IndexData *mob_index;
extern int top_of_mobt;
extern Rooms& world;
extern RoomRnum top_of_world;
extern IndexData **trig_index;
extern int top_of_trigt;

namespace YamlBoot {

// ==================== YamlIndex ====================

bool YamlIndex::Load(const std::string& index_path) {
	try {
		YAML::Node doc = YAML::LoadFile(index_path);
		if (!doc) {
			log("SYSERR: Failed to load YAML index: %s", index_path.c_str());
			return false;
		}

		// О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫
		std::filesystem::path path(index_path);
		base_path = path.parent_path().string();
		if (!base_path.empty() && base_path.back() != '/') {
			base_path += '/';
		}

		// О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫
		if (doc["files"] && doc["files"].IsSequence()) {
			for (const auto& file_node : doc["files"]) {
				files.push_back(file_node.as<std::string>());
			}
		} else if (doc.IsSequence()) {
			// О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫: О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫
			for (const auto& file_node : doc) {
				files.push_back(file_node.as<std::string>());
			}
		} else {
			log("SYSERR: YAML index has no 'files' list: %s", index_path.c_str());
			return false;
		}

		log("Loaded YAML index: %s (%zu files)", index_path.c_str(), files.size());
		return true;
	} catch (const YAML::Exception& e) {
		log("SYSERR: YAML parse error in index %s: %s", index_path.c_str(), e.what());
		return false;
	}
}

// ==================== YamlIndexLoader ====================

std::string YamlIndexLoader::GetBasePath(EBootType mode) {
	switch (mode) {
		case DB_BOOT_WLD: return LIB_WORLD "wld/";
		case DB_BOOT_MOB: return LIB_WORLD "mob/";
		case DB_BOOT_OBJ: return LIB_WORLD "obj/";
		case DB_BOOT_ZON: return LIB_WORLD "zon/";
		case DB_BOOT_TRG: return LIB_WORLD "trg/";
		default: return "";
	}
}

std::string YamlIndexLoader::GetIndexPath(EBootType mode) {
	return GetBasePath(mode) + "index.yaml";
}

std::optional<YamlIndex> YamlIndexLoader::Load(EBootType mode) {
	std::string index_path = GetIndexPath(mode);

	// О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫, О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫ YAML index
	if (!std::filesystem::exists(index_path)) {
		return std::nullopt;
	}

	YamlIndex index;
	index.base_path = GetBasePath(mode);
	if (!index.Load(index_path)) {
		return std::nullopt;
	}

	return index;
}

// ==================== YamlParallelLoader ====================

YamlParallelLoader::YamlParallelLoader(size_t thread_count)
	: m_thread_count(thread_count) {
	if (m_thread_count == 0) {
		// О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫, О©╫О©╫О©╫О©╫ 1 О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫О©╫
		m_thread_count = 1;
	}
}

/**
 * О©╫О©╫О©╫О©╫О©╫О©╫ vnum О©╫О©╫ YAML О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫.
 */
int ExtractVnumFromYaml(const std::string& filepath) {
	try {
		YAML::Node doc = YAML::LoadFile(filepath);
		if (doc && doc["vnum"]) {
			return doc["vnum"].as<int>();
		}
	} catch (const YAML::Exception&) {
		// О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫
	}
	return -1;
}

template<typename T, typename LoadFunc>
std::vector<LoadedEntity<T>> YamlParallelLoader::LoadInParallel(
	const YamlIndex& index,
	LoadFunc load_func
) {
	std::vector<LoadedEntity<T>> results(index.files.size());
	std::mutex results_mutex;
	std::atomic<size_t> next_index{0};

	// О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫
	auto worker = [&]() {
		ConstantsMapper local_mapper; // О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫

		while (true) {
			size_t idx = next_index.fetch_add(1);
			if (idx >= index.files.size()) {
				break;
			}

			std::string filepath = index.base_path + index.files[idx];

			try {
				// О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ vnum О©╫О©╫ YAML
				int vnum = ExtractVnumFromYaml(filepath);
				if (vnum < 0) {
					std::lock_guard<std::mutex> lock(results_mutex);
					results[idx] = LoadedEntity<T>(-1, "No vnum in: " + filepath);
					continue;
				}

				auto entity = load_func(filepath, local_mapper);
				if (entity) {
					std::lock_guard<std::mutex> lock(results_mutex);
					results[idx] = LoadedEntity<T>(vnum, entity.release());
				} else {
					std::lock_guard<std::mutex> lock(results_mutex);
					results[idx] = LoadedEntity<T>(vnum, "Failed to load: " + filepath);
				}
			} catch (const std::exception& e) {
				std::lock_guard<std::mutex> lock(results_mutex);
				results[idx] = LoadedEntity<T>(-1, std::string("Exception: ") + e.what());
			}
		}
	};

	// О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫
	std::vector<std::thread> threads;
	for (size_t i = 0; i < m_thread_count; ++i) {
		threads.emplace_back(worker);
	}

	// О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫О©╫
	for (auto& t : threads) {
		t.join();
	}

	return results;
}

std::vector<LoadedEntity<CharData>> YamlParallelLoader::LoadMobs(const YamlIndex& index) {
	log("Loading %zu mobs in %zu threads...", index.size(), m_thread_count);

	auto load_func = [](const std::string& path, const ConstantsMapper& mapper)
		-> std::unique_ptr<CharData> {
		CharData* mob = YamlLoader::LoadMobile(path, mapper);
		return std::unique_ptr<CharData>(mob);
	};

	return LoadInParallel<CharData>(index, load_func);
}

std::vector<LoadedEntity<CObjectPrototype>> YamlParallelLoader::LoadObjects(const YamlIndex& index) {
	log("Loading %zu objects in %zu threads...", index.size(), m_thread_count);

	auto load_func = [](const std::string& path, const ConstantsMapper& mapper)
		-> std::unique_ptr<CObjectPrototype> {
		// LoadObject О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ ObjData*, О©╫О©╫ О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫
		// TODO: О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ LoadObject О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ CObjectPrototype
		ObjData* obj = YamlLoader::LoadObject(path, mapper);
		if (obj) {
			// О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫, О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫ ObjData
			auto proto = std::make_unique<CObjectPrototype>(obj->get_vnum());
			// TODO: О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫ obj О©╫ proto
			delete obj;
			return proto;
		}
		return nullptr;
	};

	return LoadInParallel<CObjectPrototype>(index, load_func);
}

std::vector<LoadedEntity<RoomData>> YamlParallelLoader::LoadRooms(const YamlIndex& index) {
	log("Loading %zu rooms in %zu threads...", index.size(), m_thread_count);

	auto load_func = [](const std::string& path, const ConstantsMapper& mapper)
		-> std::unique_ptr<RoomData> {
		RoomData* room = YamlLoader::LoadRoom(path, mapper);
		return std::unique_ptr<RoomData>(room);
	};

	return LoadInParallel<RoomData>(index, load_func);
}

std::vector<LoadedEntity<ZoneData>> YamlParallelLoader::LoadZones(const YamlIndex& index) {
	log("Loading %zu zones in %zu threads...", index.size(), m_thread_count);

	auto load_func = [](const std::string& path, const ConstantsMapper& mapper)
		-> std::unique_ptr<ZoneData> {
		ZoneData* zone = YamlLoader::LoadZone(path, mapper);
		return std::unique_ptr<ZoneData>(zone);
	};

	return LoadInParallel<ZoneData>(index, load_func);
}

std::vector<LoadedEntity<Trigger>> YamlParallelLoader::LoadTriggers(const YamlIndex& index) {
	log("Loading %zu triggers in %zu threads...", index.size(), m_thread_count);

	auto load_func = [](const std::string& path, const ConstantsMapper& mapper)
		-> std::unique_ptr<Trigger> {
		Trigger* trig = YamlLoader::LoadTrigger(path, mapper);
		return std::unique_ptr<Trigger>(trig);
	};

	return LoadInParallel<Trigger>(index, load_func);
}

// ==================== YamlWorldBuilder ====================

bool YamlWorldBuilder::AddMobs(std::vector<LoadedEntity<CharData>>& mobs) {
	// О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫
	size_t success_count = 0;
	for (const auto& entity : mobs) {
		if (entity.success) {
			++success_count;
		}
	}

	if (success_count == 0) {
		log("SYSERR: No mobs loaded successfully");
		return false;
	}

	// О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫ mob_proto
	mob_proto = new CharData[success_count];
	CREATE(mob_index, success_count);
	top_of_mobt = static_cast<int>(success_count) - 1;

	// О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫
	std::sort(mobs.begin(), mobs.end(), [](const auto& a, const auto& b) {
		return a.vnum < b.vnum;
	});

	// О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫ О©╫ О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫
	int rnum = 0;
	for (auto& entity : mobs) {
		if (!entity.success || !entity.data) {
			if (!entity.error.empty()) {
				log("SYSERR: Mob load error: %s", entity.error.c_str());
			}
			continue;
		}

		// О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫ О©╫ mob_proto
		mob_proto[rnum] = std::move(*entity.data);
		mob_proto[rnum].set_rnum(rnum);

		// О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ mob_index
		mob_index[rnum].vnum = entity.vnum;
		mob_index[rnum].total_online = 0;
		mob_index[rnum].func = nullptr;

		++rnum;
	}

	log("Added %d mobs to world", rnum);
	return true;
}

bool YamlWorldBuilder::AddObjects(std::vector<LoadedEntity<CObjectPrototype>>& objects) {
	size_t success_count = 0;
	for (const auto& entity : objects) {
		if (entity.success) {
			++success_count;
		}
	}

	if (success_count == 0) {
		log("SYSERR: No objects loaded successfully");
		return false;
	}

	// О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫
	std::sort(objects.begin(), objects.end(), [](const auto& a, const auto& b) {
		return a.vnum < b.vnum;
	});

	// О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫ obj_proto
	for (auto& entity : objects) {
		if (!entity.success || !entity.data) {
			if (!entity.error.empty()) {
				log("SYSERR: Object load error: %s", entity.error.c_str());
			}
			continue;
		}

		// О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫ obj_proto.add()
		obj_proto.add(entity.data.release(), entity.vnum);
	}

	log("Added %zu objects to world", success_count);
	return true;
}

bool YamlWorldBuilder::AddRooms(std::vector<LoadedEntity<RoomData>>& rooms) {
	size_t success_count = 0;
	for (const auto& entity : rooms) {
		if (entity.success) {
			++success_count;
		}
	}

	if (success_count == 0) {
		log("SYSERR: No rooms loaded successfully");
		return false;
	}

	// О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫О©╫ kNowhere (О©╫О©╫О©╫О©╫О©╫О©╫ 0) - О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫
	world.push_back(new RoomData);
	top_of_world = kNowhere;

	// О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫
	std::sort(rooms.begin(), rooms.end(), [](const auto& a, const auto& b) {
		return a.vnum < b.vnum;
	});

	// О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫ world
	for (auto& entity : rooms) {
		if (!entity.success || !entity.data) {
			if (!entity.error.empty()) {
				log("SYSERR: Room load error: %s", entity.error.c_str());
			}
			continue;
		}

		// О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫ world
		RoomData* room = entity.data.release();
		world.push_back(room);
		++top_of_world;
		// rnum - О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫ О©╫О©╫О©╫О©╫О©╫О©╫О©╫ world, О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫
	}

	log("Added %d rooms to world", top_of_world + 1);
	return true;
}

bool YamlWorldBuilder::AddZones(std::vector<LoadedEntity<ZoneData>>& zones) {
	size_t success_count = 0;
	for (const auto& entity : zones) {
		if (entity.success) {
			++success_count;
		}
	}

	if (success_count == 0) {
		log("SYSERR: No zones loaded successfully");
		return false;
	}

	// О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫
	std::sort(zones.begin(), zones.end(), [](const auto& a, const auto& b) {
		return a.vnum < b.vnum;
	});

	// О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫ О©╫ zone_table
	for (auto& entity : zones) {
		if (!entity.success || !entity.data) {
			if (!entity.error.empty()) {
				log("SYSERR: Zone load error: %s", entity.error.c_str());
			}
			continue;
		}

		// TODO: О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫ О©╫ zone_table
		// zone_table.push_back(std::move(*entity.data));
	}

	log("Added %zu zones to world", success_count);
	return true;
}

bool YamlWorldBuilder::AddTriggers(std::vector<LoadedEntity<Trigger>>& triggers) {
	size_t success_count = 0;
	for (const auto& entity : triggers) {
		if (entity.success) {
			++success_count;
		}
	}

	if (success_count == 0) {
		log("SYSERR: No triggers loaded successfully");
		return false;
	}

	// О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫ trig_index
	CREATE(trig_index, success_count);

	// О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫
	std::sort(triggers.begin(), triggers.end(), [](const auto& a, const auto& b) {
		return a.vnum < b.vnum;
	});

	// О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫ trig_index
	top_of_trigt = 0;
	for (auto& entity : triggers) {
		if (!entity.success || !entity.data) {
			if (!entity.error.empty()) {
				log("SYSERR: Trigger load error: %s", entity.error.c_str());
			}
			continue;
		}

		// О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫ IndexData
		IndexData* index;
		CREATE(index, 1);
		index->vnum = entity.vnum;
		index->total_online = 0;
		index->func = nullptr;
		index->proto = entity.data.release();

		trig_index[top_of_trigt++] = index;
	}

	log("Added %d triggers to world", top_of_trigt);
	return true;
}

// ==================== YamlBootLoader ====================

bool YamlBootLoader::HasYamlIndex(EBootType mode) {
	std::string index_path = YamlIndexLoader::GetIndexPath(mode);
	return std::filesystem::exists(index_path);
}

bool YamlBootLoader::Boot(EBootType mode) {
	// О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ YAML index
	auto index_opt = YamlIndexLoader::Load(mode);
	if (!index_opt) {
		log("SYSERR: Failed to load YAML index for mode %d", static_cast<int>(mode));
		return false;
	}

	const YamlIndex& index = *index_opt;
	if (index.size() == 0) {
		log("SYSWARN: Empty YAML index for mode %d", static_cast<int>(mode));
		return true;
	}

	// О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫
	YamlParallelLoader loader;

	// О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫ О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫ О©╫О©╫О©╫О©╫
	switch (mode) {
		case DB_BOOT_MOB: {
			auto mobs = loader.LoadMobs(index);
			return YamlWorldBuilder::AddMobs(mobs);
		}

		case DB_BOOT_OBJ: {
			auto objects = loader.LoadObjects(index);
			return YamlWorldBuilder::AddObjects(objects);
		}

		case DB_BOOT_WLD: {
			auto rooms = loader.LoadRooms(index);
			return YamlWorldBuilder::AddRooms(rooms);
		}

		case DB_BOOT_ZON: {
			auto zones = loader.LoadZones(index);
			return YamlWorldBuilder::AddZones(zones);
		}

		case DB_BOOT_TRG: {
			auto triggers = loader.LoadTriggers(index);
			return YamlWorldBuilder::AddTriggers(triggers);
		}

		default:
			log("SYSERR: Unsupported boot mode for YAML: %d", static_cast<int>(mode));
			return false;
	}
}

bool YamlBootLoader::BootAll() {
	bool success = true;

	// О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫ (О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫)
	if (HasYamlIndex(DB_BOOT_ZON)) {
		log("Booting zones from YAML...");
		success &= Boot(DB_BOOT_ZON);
	}

	// О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫О©╫О©╫
	if (HasYamlIndex(DB_BOOT_TRG)) {
		log("Booting triggers from YAML...");
		success &= Boot(DB_BOOT_TRG);
	}

	// О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫
	if (HasYamlIndex(DB_BOOT_WLD)) {
		log("Booting rooms from YAML...");
		success &= Boot(DB_BOOT_WLD);
	}

	// О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫
	if (HasYamlIndex(DB_BOOT_MOB)) {
		log("Booting mobs from YAML...");
		success &= Boot(DB_BOOT_MOB);
	}

	// О©╫О©╫О©╫О©╫О©╫ О©╫О©╫О©╫О©╫О©╫О©╫О©╫
	if (HasYamlIndex(DB_BOOT_OBJ)) {
		log("Booting objects from YAML...");
		success &= Boot(DB_BOOT_OBJ);
	}

	return success;
}

} // namespace YamlBoot

// vim: ts=4 sw=4 tw=0 noet syntax=cpp :

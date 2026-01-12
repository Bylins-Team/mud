/**
 \file yaml_data_files.h
 \authors Created by Claude Code
 \date 2026.01.12
 \brief п·п╠я▒я─я┌п╨п╦ YAML п╩п╬п╟п╢п╣я─п╬п╡ п╢п╩я▐ п╦п╫я┌п╣пЁя─п╟я├п╦п╦ я│ я│п╦я│я┌п╣п╪п╬п╧ п╥п╟пЁя─я┐п╥п╨п╦.
 \details п═п╣п╟п╩п╦п╥я┐я▌я┌ п╦п╫я┌п╣я─я└п╣п╧я│ BaseDataFile, п╡я▀п╥я▀п╡п╟я▌я┌ YamlLoader п╦п╥п╫я┐я┌я─п╦.
*/

#ifndef BYLINS_SRC_ENGINE_BOOT_YAML_DATA_FILES_H_
#define BYLINS_SRC_ENGINE_BOOT_YAML_DATA_FILES_H_

#include "boot_data_files.h"
#include "constants_mapper.h"
#include "yaml_loaders.h"

/**
 * п▒п╟п╥п╬п╡я▀п╧ п╨п╩п╟я│я│ п╢п╩я▐ YAML я└п╟п╧п╩п╬п╡.
 * п═п╣п╟п╩п╦п╥я┐п╣я┌ п╦п╫я┌п╣я─я└п╣п╧я│ BaseDataFile, п╢п╣п╩п╣пЁп╦я─я┐я▐ я─п╟п╠п╬я┌я┐ YamlLoader.
 */
class YamlDataFile : public BaseDataFile {
 public:
	explicit YamlDataFile(const std::string& filename)
		: m_filename(filename), m_loaded(false) {}

	virtual ~YamlDataFile() = default;

	virtual bool open() override { return true; } // YAML п╫п╣ я┌я─п╣п╠я┐п╣я┌ п╬я┌п╨я─я▀я┌п╦я▐
	virtual bool load() override = 0; // п═п╣п╟п╩п╦п╥я┐п╣я┌я│я▐ п╡ п©п╬п╢п╨п╩п╟я│я│п╟я┘
	virtual void close() override {} // YAML п╫п╣ я┌я─п╣п╠я┐п╣я┌ п╥п╟п╨я─я▀я┌п╦я▐
	virtual std::string full_file_name() const override { return m_filename; }

 protected:
	std::string m_filename;
	bool m_loaded;
	ConstantsMapper m_mapper;
};

/**
 * YAML п╩п╬п╟п╢п╣я─ п╢п╩я▐ п╪п╬п╠п╬п╡.
 */
class YamlMobileFile : public YamlDataFile {
 public:
	explicit YamlMobileFile(const std::string& filename)
		: YamlDataFile(filename) {}

	virtual bool load() override;

	static shared_ptr create(const std::string& filename) {
		return std::make_shared<YamlMobileFile>(filename);
	}
};

/**
 * YAML п╩п╬п╟п╢п╣я─ п╢п╩я▐ п╬п╠я┼п╣п╨я┌п╬п╡.
 */
class YamlObjectFile : public YamlDataFile {
 public:
	explicit YamlObjectFile(const std::string& filename)
		: YamlDataFile(filename) {}

	virtual bool load() override;

	static shared_ptr create(const std::string& filename) {
		return std::make_shared<YamlObjectFile>(filename);
	}
};

/**
 * YAML п╩п╬п╟п╢п╣я─ п╢п╩я▐ п╨п╬п╪п╫п╟я┌.
 */
class YamlWorldFile : public YamlDataFile {
 public:
	explicit YamlWorldFile(const std::string& filename)
		: YamlDataFile(filename) {}

	virtual bool load() override;

	static shared_ptr create(const std::string& filename) {
		return std::make_shared<YamlWorldFile>(filename);
	}
};

/**
 * YAML п╩п╬п╟п╢п╣я─ п╢п╩я▐ п╥п╬п╫.
 */
class YamlZoneFile : public YamlDataFile {
 public:
	explicit YamlZoneFile(const std::string& filename)
		: YamlDataFile(filename) {}

	virtual bool load() override;

	static shared_ptr create(const std::string& filename) {
		return std::make_shared<YamlZoneFile>(filename);
	}
};

/**
 * YAML п╩п╬п╟п╢п╣я─ п╢п╩я▐ я┌я─п╦пЁпЁп╣я─п╬п╡.
 */
class YamlTriggersFile : public YamlDataFile {
 public:
	explicit YamlTriggersFile(const std::string& filename)
		: YamlDataFile(filename) {}

	virtual bool load() override;

	static shared_ptr create(const std::string& filename) {
		return std::make_shared<YamlTriggersFile>(filename);
	}
};

/**
 * YAML п╩п╬п╟п╢п╣я─ п╢п╩я▐ help я└п╟п╧п╩п╬п╡.
 */
class YamlHelpFile : public YamlDataFile {
 public:
	explicit YamlHelpFile(const std::string& filename)
		: YamlDataFile(filename) {}

	virtual bool load() override;

	static shared_ptr create(const std::string& filename) {
		return std::make_shared<YamlHelpFile>(filename);
	}
};

/**
 * YAML п╩п╬п╟п╢п╣я─ п╢п╩я▐ socials.
 */
class YamlSocialsFile : public YamlDataFile {
 public:
	explicit YamlSocialsFile(const std::string& filename)
		: YamlDataFile(filename) {}

	virtual bool load() override;

	static shared_ptr create(const std::string& filename) {
		return std::make_shared<YamlSocialsFile>(filename);
	}
};

#endif //BYLINS_SRC_ENGINE_BOOT_YAML_DATA_FILES_H_

// vim: ts=4 sw=4 tw=0 noet syntax=cpp :

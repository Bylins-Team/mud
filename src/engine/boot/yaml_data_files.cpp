/**
 \file yaml_data_files.cpp
 \authors Created by Claude Code
 \date 2026.01.12
 \brief п═п╣п╟п╩п╦п╥п╟я├п╦я▐ п╬п╠я▒я─я┌п╬п╨ YAML п╩п╬п╟п╢п╣я─п╬п╡.
*/

#include "yaml_data_files.h"
#include "utils/logger.h"

/**
 * п≈п╟пЁя─я┐п╥п╨п╟ п╪п╬п╠п╬п╡ п╦п╥ YAML.
 */
bool YamlMobileFile::load() {
	if (m_loaded) {
		return true;
	}

	// п▓я▀п╥я▀п╡п╟п╣п╪ YamlLoader
	CharData* mob = YamlLoader::LoadMobile(m_filename, m_mapper);
	if (!mob) {
		log("SYSERR: Failed to load mobile from YAML: %s", m_filename.c_str());
		return false;
	}

	// TODO: п■п╬п╠п╟п╡п╦я┌я▄ п╪п╬п╠п╟ п╡ пЁп╩п╬п╠п╟п╩я▄п╫я▀п╧ п╪п╟я│я│п╦п╡
	m_loaded = true;
	return true;
}

/**
 * п≈п╟пЁя─я┐п╥п╨п╟ п╬п╠я┼п╣п╨я┌п╬п╡ п╦п╥ YAML.
 */
bool YamlObjectFile::load() {
	if (m_loaded) {
		return true;
	}

	ObjData* obj = YamlLoader::LoadObject(m_filename, m_mapper);
	if (!obj) {
		log("SYSERR: Failed to load object from YAML: %s", m_filename.c_str());
		return false;
	}

	// TODO: п■п╬п╠п╟п╡п╦я┌я▄ п╬п╠я┼п╣п╨я┌ п╡ пЁп╩п╬п╠п╟п╩я▄п╫я▀п╧ п╪п╟я│я│п╦п╡
	m_loaded = true;
	return true;
}

/**
 * п≈п╟пЁя─я┐п╥п╨п╟ п╨п╬п╪п╫п╟я┌ п╦п╥ YAML.
 */
bool YamlWorldFile::load() {
	if (m_loaded) {
		return true;
	}

	RoomData* room = YamlLoader::LoadRoom(m_filename, m_mapper);
	if (!room) {
		log("SYSERR: Failed to load room from YAML: %s", m_filename.c_str());
		return false;
	}

	// TODO: п■п╬п╠п╟п╡п╦я┌я▄ п╨п╬п╪п╫п╟я┌я┐ п╡ пЁп╩п╬п╠п╟п╩я▄п╫я▀п╧ п╪п╟я│я│п╦п╡
	m_loaded = true;
	return true;
}

/**
 * п≈п╟пЁя─я┐п╥п╨п╟ п╥п╬п╫ п╦п╥ YAML.
 */
bool YamlZoneFile::load() {
	if (m_loaded) {
		return true;
	}

	ZoneData* zone = YamlLoader::LoadZone(m_filename, m_mapper);
	if (!zone) {
		log("SYSERR: Failed to load zone from YAML: %s", m_filename.c_str());
		return false;
	}

	// TODO: п■п╬п╠п╟п╡п╦я┌я▄ п╥п╬п╫я┐ п╡ пЁп╩п╬п╠п╟п╩я▄п╫я▀п╧ п╪п╟я│я│п╦п╡
	m_loaded = true;
	return true;
}

/**
 * п≈п╟пЁя─я┐п╥п╨п╟ я┌я─п╦пЁпЁп╣я─п╬п╡ п╦п╥ YAML.
 */
bool YamlTriggersFile::load() {
	if (m_loaded) {
		return true;
	}

	Trigger* trig = YamlLoader::LoadTrigger(m_filename, m_mapper);
	if (!trig) {
		log("SYSERR: Failed to load trigger from YAML: %s", m_filename.c_str());
		return false;
	}

	// TODO: п■п╬п╠п╟п╡п╦я┌я▄ я┌я─п╦пЁпЁп╣я─ п╡ пЁп╩п╬п╠п╟п╩я▄п╫я▀п╧ п╪п╟я│я│п╦п╡
	m_loaded = true;
	return true;
}

/**
 * п≈п╟пЁя─я┐п╥п╨п╟ help п╦п╥ YAML.
 */
bool YamlHelpFile::load() {
	if (m_loaded) {
		return true;
	}

	bool success = YamlLoader::LoadHelp(m_filename, m_mapper);
	if (!success) {
		log("SYSERR: Failed to load help from YAML: %s", m_filename.c_str());
		return false;
	}

	m_loaded = true;
	return true;
}

/**
 * п≈п╟пЁя─я┐п╥п╨п╟ socials п╦п╥ YAML.
 */
bool YamlSocialsFile::load() {
	if (m_loaded) {
		return true;
	}

	bool success = YamlLoader::LoadSocial(m_filename, m_mapper);
	if (!success) {
		log("SYSERR: Failed to load social from YAML: %s", m_filename.c_str());
		return false;
	}

	m_loaded = true;
	return true;
}

// vim: ts=4 sw=4 tw=0 noet syntax=cpp :

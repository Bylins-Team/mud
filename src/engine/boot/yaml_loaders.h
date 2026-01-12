/**
 \file yaml_loaders.h
 \authors Created by Claude Code
 \date 2026.01.12
 \brief пёп╫п╦я└п╦я├п╦я─п╬п╡п╟п╫п╫п╟я▐ я│п╦я│я┌п╣п╪п╟ п╥п╟пЁя─я┐п╥п╨п╦ п╪п╦я─п╟ п╦п╥ YAML я└п╟п╧п╩п╬п╡.
 \details п°п╦п╫п╦п╪п╟п╩п╦я│я┌п╦я┤п╫я▀п╧ я└я┐п╫п╨я├п╦п╬п╫п╟п╩я▄п╫я▀п╧ п©п╬п╢я┘п╬п╢ п╢п╩я▐ п╥п╟пЁя─я┐п╥п╨п╦ п╡я│п╣я┘ я┌п╦п©п╬п╡ я└п╟п╧п╩п╬п╡ п╪п╦я─п╟.
*/

#ifndef BYLINS_SRC_ENGINE_BOOT_YAML_LOADERS_H_
#define BYLINS_SRC_ENGINE_BOOT_YAML_LOADERS_H_

#include <string>
#include <yaml-cpp/yaml.h>
#include "constants_mapper.h"

// Forward declarations
class CharData;
class ObjData;
struct RoomData;
class ZoneData;
class Trigger;

/**
 * Namespace п╢п╩я▐ я┐п╫п╦я└п╦я├п╦я─п╬п╡п╟п╫п╫п╬п╧ п╥п╟пЁя─я┐п╥п╨п╦ п╪п╦я─п╟ п╦п╥ YAML.
 *
 * п▓я│п╣ я└я┐п╫п╨я├п╦п╦ я─п╟п╠п╬я┌п╟я▌я┌ п©п╬ п╣п╢п╦п╫п╬п╪я┐ п©я─п╦п╫я├п╦п©я┐:
 * - п·п╢п╦п╫ YAML я└п╟п╧п╩ = п╬п╢п╦п╫ entity
 * - п п╬п╢п╦я─п╬п╡п╨п╟ я└п╟п╧п╩п╬п╡: KOI8-R
 * - п≤п╪п╣п╫п╟ enum п╦п╥ п╨п╬п╢п╟, п╥п╫п╟я┤п╣п╫п╦я▐ - я─я┐я│я│п╨п╦п╣ я│я┌я─п╬п╨п╦
 * - п▓п╬п╥п╡я─п╟я┴п╟я▌я┌ я┐п╨п╟п╥п╟я┌п╣п╩я▄ п╫п╟ я│п╬п╥п╢п╟п╫п╫я▀п╧ п╬п╠я┼п╣п╨я┌ п╦п╩п╦ nullptr п©я─п╦ п╬я┬п╦п╠п╨п╣
 */
namespace YamlLoader {

/**
 * п≈п╟пЁя─я┐п╥п╦я┌я▄ п╪п╬п╠п╟ п╦п╥ YAML я└п╟п╧п╩п╟.
 * @param filename п÷я┐я┌я▄ п╨ я└п╟п╧п╩я┐ (п╫п╟п©я─п╦п╪п╣я─, lib/world/mob/100.yaml)
 * @param mapper п°п╟п©п©п╣я─ п╨п╬п╫я│я┌п╟п╫я┌ п╢п╩я▐ п©я─п╣п╬п╠я─п╟п╥п╬п╡п╟п╫п╦я▐ п╦п╪п╣п╫
 * @return пёп╨п╟п╥п╟я┌п╣п╩я▄ п╫п╟ CharData п╦п╩п╦ nullptr п©я─п╦ п╬я┬п╦п╠п╨п╣
 */
CharData* LoadMobile(const std::string& filename, const ConstantsMapper& mapper);

/**
 * п≈п╟пЁя─я┐п╥п╦я┌я▄ п╬п╠я┼п╣п╨я┌ п╦п╥ YAML я└п╟п╧п╩п╟.
 * @param filename п÷я┐я┌я▄ п╨ я└п╟п╧п╩я┐ (п╫п╟п©я─п╦п╪п╣я─, lib/world/obj/108.yaml)
 * @param mapper п°п╟п©п©п╣я─ п╨п╬п╫я│я┌п╟п╫я┌
 * @return пёп╨п╟п╥п╟я┌п╣п╩я▄ п╫п╟ ObjData п╦п╩п╦ nullptr п©я─п╦ п╬я┬п╦п╠п╨п╣
 */
ObjData* LoadObject(const std::string& filename, const ConstantsMapper& mapper);

/**
 * п≈п╟пЁя─я┐п╥п╦я┌я▄ п╨п╬п╪п╫п╟я┌я┐ п╦п╥ YAML я└п╟п╧п╩п╟.
 * @param filename п÷я┐я┌я▄ п╨ я└п╟п╧п╩я┐ (п╫п╟п©я─п╦п╪п╣я─, lib/world/wld/100.yaml)
 * @param mapper п°п╟п©п©п╣я─ п╨п╬п╫я│я┌п╟п╫я┌
 * @return пёп╨п╟п╥п╟я┌п╣п╩я▄ п╫п╟ RoomData п╦п╩п╦ nullptr п©я─п╦ п╬я┬п╦п╠п╨п╣
 */
RoomData* LoadRoom(const std::string& filename, const ConstantsMapper& mapper);

/**
 * п≈п╟пЁя─я┐п╥п╦я┌я▄ п╥п╬п╫я┐ п╦п╥ YAML я└п╟п╧п╩п╟.
 * @param filename п÷я┐я┌я▄ п╨ я└п╟п╧п╩я┐ (п╫п╟п©я─п╦п╪п╣я─, lib/world/zon/1.yaml)
 * @param mapper п°п╟п©п©п╣я─ п╨п╬п╫я│я┌п╟п╫я┌
 * @return пёп╨п╟п╥п╟я┌п╣п╩я▄ п╫п╟ ZoneData п╦п╩п╦ nullptr п©я─п╦ п╬я┬п╦п╠п╨п╣
 */
ZoneData* LoadZone(const std::string& filename, const ConstantsMapper& mapper);

/**
 * п≈п╟пЁя─я┐п╥п╦я┌я▄ я┌я─п╦пЁпЁп╣я─ п╦п╥ YAML я└п╟п╧п╩п╟.
 * @param filename п÷я┐я┌я▄ п╨ я└п╟п╧п╩я┐ (п╫п╟п©я─п╦п╪п╣я─, lib/world/trg/100.yaml)
 * @param mapper п°п╟п©п©п╣я─ п╨п╬п╫я│я┌п╟п╫я┌
 * @return пёп╨п╟п╥п╟я┌п╣п╩я▄ п╫п╟ Trigger п╦п╩п╦ nullptr п©я─п╦ п╬я┬п╦п╠п╨п╣
 */
Trigger* LoadTrigger(const std::string& filename, const ConstantsMapper& mapper);

/**
 * п≈п╟пЁя─я┐п╥п╦я┌я▄ help entry п╦п╥ YAML я└п╟п╧п╩п╟.
 * @param filename п÷я┐я┌я▄ п╨ я└п╟п╧п╩я┐
 * @param mapper п°п╟п©п©п╣я─ п╨п╬п╫я│я┌п╟п╫я┌
 * @return true п╣я│п╩п╦ я┐я│п©п╣я┬п╫п╬
 */
bool LoadHelp(const std::string& filename, const ConstantsMapper& mapper);

/**
 * п≈п╟пЁя─я┐п╥п╦я┌я▄ social п╦п╥ YAML я└п╟п╧п╩п╟.
 * @param filename п÷я┐я┌я▄ п╨ я└п╟п╧п╩я┐
 * @param mapper п°п╟п©п©п╣я─ п╨п╬п╫я│я┌п╟п╫я┌
 * @return true п╣я│п╩п╦ я┐я│п©п╣я┬п╫п╬
 */
bool LoadSocial(const std::string& filename, const ConstantsMapper& mapper);

/**
 * п▓я│п©п╬п╪п╬пЁп╟я┌п╣п╩я▄п╫п╟я▐ я└я┐п╫п╨я├п╦я▐: п╥п╟пЁя─я┐п╥п╦я┌я▄ YAML п╦п╥ я└п╟п╧п╩п╟ я│ п╬п╠я─п╟п╠п╬я┌п╨п╬п╧ п╬я┬п╦п╠п╬п╨.
 * @param filename п÷я┐я┌я▄ п╨ я└п╟п╧п╩я┐
 * @return YAML::Node п╦п╩п╦ п©я┐я│я┌п╬п╧ я┐п╥п╣п╩ п©я─п╦ п╬я┬п╦п╠п╨п╣
 */
YAML::Node LoadYamlFile(const std::string& filename);

/**
 * п▓я│п©п╬п╪п╬пЁп╟я┌п╣п╩я▄п╫п╟я▐ я└я┐п╫п╨я├п╦я▐: п╠п╣п╥п╬п©п╟я│п╫п╬п╣ п©п╬п╩я┐я┤п╣п╫п╦п╣ п╥п╫п╟я┤п╣п╫п╦я▐ п╦п╥ YAML я┐п╥п╩п╟.
 * @param node YAML я┐п╥п╣п╩
 * @param key п п╩я▌я┤
 * @param default_value п≈п╫п╟я┤п╣п╫п╦п╣ п©п╬ я┐п╪п╬п╩я┤п╟п╫п╦я▌
 * @return п≈п╫п╟я┤п╣п╫п╦п╣ п╦п╩п╦ default_value п╣я│п╩п╦ п╨п╩я▌я┤ п╫п╣ п╫п╟п╧п╢п╣п╫
 */
template<typename T>
T GetYamlValue(const YAML::Node& node, const std::string& key, const T& default_value);

/**
 * п▓я│п©п╬п╪п╬пЁп╟я┌п╣п╩я▄п╫п╟я▐ я└я┐п╫п╨я├п╦я▐: п©п╬п╩я┐я┤п╦я┌я▄ я│п©п╦я│п╬п╨ я│я┌я─п╬п╨ п╦п╥ YAML.
 * @param node YAML я┐п╥п╣п╩
 * @param key п п╩я▌я┤
 * @return п▓п╣п╨я┌п╬я─ я│я┌я─п╬п╨ (п©я┐я│я┌п╬п╧ п╣я│п╩п╦ п╨п╩я▌я┤ п╫п╣ п╫п╟п╧п╢п╣п╫)
 */
std::vector<std::string> GetYamlStringList(const YAML::Node& node, const std::string& key);

} // namespace YamlLoader

#endif //BYLINS_SRC_ENGINE_BOOT_YAML_LOADERS_H_

// vim: ts=4 sw=4 tw=0 noet syntax=cpp :

/**
 \file constants_mapper.h
 \authors Created by Claude Code
 \date 2026.01.12
 \brief пёп╫п╦п╡п╣я─я│п╟п╩я▄п╫я▀п╧ п╪п╟п©п©п╣я─ п╢п╩я▐ п©я─п╣п╬п╠я─п╟п╥п╬п╡п╟п╫п╦я▐ п╦п╪п╣п╫ п╨п╬п╫я│я┌п╟п╫я┌ п╡ п╥п╫п╟я┤п╣п╫п╦я▐ п╦ п╬п╠я─п╟я┌п╫п╬.
 \details п≤я│п©п╬п╩я▄п╥я┐п╣я┌я│я▐ п╢п╩я▐ п©п╟я─я│п╦п╫пЁп╟ YAML я└п╟п╧п╩п╬п╡ п╪п╦я─п╟, пЁп╢п╣ я└п╩п╟пЁп╦ п╦ enum'я▀ п©я─п╣п╢я│я┌п╟п╡п╩п╣п╫я▀
          я┤п╣п╩п╬п╡п╣п╨п╬я┤п╦я┌п╟п╣п╪я▀п╪п╦ п╦п╪п╣п╫п╟п╪п╦ п╡п╪п╣я│я┌п╬ п╪п╟пЁп╦я┤п╣я│п╨п╦я┘ я┤п╦я│п╣п╩.
*/

#ifndef BYLINS_SRC_ENGINE_BOOT_CONSTANTS_MAPPER_H_
#define BYLINS_SRC_ENGINE_BOOT_CONSTANTS_MAPPER_H_

#include "engine/structs/structs.h"
#include <string>
#include <vector>
#include <unordered_map>

/**
 * пёп╫п╦п╡п╣я─я│п╟п╩я▄п╫я▀п╧ п╪п╟п©п©п╣я─ п╨п╬п╫я│я┌п╟п╫я┌ п╢п╩я▐ YAML я└п╬я─п╪п╟я┌п╟.
 *
 * п÷я─п╣п╬п╠я─п╟п╥я┐п╣я┌:
 * - п≤п╪п╣п╫п╟ enum Б├▓ п╥п╫п╟я┤п╣п╫п╦я▐ (п╢п╩я▐ п©п╟я─я│п╦п╫пЁп╟ YAML)
 * - п≈п╫п╟я┤п╣п╫п╦я▐ Б├▓ п╦п╪п╣п╫п╟ enum (п╢п╩я▐ пЁп╣п╫п╣я─п╟я├п╦п╦ YAML)
 * - ASCII я└п╩п╟пЁп╦ Б├▓ bitvector (п╢п╩я▐ я│я┌п╟я─п╬пЁп╬ я└п╬я─п╪п╟я┌п╟, п╣я│п╩п╦ п╫я┐п╤п╫п╬)
 * - Bitvector Б├▓ я│п©п╦я│п╬п╨ п╦п╪п╣п╫ (п╢п╩я▐ пЁп╣п╫п╣я─п╟я├п╦п╦ YAML)
 * - п║п©п╦я│п╬п╨ п╦п╪п╣п╫ Б├▓ bitvector (п╢п╩я▐ п©п╟я─я│п╦п╫пЁп╟ YAML)
 */
class ConstantsMapper {
 public:
	ConstantsMapper() = default;

	/**
	 * п²п╟п╧я┌п╦ п╦п╫п╢п╣п╨я│ п╦п╪п╣п╫п╦ п╡ п╪п╟я│я│п╦п╡п╣ я│я┌я─п╬п╨.
	 * @param name п≤п╪я▐ п╢п╩я▐ п©п╬п╦я│п╨п╟ (п╫п╟п©я─п╦п╪п╣я─, "SENTINEL", "kMale")
	 * @param array п°п╟я│я│п╦п╡ я│я┌я─п╬п╨ (п╫п╟п©я─п╦п╪п╣я─, action_bits[], genders[])
	 * @return п≤п╫п╢п╣п╨я│ п╡ п╪п╟я│я│п╦п╡п╣, п╦п╩п╦ -1 п╣я│п╩п╦ п╫п╣ п╫п╟п╧п╢п╣п╫п╬
	 */
	int FindNameIndex(const std::string& name, const char** array) const;

	/**
	 * п÷я─п╣п╬п╠я─п╟п╥п╬п╡п╟я┌я▄ я│п©п╦я│п╬п╨ п╦п╪п╣п╫ я└п╩п╟пЁп╬п╡ п╡ bitvector.
	 * @param names п║п©п╦я│п╬п╨ п╦п╪п╣п╫ (п╫п╟п©я─п╦п╪п╣я─, {"SENTINEL", "SCAVENGER"})
	 * @param array п°п╟я│я│п╦п╡ я│я┌я─п╬п╨ я│ п╦п╪п╣п╫п╟п╪п╦ я└п╩п╟пЁп╬п╡ (action_bits[], room_bits[], etc.)
	 * @param plane_count п п╬п╩п╦я┤п╣я│я┌п╡п╬ п©п╩п╬я│п╨п╬я│я┌п╣п╧ (1-4)
	 * @return Bitvector я│ я┐я│я┌п╟п╫п╬п╡п╩п╣п╫п╫я▀п╪п╦ я└п╩п╟пЁп╟п╪п╦ (п╢п╩я▐ п╨п╟п╤п╢п╬п╧ п©п╩п╬я│п╨п╬я│я┌п╦)
	 */
	std::vector<Bitvector> NamesToBitvector(
		const std::vector<std::string>& names,
		const char** array,
		int plane_count = 4
	) const;

	/**
	 * п÷я─п╣п╬п╠я─п╟п╥п╬п╡п╟я┌я▄ bitvector п╡ я│п©п╦я│п╬п╨ п╦п╪п╣п╫ я└п╩п╟пЁп╬п╡.
	 * @param flags п°п╟я│я│п╦п╡ bitvector (п╢п╩я▐ п╨п╟п╤п╢п╬п╧ п©п╩п╬я│п╨п╬я│я┌п╦)
	 * @param array п°п╟я│я│п╦п╡ я│я┌я─п╬п╨ я│ п╦п╪п╣п╫п╟п╪п╦ я└п╩п╟пЁп╬п╡
	 * @param plane_count п п╬п╩п╦я┤п╣я│я┌п╡п╬ п©п╩п╬я│п╨п╬я│я┌п╣п╧
	 * @return п║п©п╦я│п╬п╨ п╦п╪п╣п╫ я┐я│я┌п╟п╫п╬п╡п╩п╣п╫п╫я▀я┘ я└п╩п╟пЁп╬п╡
	 */
	std::vector<std::string> BitvectorToNames(
		const Bitvector* flags,
		const char** array,
		int plane_count = 4
	) const;

	/**
	 * п÷я─п╣п╬п╠я─п╟п╥п╬п╡п╟я┌я▄ ASCII я└п╩п╟пЁп╦ п╡ bitvector (п╢п╩я▐ я│п╬п╡п╪п╣я│я┌п╦п╪п╬я│я┌п╦ я│п╬ я│я┌п╟я─я▀п╪ я└п╬я─п╪п╟я┌п╬п╪).
	 * п╓п╬я─п╪п╟я┌: "a0b1c0" п╬п╥п╫п╟я┤п╟п╣я┌ я└п╩п╟пЁп╦ a (п©п╩п╬я│п╨п╬я│я┌я▄ 0), b (п©п╩п╬я│п╨п╬я│я┌я▄ 1), c (п©п╩п╬я│п╨п╬я│я┌я▄ 0)
	 * @param ascii ASCII я│я┌я─п╬п╨п╟ я└п╩п╟пЁп╬п╡
	 * @param array п°п╟я│я│п╦п╡ я│я┌я─п╬п╨ я│ п╦п╪п╣п╫п╟п╪п╦ я└п╩п╟пЁп╬п╡
	 * @param plane_count п п╬п╩п╦я┤п╣я│я┌п╡п╬ п©п╩п╬я│п╨п╬я│я┌п╣п╧
	 * @return Bitvector я│ я┐я│я┌п╟п╫п╬п╡п╩п╣п╫п╫я▀п╪п╦ я└п╩п╟пЁп╟п╪п╦
	 */
	std::vector<Bitvector> AsciiFlagsToBitvector(
		const char* ascii,
		const char** array,
		int plane_count = 4
	) const;

	/**
	 * п÷п╬п╩я┐я┤п╦я┌я▄ п╦п╪я▐ п©п╬ п╦п╫п╢п╣п╨я│я┐ п╦п╥ п╪п╟я│я│п╦п╡п╟.
	 * @param index п≤п╫п╢п╣п╨я│ п╡ п╪п╟я│я│п╦п╡п╣
	 * @param array п°п╟я│я│п╦п╡ я│я┌я─п╬п╨
	 * @return п≤п╪я▐ п╦п╩п╦ п©я┐я│я┌п╟я▐ я│я┌я─п╬п╨п╟ п╣я│п╩п╦ п╦п╫п╢п╣п╨я│ п╫п╣п╡п╣я─п╫я▀п╧
	 */
	std::string GetNameByIndex(int index, const char** array) const;

 private:
	/**
	 * п≤п╥п╡п╩п╣я┤я▄ п╫п╬п╪п╣я─ п©п╩п╬я│п╨п╬я│я┌п╦ п╦п╥ ASCII я└п╩п╟пЁп╟ (0-3).
	 * п²п╟п©я─п╦п╪п╣я─: 'a0' Б├▓ п©п╩п╬я│п╨п╬я│я┌я▄ 0, 'b2' Б├▓ п©п╩п╬я│п╨п╬я│я┌я▄ 2
	 */
	int GetPlaneFromAscii(char letter, char digit) const;

	/**
	 * п÷п╬п╩я┐я┤п╦я┌я▄ п╠п╦я┌п╬п╡я┐я▌ п╪п╟я│п╨я┐ п╢п╩я▐ п©п╬п╥п╦я├п╦п╦ п╡ п╪п╟я│я│п╦п╡п╣.
	 * @param position п÷п╬п╥п╦я├п╦я▐ п╡ п╪п╟я│я│п╦п╡п╣ я└п╩п╟пЁп╬п╡ (0-29 п╢п╩я▐ п╬п╢п╫п╬п╧ п©п╩п╬я│п╨п╬я│я┌п╦)
	 * @return п▒п╦я┌п╬п╡п╟я▐ п╪п╟я│п╨п╟ (1 << position)
	 */
	Bitvector GetBitmask(int position) const;
};

#endif //BYLINS_SRC_ENGINE_BOOT_CONSTANTS_MAPPER_H_

// vim: ts=4 sw=4 tw=0 noet syntax=cpp :

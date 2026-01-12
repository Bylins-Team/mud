/**
 \file constants_mapper.cpp
 \authors Created by Claude Code
 \date 2026.01.12
 \brief п═п╣п╟п╩п╦п╥п╟я├п╦я▐ я┐п╫п╦п╡п╣я─я│п╟п╩я▄п╫п╬пЁп╬ п╪п╟п©п©п╣я─п╟ п╨п╬п╫я│я┌п╟п╫я┌.
*/

#include "constants_mapper.h"
#include "utils/utils.h"
#include "utils/logger.h"
#include <algorithm>
#include <cstring>

// п≤я│п©п╬п╩я▄п╥я┐п╣п╪ п╨п╬п╫я│я┌п╟п╫я┌я▀ п╦п╥ structs.h: kIntOne, kIntTwo, kIntThree
constexpr int kBitsPerPlane = 30;           // п▒п╦я┌ п╫п╟ п©п╩п╬я│п╨п╬я│я┌я▄

/**
 * п²п╟п╧я┌п╦ п╦п╫п╢п╣п╨я│ п╦п╪п╣п╫п╦ п╡ п╪п╟я│я│п╦п╡п╣ я│я┌я─п╬п╨.
 */
int ConstantsMapper::FindNameIndex(const std::string& name, const char** array) const {
	if (!array || name.empty()) {
		return -1;
	}

	// п÷я─п╬п╧я┌п╦ п©п╬ п╪п╟я│я│п╦п╡я┐ п╢п╬ "\n"
	for (int i = 0; array[i] && strcmp(array[i], "\n") != 0; ++i) {
		// п║я─п╟п╡п╫п╣п╫п╦п╣ п╠п╣п╥ я┐я┤п╣я┌п╟ я─п╣пЁп╦я│я┌я─п╟
		if (strcasecmp(array[i], name.c_str()) == 0) {
			return i;
		}

		// п╒п╟п╨п╤п╣ п©п╬п©я─п╬п╠п╬п╡п╟я┌я▄ п╠п╣п╥ п©я─п╣я└п╦п╨я│п╟ k (kMale Б├▓ Male)
		if (name.length() > 1 && name[0] == 'k' &&
		    strcasecmp(array[i], name.c_str() + 1) == 0) {
			return i;
		}
	}

	return -1;
}

/**
 * п÷я─п╣п╬п╠я─п╟п╥п╬п╡п╟я┌я▄ я│п©п╦я│п╬п╨ п╦п╪п╣п╫ п╡ bitvector.
 */
std::vector<Bitvector> ConstantsMapper::NamesToBitvector(
	const std::vector<std::string>& names,
	const char** array,
	int plane_count
) const {
	std::vector<Bitvector> result(plane_count, 0);

	for (const auto& name : names) {
		int index = FindNameIndex(name, array);
		if (index == -1) {
			log("SYSERR: ConstantsMapper: unknown flag name '%s'", name.c_str());
			continue;
		}

		// п·п©я─п╣п╢п╣п╩п╦я┌я▄ п©п╩п╬я│п╨п╬я│я┌я▄ п╦ п©п╬п╥п╦я├п╦я▌
		int plane = index / kBitsPerPlane;
		int position = index % kBitsPerPlane;

		if (plane >= plane_count) {
			log("SYSERR: ConstantsMapper: flag '%s' index %d exceeds plane count %d",
			    name.c_str(), index, plane_count);
			continue;
		}

		// пёя│я┌п╟п╫п╬п╡п╦я┌я▄ п╠п╦я┌
		result[plane] |= GetBitmask(position);
	}

	return result;
}

/**
 * п÷я─п╣п╬п╠я─п╟п╥п╬п╡п╟я┌я▄ bitvector п╡ я│п©п╦я│п╬п╨ п╦п╪п╣п╫.
 */
std::vector<std::string> ConstantsMapper::BitvectorToNames(
	const Bitvector* flags,
	const char** array,
	int plane_count
) const {
	std::vector<std::string> result;

	if (!flags || !array) {
		return result;
	}

	// п÷я─п╬п╧я┌п╦ п©п╬ п╡я│п╣п╪ п╠п╦я┌п╟п╪ п╡п╬ п╡я│п╣я┘ п©п╩п╬я│п╨п╬я│я┌я▐я┘
	int index = 0;
	for (int plane = 0; plane < plane_count; ++plane) {
		for (int bit = 0; bit < kBitsPerPlane; ++bit) {
			Bitvector mask = GetBitmask(bit);
			if (flags[plane] & mask) {
				std::string name = GetNameByIndex(index, array);
				if (!name.empty()) {
					result.push_back(name);
				}
			}
			++index;
		}
	}

	return result;
}

/**
 * п÷я─п╣п╬п╠я─п╟п╥п╬п╡п╟я┌я▄ ASCII я└п╩п╟пЁп╦ п╡ bitvector.
 * п╓п╬я─п╪п╟я┌: "a0b1c0" Б├▓ п╠я┐п╨п╡п╟ = п©п╬п╥п╦я├п╦я▐ п╡ п╟п╩я└п╟п╡п╦я┌п╣, я├п╦я└я─п╟ = п©п╩п╬я│п╨п╬я│я┌я▄
 */
std::vector<Bitvector> ConstantsMapper::AsciiFlagsToBitvector(
	const char* ascii,
	const char** array,
	int plane_count
) const {
	std::vector<Bitvector> result(plane_count, 0);

	if (!ascii || !array) {
		return result;
	}

	// п÷п╟я─я│п╦я┌я▄ п©п╬ п©п╟я─п╟п╪ я│п╦п╪п╡п╬п╩п╬п╡: п╠я┐п╨п╡п╟ + я├п╦я└я─п╟
	for (size_t i = 0; ascii[i] != '\0'; ) {
		if (!isalpha(ascii[i])) {
			++i;
			continue;
		}

		char letter = tolower(ascii[i]);
		int plane = 0;

		// п║п╩п╣п╢я┐я▌я┴п╦п╧ я│п╦п╪п╡п╬п╩ п╪п╬п╤п╣я┌ п╠я▀я┌я▄ я├п╦я└я─п╬п╧ п©п╩п╬я│п╨п╬я│я┌п╦
		if (ascii[i + 1] && isdigit(ascii[i + 1])) {
			plane = ascii[i + 1] - '0';
			i += 2;
		} else {
			i += 1;
		}

		if (plane >= plane_count) {
			log("SYSERR: ConstantsMapper: ASCII flag plane %d exceeds count %d",
			    plane, plane_count);
			continue;
		}

		// п▒я┐п╨п╡п╟ я│п╬п╬я┌п╡п╣я┌я│я┌п╡я┐п╣я┌ п©п╬п╥п╦я├п╦п╦: a=0, b=1, c=2, ...
		int position = letter - 'a';
		if (position < 0 || position >= kBitsPerPlane) {
			log("SYSERR: ConstantsMapper: ASCII flag letter '%c' out of range", letter);
			continue;
		}

		result[plane] |= GetBitmask(position);
	}

	return result;
}

/**
 * п÷п╬п╩я┐я┤п╦я┌я▄ п╦п╪я▐ п©п╬ п╦п╫п╢п╣п╨я│я┐.
 */
std::string ConstantsMapper::GetNameByIndex(int index, const char** array) const {
	if (!array || index < 0) {
		return "";
	}

	int i = 0;
	while (array[i] && strcmp(array[i], "\n") != 0) {
		if (i == index) {
			return array[i];
		}
		++i;
	}

	return "";
}

/**
 * п≤п╥п╡п╩п╣я┤я▄ п╫п╬п╪п╣я─ п©п╩п╬я│п╨п╬я│я┌п╦ п╦п╥ ASCII я└п╩п╟пЁп╟.
 */
int ConstantsMapper::GetPlaneFromAscii(char letter, char digit) const {
	(void)letter; // п÷п╬п╨п╟ п╫п╣ п╦я│п©п╬п╩я▄п╥я┐п╣я┌я│я▐
	if (isdigit(digit)) {
		return digit - '0';
	}
	return 0; // п÷п╬ я┐п╪п╬п╩я┤п╟п╫п╦я▌ п©п╩п╬я│п╨п╬я│я┌я▄ 0
}

/**
 * п÷п╬п╩я┐я┤п╦я┌я▄ п╠п╦я┌п╬п╡я┐я▌ п╪п╟я│п╨я┐ п╢п╩я▐ п©п╬п╥п╦я├п╦п╦.
 */
Bitvector ConstantsMapper::GetBitmask(int position) const {
	if (position < 0 || position >= kBitsPerPlane) {
		return 0;
	}
	return static_cast<Bitvector>(1) << position;
}

// vim: ts=4 sw=4 tw=0 noet syntax=cpp :

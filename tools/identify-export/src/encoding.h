#ifndef IDENTIFY_EXPORT_ENCODING_H
#define IDENTIFY_EXPORT_ENCODING_H

#include <string>

// Конвертирует строку из KOI8-R в UTF-8
std::string ConvertKOI8RtoUTF8(const std::string& koi8r);

// Конвертирует C-строку из KOI8-R в UTF-8
std::string ConvertKOI8RtoUTF8(const char* koi8r);

#endif // IDENTIFY_EXPORT_ENCODING_H

// vim: ts=4 sw=4 tw=0 noet syntax=cpp :

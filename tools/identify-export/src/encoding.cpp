#include "encoding.h"
#include <iconv.h>
#include <cstring>
#include <vector>

std::string ConvertKOI8RtoUTF8(const std::string& koi8r)
{
	if (koi8r.empty())
	{
		return "";
	}

	return ConvertKOI8RtoUTF8(koi8r.c_str());
}

std::string ConvertKOI8RtoUTF8(const char* koi8r)
{
	if (!koi8r || koi8r[0] == '\0')
	{
		return "";
	}

	// Открыть iconv дескриптор
	iconv_t cd = iconv_open("UTF-8", "KOI8-R");
	if (cd == (iconv_t)-1)
	{
		// Ошибка открытия iconv, возвращаем исходную строку
		return std::string(koi8r);
	}

	// Подготовить буферы
	size_t inbytesleft = std::strlen(koi8r);
	size_t outbytesleft = inbytesleft * 4; // UTF-8 может быть до 4 байт на символ
	std::vector<char> outbuf(outbytesleft);

	char* inbuf = const_cast<char*>(koi8r);
	char* outptr = outbuf.data();

	// Выполнить конвертацию
	size_t result = iconv(cd, &inbuf, &inbytesleft, &outptr, &outbytesleft);

	// Закрыть дескриптор
	iconv_close(cd);

	if (result == (size_t)-1)
	{
		// Ошибка конвертации, возвращаем исходную строку
		return std::string(koi8r);
	}

	// Вернуть сконвертированную строку
	size_t converted_len = outbuf.size() - outbytesleft;
	return std::string(outbuf.data(), converted_len);
}

// vim: ts=4 sw=4 tw=0 noet syntax=cpp :

#pragma once

#include <cstddef>

/* Nexus ImGui default font (ProggyClean) lacks common Unicode punctuation and
   many accented glyphs - missing codepoints draw as '?'. Prefer ASCII in
   static labels; run dynamic strings through SanitizeForUi before ImGui. */
namespace UiAscii
{
	inline void SanitizeForUi(char* dst, size_t dstLen, const char* src)
	{
		if (!dst || dstLen == 0)
			return;
		dst[0] = 0;
		if (!src)
			return;
		size_t o = 0;
		for (size_t i = 0; src[i] && o + 1 < dstLen; )
		{
			const unsigned char c = static_cast<unsigned char>(src[i]);
			if (c < 0x80)
			{
				dst[o++] = static_cast<char>(c);
				++i;
				continue;
			}
			/* em/en dash */
			if (c == 0xE2 && static_cast<unsigned char>(src[i + 1]) == 0x80 &&
				(static_cast<unsigned char>(src[i + 2]) == 0x94 ||
					static_cast<unsigned char>(src[i + 2]) == 0x93))
			{
				dst[o++] = '-';
				i += 3;
				continue;
			}
			/* middle dot */
			if (c == 0xC2 && static_cast<unsigned char>(src[i + 1]) == 0xB7)
			{
				dst[o++] = '|';
				i += 2;
				continue;
			}
			/* ellipsis */
			if (c == 0xE2 && static_cast<unsigned char>(src[i + 1]) == 0x80 &&
				static_cast<unsigned char>(src[i + 2]) == 0xA6)
			{
				if (o + 3 < dstLen)
				{
					dst[o++] = '.';
					dst[o++] = '.';
					dst[o++] = '.';
				}
				i += 3;
				continue;
			}
			/* -> */
			if (c == 0xE2 && static_cast<unsigned char>(src[i + 1]) == 0x86 &&
				static_cast<unsigned char>(src[i + 2]) == 0x92)
			{
				if (o + 2 < dstLen)
				{
					dst[o++] = '-';
					dst[o++] = '>';
				}
				i += 3;
				continue;
			}
			/* x multiplication */
			if (c == 0xC3 && static_cast<unsigned char>(src[i + 1]) == 0x97)
			{
				dst[o++] = 'x';
				i += 2;
				continue;
			}
			/* common Latin-1 accented letters -> ASCII base */
			if (c == 0xC3)
			{
				const unsigned char n = static_cast<unsigned char>(src[i + 1]);
				char out = 0;
				if (n >= 0x80 && n <= 0x85) out = 'A';
				else if (n == 0x87) out = 'C';
				else if (n >= 0x88 && n <= 0x8B) out = 'E';
				else if (n >= 0x8C && n <= 0x8F) out = 'I';
				else if (n == 0x91) out = 'N';
				else if (n >= 0x92 && n <= 0x96) out = 'O';
				else if (n >= 0x99 && n <= 0x9C) out = 'U';
				else if (n == 0x9F) out = 's'; /* sharp s */
				else if (n >= 0xA0 && n <= 0xA5) out = 'a';
				else if (n == 0xA7) out = 'c';
				else if (n >= 0xA8 && n <= 0xAB) out = 'e';
				else if (n >= 0xAC && n <= 0xAF) out = 'i';
				else if (n == 0xB1) out = 'n';
				else if (n >= 0xB2 && n <= 0xB6) out = 'o';
				else if (n >= 0xB9 && n <= 0xBC) out = 'u';
				else if (n == 0xBD || n == 0xBF) out = 'y';
				if (out)
				{
					dst[o++] = out;
					i += 2;
					continue;
				}
			}
			/* drop other multibyte */
			if ((c & 0xE0) == 0xC0) i += 2;
			else if ((c & 0xF0) == 0xE0) i += 3;
			else if ((c & 0xF8) == 0xF0) i += 4;
			else ++i;
		}
		dst[o] = 0;
	}
}

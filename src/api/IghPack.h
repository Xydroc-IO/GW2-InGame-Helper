#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include <windows.h>

/* IGH1 little-endian pack (not zip). See scripts/ighpack.py. */
namespace IghPack
{
	struct Entry
	{
		unsigned packed = 0;
		unsigned uncomp = 0;
		unsigned stored = 0;
		unsigned long long offset = 0;
	};

	class Reader
	{
	public:
		bool OpenFile(const wchar_t* path);
		bool OpenBytes(const void* data, size_t size);
		void Close();
		bool Has(const char* name) const;
		bool Get(const char* name, std::string* out, size_t maxOut);

	private:
		bool ParseIndex(const unsigned char* idx, size_t idxBytes, unsigned count);
		bool ReadStored(const Entry& e, std::string* stored);

		HANDLE mFile = INVALID_HANDLE_VALUE;
		std::vector<unsigned char> mMem;
		std::unordered_map<std::string, Entry> mIndex;
	};
}

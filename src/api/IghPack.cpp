#include "IghPack.h"

#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "miniz.h"

#include <cstdint>
#include <cstring>

namespace
{
	constexpr unsigned kHeader = 20;
	constexpr unsigned kMaxCount = 100000;
	constexpr unsigned kMaxName = 200;

	bool Gunzip(const std::string& in, std::string* out, size_t maxOut)
	{
		if (in.size() < 18 || !out)
			return false;
		mz_stream s{};
		if (mz_inflateInit2(&s, 15 + 16) != MZ_OK)
			return false;
		out->assign(in.size() * 4u + 256u, '\0');
		s.next_in = reinterpret_cast<const unsigned char*>(in.data());
		s.avail_in = static_cast<unsigned>(in.size());
		int rc = MZ_OK;
		while (rc == MZ_OK || rc == MZ_BUF_ERROR)
		{
			if (s.total_out >= out->size())
			{
				if (out->size() >= maxOut)
				{
					mz_inflateEnd(&s);
					out->clear();
					return false;
				}
				out->resize(out->size() * 2u);
			}
			s.next_out = reinterpret_cast<unsigned char*>(&(*out)[s.total_out]);
			s.avail_out = static_cast<unsigned>(out->size() - s.total_out);
			rc = mz_inflate(&s, MZ_FINISH);
		}
		const size_t n = static_cast<size_t>(s.total_out);
		mz_inflateEnd(&s);
		if (rc != MZ_STREAM_END || n > maxOut)
		{
			out->clear();
			return false;
		}
		out->resize(n);
		return true;
	}

	bool ValidName(const std::string& name)
	{
		if (name.empty() || name.size() > kMaxName || name[0] == '/' ||
			name.find("..") != std::string::npos || name.find('\\') != std::string::npos)
			return false;
		for (unsigned char c : name)
		{
			const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
				(c >= '0' && c <= '9') || c == '/' || c == '.' || c == '_' || c == '-';
			if (!ok)
				return false;
		}
		return true;
	}

	bool ParseHeader(const unsigned char* p, size_t n, unsigned* count, unsigned* idxBytes)
	{
		if (!p || n < kHeader || std::memcmp(p, "IGH1", 4) != 0)
			return false;
		unsigned ver = 0, flags = 0;
		std::memcpy(&ver, p + 4, 4);
		std::memcpy(&flags, p + 8, 4);
		(void)flags;
		std::memcpy(count, p + 12, 4);
		std::memcpy(idxBytes, p + 16, 4);
		return ver == 1 && *count > 0 && *count <= kMaxCount && *idxBytes > 0;
	}
}

bool IghPack::Reader::ParseIndex(const unsigned char* idx, size_t idxBytes, unsigned count)
{
	mIndex.clear();
	size_t p = 0;
	for (unsigned i = 0; i < count; ++i)
	{
		if (p + 2 > idxBytes)
			return false;
		uint16_t nlen = 0;
		std::memcpy(&nlen, idx + p, 2);
		p += 2;
		if (nlen == 0 || nlen > kMaxName || p + nlen + 1 + 16 > idxBytes)
			return false;
		std::string name(reinterpret_cast<const char*>(idx + p), nlen);
		p += nlen;
		if (!ValidName(name))
			return false;
		Entry e{};
		e.packed = idx[p];
		p += 1;
		std::memcpy(&e.uncomp, idx + p, 4);
		std::memcpy(&e.stored, idx + p + 4, 4);
		std::memcpy(&e.offset, idx + p + 8, 8);
		p += 16;
		if (e.stored == 0 || e.uncomp == 0)
			return false;
		mIndex.emplace(std::move(name), e);
	}
	return !mIndex.empty();
}

void IghPack::Reader::Close()
{
	if (mFile != INVALID_HANDLE_VALUE)
	{
		CloseHandle(mFile);
		mFile = INVALID_HANDLE_VALUE;
	}
	mMem.clear();
	mIndex.clear();
}

bool IghPack::Reader::OpenBytes(const void* data, size_t size)
{
	Close();
	if (!data || size < kHeader)
		return false;
	const auto* p = static_cast<const unsigned char*>(data);
	unsigned count = 0, idxBytes = 0;
	if (!ParseHeader(p, size, &count, &idxBytes))
		return false;
	if (kHeader + idxBytes > size)
		return false;
	mMem.assign(p, p + size);
	if (!ParseIndex(mMem.data() + kHeader, idxBytes, count))
	{
		Close();
		return false;
	}
	return true;
}

bool IghPack::Reader::OpenFile(const wchar_t* path)
{
	Close();
	if (!path || !path[0])
		return false;
	mFile = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (mFile == INVALID_HANDLE_VALUE)
		return false;
	unsigned char hdr[kHeader];
	DWORD rd = 0;
	if (!ReadFile(mFile, hdr, kHeader, &rd, nullptr) || rd != kHeader)
	{
		Close();
		return false;
	}
	unsigned count = 0, idxBytes = 0;
	if (!ParseHeader(hdr, kHeader, &count, &idxBytes))
	{
		Close();
		return false;
	}
	std::vector<unsigned char> idx(idxBytes);
	if (!ReadFile(mFile, idx.data(), idxBytes, &rd, nullptr) || rd != idxBytes)
	{
		Close();
		return false;
	}
	if (!ParseIndex(idx.data(), idxBytes, count))
	{
		Close();
		return false;
	}
	return true;
}

bool IghPack::Reader::Has(const char* name) const
{
	return name && mIndex.find(name) != mIndex.end();
}

bool IghPack::Reader::ReadStored(const Entry& e, std::string* stored)
{
	if (!stored)
		return false;
	stored->assign(e.stored, '\0');
	if (!mMem.empty())
	{
		if (e.offset + e.stored > mMem.size())
			return false;
		stored->assign(reinterpret_cast<const char*>(mMem.data() + e.offset), e.stored);
		return true;
	}
	if (mFile == INVALID_HANDLE_VALUE)
		return false;
	LARGE_INTEGER pos{};
	pos.QuadPart = static_cast<LONGLONG>(e.offset);
	if (!SetFilePointerEx(mFile, pos, nullptr, FILE_BEGIN))
		return false;
	DWORD rd = 0;
	return ReadFile(mFile, stored->data(), e.stored, &rd, nullptr) && rd == e.stored;
}

bool IghPack::Reader::Get(const char* name, std::string* out, size_t maxOut)
{
	if (!name || !out)
		return false;
	auto it = mIndex.find(name);
	if (it == mIndex.end())
		return false;
	const Entry& e = it->second;
	if (e.uncomp > maxOut)
		return false;
	std::string stored;
	if (!ReadStored(e, &stored))
		return false;
	if (e.packed)
		return Gunzip(stored, out, maxOut) && out->size() == e.uncomp;
	if (stored.size() != e.uncomp)
		return false;
	*out = std::move(stored);
	return true;
}

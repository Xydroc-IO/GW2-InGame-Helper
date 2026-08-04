/* CEF runtime SHA-256 verify + zip extract. */
#include "CefRuntime.h"
#include "CefRuntimeInternal.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <windows.h>
#include <bcrypt.h>

#include "miniz.h"

namespace CefRuntimeDetail
{
bool Sha256File(const std::wstring& path, unsigned char out[32])
{
	HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE)
		return false;

	BCRYPT_ALG_HANDLE alg = nullptr;
	BCRYPT_HASH_HANDLE hash = nullptr;
	DWORD objLen = 0, cb = 0;
	bool ok = false;
	std::vector<unsigned char> obj;

	if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0)
		goto done;
	if (BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objLen),
			sizeof(objLen), &cb, 0) != 0)
		goto done;
	obj.resize(objLen);
	if (BCryptCreateHash(alg, &hash, obj.data(), objLen, nullptr, 0, 0) != 0)
		goto done;

	{
		unsigned char buf[1 << 16];
		for (;;)
		{
			DWORD got = 0;
			if (!ReadFile(h, buf, sizeof(buf), &got, nullptr))
				goto done;
			if (got == 0)
				break;
			if (BCryptHashData(hash, buf, got, 0) != 0)
				goto done;
		}
	}
	ok = (BCryptFinishHash(hash, out, 32, 0) == 0);

done:
	if (hash)
		BCryptDestroyHash(hash);
	if (alg)
		BCryptCloseAlgorithmProvider(alg, 0);
	CloseHandle(h);
	return ok;
}

int HexNibble(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

bool ParseSha256Hex(const char* hex, unsigned char out[32])
{
	if (!hex || std::strlen(hex) != 64)
		return false;
	for (int i = 0; i < 32; ++i)
	{
		const int hi = HexNibble(hex[i * 2]);
		const int lo = HexNibble(hex[i * 2 + 1]);
		if (hi < 0 || lo < 0)
			return false;
		out[i] = static_cast<unsigned char>((hi << 4) | lo);
	}
	return true;
}

bool VerifySha256(const std::wstring& path, void (*statusFn)(const char*))
{
	if (!CefRuntime::kSha256Hex || !CefRuntime::kSha256Hex[0])
	{
		Status(statusFn, "CEF SHA256 not configured in CefRuntime.h");
		return false;
	}
	unsigned char expect[32]{};
	if (!ParseSha256Hex(CefRuntime::kSha256Hex, expect))
	{
		Status(statusFn, "CEF SHA256 constant is invalid");
		return false;
	}
	Status(statusFn, "Verifying CEF…");
	unsigned char got[32]{};
	if (!Sha256File(path, got))
	{
		Status(statusFn, "CEF hash read failed");
		return false;
	}
	if (std::memcmp(got, expect, 32) != 0)
	{
		Status(statusFn, "CEF SHA256 mismatch");
		return false;
	}
	return true;
}

bool ExtractZip(const std::wstring& zipPath, const std::wstring& destDir, void (*statusFn)(const char*))
{
	Status(statusFn, "Extracting CEF…");
	const std::string zipUtf8 = WideToUtf8(zipPath);

	mz_zip_archive zip{};
	if (!mz_zip_reader_init_file(&zip, zipUtf8.c_str(), 0))
	{
		Status(statusFn, "CEF zip open failed");
		return false;
	}

	const std::wstring staging = destDir + L".staging";
	RemoveTree(staging);
	if (!MkDirDeep(staging))
	{
		mz_zip_reader_end(&zip);
		Status(statusFn, "CEF staging mkdir failed");
		return false;
	}

	const mz_uint n = mz_zip_reader_get_num_files(&zip);
	bool ok = true;
	for (mz_uint i = 0; i < n; ++i)
	{
		mz_zip_archive_file_stat st{};
		if (!mz_zip_reader_file_stat(&zip, i, &st))
		{
			ok = false;
			break;
		}
		if (st.m_is_directory)
		{
			std::wstring dir = staging + L"\\";
			for (const char* p = st.m_filename; *p; ++p)
				dir.push_back(*p == '/' ? L'\\' : static_cast<wchar_t>(static_cast<unsigned char>(*p)));
			if (!MkDirDeep(dir))
			{
				ok = false;
				break;
			}
			continue;
		}

		std::string rel = st.m_filename;
		for (char& c : rel)
		{
			if (c == '/')
				c = '\\';
		}
		if (rel.find("..") != std::string::npos)
		{
			ok = false;
			break;
		}

		const std::wstring outPath = staging + L"\\" + std::wstring(rel.begin(), rel.end());
		const size_t slash = outPath.find_last_of(L"\\/");
		if (slash != std::wstring::npos)
		{
			if (!MkDirDeep(outPath.substr(0, slash)))
			{
				ok = false;
				break;
			}
		}

		const std::string outUtf8 = WideToUtf8(outPath);
		if (!mz_zip_reader_extract_to_file(&zip, i, outUtf8.c_str(), 0))
		{
			ok = false;
			break;
		}
	}
	mz_zip_reader_end(&zip);

	if (!ok)
	{
		RemoveTree(staging);
		Status(statusFn, "CEF extract failed");
		return false;
	}
	if (GetFileAttributesW(Join(staging, L"libcef.dll").c_str()) == INVALID_FILE_ATTRIBUTES)
	{
		RemoveTree(staging);
		Status(statusFn, "CEF zip missing libcef.dll");
		return false;
	}

	RemoveTree(destDir);
	if (!MoveFileW(staging.c_str(), destDir.c_str()))
	{
		Status(statusFn, "CEF install move failed");
		RemoveTree(staging);
		return false;
	}
	return true;
}
}

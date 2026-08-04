#include "EiRuntime.h"
#include "EiRuntimeInternal.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <windows.h>
#include <bcrypt.h>

#include "miniz.h"

namespace EiRuntimeDetail
{
	void Status(void (*fn)(const char*), const char* msg)
	{
		if (fn && msg)
			fn(msg);
	}

	std::wstring Join(const std::wstring& a, const wchar_t* b)
	{
		if (a.empty())
			return b ? b : L"";
		if (!b || !b[0])
			return a;
		if (a.back() == L'\\' || a.back() == L'/')
			return a + b;
		return a + L"\\" + b;
	}

	std::string WideToUtf8(const std::wstring& w)
	{
		if (w.empty())
			return {};
		int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
		if (n <= 0)
			return {};
		std::string out(static_cast<size_t>(n - 1), '\0');
		WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, out.data(), n, nullptr, nullptr);
		return out;
	}

	bool ReadStamp(const std::wstring& verPath, std::string& out)
	{
		out.clear();
		HANDLE h = CreateFileW(verPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return false;
		char buf[64]{};
		DWORD got = 0;
		const BOOL ok = ReadFile(h, buf, sizeof(buf) - 1, &got, nullptr);
		CloseHandle(h);
		if (!ok || got == 0)
			return false;
		out.assign(buf, got);
		while (!out.empty() && (out.back() == '\n' || out.back() == '\r' || out.back() == ' '))
			out.pop_back();
		return true;
	}

	bool WriteStamp(const std::wstring& verPath, const char* stamp)
	{
		HANDLE h = CreateFileW(verPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return false;
		DWORD written = 0;
		const BOOL ok = WriteFile(h, stamp, static_cast<DWORD>(std::strlen(stamp)), &written, nullptr);
		CloseHandle(h);
		return ok && written == std::strlen(stamp);
	}

	bool MkDirDeep(const std::wstring& path)
	{
		if (path.empty())
			return false;
		if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES)
			return true;
		const size_t slash = path.find_last_of(L"\\/");
		if (slash != std::wstring::npos)
		{
			if (!MkDirDeep(path.substr(0, slash)))
				return false;
		}
		return CreateDirectoryW(path.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
	}

	void RemoveTree(const std::wstring& path)
	{
		WIN32_FIND_DATAW fd{};
		const std::wstring glob = Join(path, L"*");
		HANDLE h = FindFirstFileW(glob.c_str(), &fd);
		if (h == INVALID_HANDLE_VALUE)
		{
			RemoveDirectoryW(path.c_str());
			DeleteFileW(path.c_str());
			return;
		}
		do
		{
			if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
				continue;
			const std::wstring child = Join(path, fd.cFileName);
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				RemoveTree(child);
			else
				DeleteFileW(child.c_str());
		} while (FindNextFileW(h, &fd));
		FindClose(h);
		RemoveDirectoryW(path.c_str());
	}

	bool FindCliRecursive(const std::wstring& dir, std::wstring& out, int depth)
	{
		if (dir.empty() || depth > 8)
			return false;
		const std::wstring direct = Join(dir, L"GuildWars2EliteInsights-CLI.exe");
		if (GetFileAttributesW(direct.c_str()) != INVALID_FILE_ATTRIBUTES)
		{
			out = direct;
			return true;
		}

		WIN32_FIND_DATAW fd{};
		const std::wstring glob = Join(dir, L"*");
		HANDLE h = FindFirstFileW(glob.c_str(), &fd);
		if (h == INVALID_HANDLE_VALUE)
			return false;
		bool found = false;
		do
		{
			if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
				continue;
			if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				continue;
			if (FindCliRecursive(Join(dir, fd.cFileName), out, depth + 1))
			{
				found = true;
				break;
			}
		} while (FindNextFileW(h, &fd));
		FindClose(h);
		return found;
	}

	bool TreeLooksComplete(const std::wstring& eiDir)
	{
		std::wstring cli;
		return FindCliRecursive(eiDir, cli, 0);
	}

	bool AlreadyInstalled(const std::wstring& eiDir)
	{
		if (!TreeLooksComplete(eiDir))
			return false;
		std::string stamp;
		return ReadStamp(Join(eiDir, L"ei.ver"), stamp) && !stamp.empty();
	}

	bool MatchesStamp(const std::wstring& eiDir, const char* stamp)
	{
		if (!stamp || !stamp[0] || !TreeLooksComplete(eiDir))
			return false;
		std::string cur;
		if (!ReadStamp(Join(eiDir, L"ei.ver"), cur))
			return false;
		return cur == stamp;
	}

	std::string NormalizeTag(const std::string& tag)
	{
		if (tag.size() >= 2 && (tag[0] == 'v' || tag[0] == 'V') &&
			tag[1] >= '0' && tag[1] <= '9')
			return tag.substr(1);
		return tag;
	}

	std::wstring DllDir()
	{
		HMODULE mod = nullptr;
		if (!GetModuleHandleExW(
				GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				reinterpret_cast<LPCWSTR>(&EiRuntime::EnsureInstalled), &mod) ||
			!mod)
			return {};
		wchar_t path[MAX_PATH]{};
		if (!GetModuleFileNameW(mod, path, MAX_PATH))
			return {};
		std::wstring full = path;
		const size_t slash = full.find_last_of(L"\\/");
		if (slash == std::wstring::npos)
			return {};
		return full.substr(0, slash);
	}

	void CleanupStrayZipInEi(const std::wstring& eiDir)
	{
		DeleteFileW(Join(eiDir, L"GW2EICLI.zip").c_str());
		DeleteFileW(Join(eiDir, L"ei-download.tmp").c_str());
	}

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

	bool VerifySha256(const std::wstring& path, const char* sha256Hex, void (*statusFn)(const char*))
	{
		if (!sha256Hex || !sha256Hex[0])
		{
			Status(statusFn, "EI SHA256 missing");
			return false;
		}
		unsigned char expect[32]{};
		if (!ParseSha256Hex(sha256Hex, expect))
		{
			Status(statusFn, "EI SHA256 is invalid");
			return false;
		}
		Status(statusFn, "Verifying Elite Insights…");
		unsigned char got[32]{};
		if (!Sha256File(path, got))
		{
			Status(statusFn, "EI hash read failed");
			return false;
		}
		if (std::memcmp(got, expect, 32) != 0)
		{
			Status(statusFn, "EI SHA256 mismatch");
			return false;
		}
		return true;
	}

	bool ExtractZip(const std::wstring& zipPath, const std::wstring& destDir, void (*statusFn)(const char*))
	{
		Status(statusFn, "Extracting Elite Insights…");
		const std::string zipUtf8 = WideToUtf8(zipPath);

		mz_zip_archive zip{};
		if (!mz_zip_reader_init_file(&zip, zipUtf8.c_str(), 0))
		{
			Status(statusFn, "EI zip open failed");
			return false;
		}

		const std::wstring staging = destDir + L".staging";
		RemoveTree(staging);
		if (!MkDirDeep(staging))
		{
			mz_zip_reader_end(&zip);
			Status(statusFn, "EI staging mkdir failed");
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
			Status(statusFn, "EI extract failed");
			return false;
		}
		if (!TreeLooksComplete(staging))
		{
			RemoveTree(staging);
			Status(statusFn, "EI zip missing CLI exe");
			return false;
		}

		RemoveTree(destDir);
		if (!MoveFileW(staging.c_str(), destDir.c_str()))
		{
			Status(statusFn, "EI install move failed");
			RemoveTree(staging);
			return false;
		}
		return true;
	}

} // namespace EiRuntimeDetail

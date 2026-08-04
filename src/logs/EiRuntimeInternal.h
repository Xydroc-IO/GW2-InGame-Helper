#pragma once

#include <cstddef>
#include <string>

#include <windows.h>

/* Shared helpers for EiRuntime.cpp / EiRuntimeFs.cpp / EiRuntimeHttp.cpp. */
namespace EiRuntimeDetail
{
	struct LatestRelease
	{
		std::string stamp;
		std::string url;
		std::string sha256;
	};

	void Status(void (*fn)(const char*), const char* msg);
	std::wstring Join(const std::wstring& a, const wchar_t* b);
	std::string WideToUtf8(const std::wstring& w);
	bool ReadStamp(const std::wstring& verPath, std::string& out);
	bool WriteStamp(const std::wstring& verPath, const char* stamp);
	bool MkDirDeep(const std::wstring& path);
	void RemoveTree(const std::wstring& path);
	bool FindCliRecursive(const std::wstring& dir, std::wstring& out, int depth);
	bool TreeLooksComplete(const std::wstring& eiDir);
	bool AlreadyInstalled(const std::wstring& eiDir);
	bool MatchesStamp(const std::wstring& eiDir, const char* stamp);
	std::string NormalizeTag(const std::string& tag);
	std::wstring DllDir();
	void CleanupStrayZipInEi(const std::wstring& eiDir);
	bool Sha256File(const std::wstring& path, unsigned char out[32]);
	int HexNibble(char c);
	bool ParseSha256Hex(const char* hex, unsigned char out[32]);
	bool VerifySha256(const std::wstring& path, const char* expectHex, void (*statusFn)(const char*));
	bool ExtractZip(const std::wstring& zipPath, const std::wstring& destDir, void (*statusFn)(const char*));
	bool DownloadToFile(const wchar_t* urlW, const std::wstring& outPath, void (*statusFn)(const char*));
	bool InstallFromZip(const std::wstring& zipPath, const std::wstring& eiDir,
		const char* stamp, const char* sha256Hex, void (*statusFn)(const char*), bool deleteZipAfter);
	bool HttpGetUtf8(const char* urlUtf8, std::string& body, void (*statusFn)(const char*));
	bool JsonStringAfterKey(const char* json, const char* key, std::string& out);
	bool ParseLatestCliRelease(const std::string& json, LatestRelease& out);
	bool QueryLatestRelease(LatestRelease& out, void (*statusFn)(const char*));
	bool FindLocalZip(const std::wstring& addonDir, std::wstring& outPath, bool& deleteAfter);
}

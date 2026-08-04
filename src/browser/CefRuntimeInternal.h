#pragma once

#include <string>

#include <windows.h>

/* Shared helpers for CefRuntime.cpp + Http/Verify/Fs TUs. */
namespace CefRuntimeDetail
{
	void Status(void (*fn)(const char*), const char* msg);
	std::wstring Join(const std::wstring& a, const wchar_t* b);
	std::string WideToUtf8(const std::wstring& w);
	bool ReadStamp(const std::wstring& verPath, std::string& out);
	bool WriteStamp(const std::wstring& verPath, const char* stamp);
	bool MkDirDeep(const std::wstring& path);
	void RemoveTree(const std::wstring& path);
	bool TreeLooksComplete(const std::wstring& cefDir);
	bool AlreadyInstalled(const std::wstring& cefDir);
	std::wstring DllDir();
	void CleanupStrayZipInCef(const std::wstring& cefDir);

	bool Sha256File(const std::wstring& path, unsigned char out[32]);
	int HexNibble(char c);
	bool ParseSha256Hex(const char* hex, unsigned char out[32]);
	bool VerifySha256(const std::wstring& path, void (*statusFn)(const char*));
	bool ExtractZip(const std::wstring& zipPath, const std::wstring& destDir, void (*statusFn)(const char*));

	bool DownloadToFile(const wchar_t* urlW, const std::wstring& outPath, void (*statusFn)(const char*));
	bool InstallFromZip(const std::wstring& zipPath, const std::wstring& cefDir,
		void (*statusFn)(const char*), bool deleteZipAfter);
	bool FindLocalZip(const std::wstring& addonDir, std::wstring& outPath, bool& deleteAfter);
}

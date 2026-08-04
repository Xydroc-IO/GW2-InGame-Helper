#pragma once

#include <atomic>
#include <cstddef>
#include <mutex>
#include <string>

#include <windows.h>

/* Shared state / helpers for PathingPacks.cpp + PathingPacksHttp.cpp. */
namespace PathingPacksDetail
{
	extern std::atomic<bool> gForceUpdate;
	extern std::atomic<bool> gUpdating;
	extern std::atomic<bool> gCancel;
	extern std::mutex gStatusMu;
	extern char gStatus[160];

	void SetStatus(const char* msg);
	std::wstring Utf8ToWide(const char* u);
	std::string WideToUtf8(const std::wstring& w);
	bool ReadStamp(const std::wstring& verPath, std::string& out);
	bool WriteStamp(const std::wstring& verPath, const std::string& stamp);
	bool FileExistsNonEmpty(const std::wstring& path);
	bool JsonStringAfterKey(const char* json, const char* key, std::string& out);

	bool HttpGetUtf8(const wchar_t* urlW, std::string& body, DWORD timeoutMs);
	bool HttpHeadStamp(const wchar_t* urlW, std::string& stampOut);
	bool DownloadToFile(const wchar_t* urlW, const std::wstring& outPath);
}

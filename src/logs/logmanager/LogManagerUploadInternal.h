#pragma once

#include "LogManagerShared.h"

#include <string>

#include <windows.h>

/* Shared upload/hydrate helpers for LogManagerUpload*.cpp. */
namespace LogManagerDetail
{
	std::string UrlEncode(const std::string& s);
	std::string PermalinkQueryValue(const std::string& permalink);
	void ApplyDpsReportMeta(LogEntry& e, const std::string& resp);
	bool FetchEiJsonFromReport(const std::string& permalink, std::string& json, std::string& err);
	bool HttpGetSimple(const char* hostA, const char* pathAndQuery, std::string& body, std::string& err);
	bool FetchDpsReportMeta(const std::string& permalink, std::string& resp, std::string& err);
	bool UploadToDpsReport(const std::wstring& filePath, std::string& respOut, std::string& err);
	DWORD WINAPI UploadWorker(LPVOID);
	DWORD WINAPI HydrateWorker(LPVOID);
}

#pragma once

#include <string>

/* Elite Insights CLI ensure / parse (LogManagerPad internal). */
namespace LogManagerDetail
{
	void BeginEiEnsure(bool force);
	void BeginParsePending();
	void BeginParseSelected(const std::string& pathUtf8);
}

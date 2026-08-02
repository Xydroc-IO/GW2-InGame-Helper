#pragma once

#include <string>
#include <vector>

/* dps.report upload / hydrate (LogManagerPad internal). */
namespace LogManagerDetail
{
	void BeginUpload(const std::vector<std::string>& paths);
	void BeginHydrateFromReports(bool force = true);
}

#pragma once

#include "CompletionShared.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace CompletionDetail
{
	constexpr float kPadW = 440.f;
	constexpr float kPadH = 480.f;

	void ParseAchGroups(const std::string& body, std::vector<AchGroup>& out);
	void ParseAchCategories(const std::string& body, std::unordered_map<int, AchCategory>& out);
	void ParseAchDefs(const std::string& body, std::unordered_map<int, AchDef>& out);
	void ParseAchGroupsTsv(const std::string& tsv, std::vector<AchGroup>& out);
	void ParseAchCategoriesTsv(const std::string& tsv, std::unordered_map<int, AchCategory>& out);
	void ParseAchDefsTsv(const std::string& tsv, std::unordered_map<int, AchDef>& out);
}

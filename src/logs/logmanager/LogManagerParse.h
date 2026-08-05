#pragma once

#include <string>
#include <vector>

/* Pure JSON / report parsing for LogManagerPad (no ImGui). */
namespace LogManagerParse
{
	struct PlayerInfo
	{
		std::string name;
		std::string account;
		std::string profession;
		std::string guildTag; /* from name [TAG] when present */
		std::string guildId;
		int group = 0;
		int dps = 0;
		int powerDps = 0;
		int condiDps = 0;
		/* Full-fight boon uptimes (%), -1 = unknown */
		float might = -1.f;
		float fury = -1.f;
		float quickness = -1.f;
		float alacrity = -1.f;
		float protection = -1.f;
		float regeneration = -1.f;
		float swiftness = -1.f;
		float vigor = -1.f;
		/* killproof.me — -1 unknown / not loaded */
		int kpLi = -1;
		int kpLd = -1;
		int kpUfe = -1;
		int kpBoss = -1; /* encounter token amount when mapped */
		std::string kpBossLabel;
		std::string kpUrl;
		int kpState = 0; /* 0 unknown, 1 loading, 2 ok, 3 missing, 4 error */
	};

	std::string ExtractGuildTag(const std::string& name);

	const char* SkipWs(const char* p);
	bool JsonStringAfterKey(const char* json, const char* key, std::string& out);
	bool JsonBoolAfterKey(const char* json, const char* key, bool& out);
	bool JsonLongAfterKey(const char* json, const char* key, long long& out);
	bool JsonDoubleAfterKey(const char* json, const char* key, double& out);
	const char* ObjectEnd(const char* start);
	bool ExtractFirstObjectInArrayAfterKey(const char* json, const char* key, std::string& objOut);

	void FillPlayerCombatStats(const char* playerObj, PlayerInfo& pi);
	bool PlayersHaveDps(const std::vector<PlayerInfo>& players);
	bool PlayersHaveBoons(const std::vector<PlayerInfo>& players);
	bool PlayersNeedCombatStats(const std::vector<PlayerInfo>& players);

	void FillPlayerFromDpsReportObj(const char* obj, const std::string& fallbackName, PlayerInfo& pi);
	const char* FindPlayersArray(const char* json);
	void ParsePlayersFromJson(const char* json, std::vector<PlayerInfo>& out);
	void ParseDpsReportPlayers(const char* json, std::vector<PlayerInfo>& out);

	int AmountNearId(const char* json, int itemId);
}

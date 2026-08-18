/* Host-side golden tests for LogManagerParse (no Windows / ImGui).
   Build: make test-parse */
#include "LogManagerParse.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	std::string ReadFile(const char* path)
	{
		std::ifstream in(path, std::ios::binary);
		if (!in)
			return {};
		std::ostringstream ss;
		ss << in.rdbuf();
		return ss.str();
	}

	int gFails = 0;

	void Expect(bool ok, const char* msg)
	{
		if (!ok)
		{
			std::cerr << "FAIL: " << msg << "\n";
			++gFails;
		}
	}
}

int main(int argc, char** argv)
{
	const char* eiPath = "tools/fixtures/ei_players_sample.json";
	const char* dpsPath = "tools/fixtures/dpsreport_players_sample.json";
	if (argc >= 3)
	{
		eiPath = argv[1];
		dpsPath = argv[2];
	}

	const std::string ei = ReadFile(eiPath);
	Expect(!ei.empty(), "read ei_players_sample.json");

	std::string name;
	Expect(LogManagerParse::JsonStringAfterKey(ei.c_str(), "name", name) && name == "Alice [ABC]",
		"JsonStringAfterKey name Alice");
	Expect(LogManagerParse::ExtractGuildTag("Alice [ABC]") == "ABC", "ExtractGuildTag ABC");
	Expect(LogManagerParse::ExtractGuildTag("Bob").empty(), "ExtractGuildTag empty");

	std::vector<LogManagerParse::PlayerInfo> players;
	LogManagerParse::ParsePlayersFromJson(ei.c_str(), players);
	Expect(players.size() == 2, "ParsePlayersFromJson count");
	if (players.size() >= 1)
	{
		Expect(players[0].name == "Alice [ABC]", "player0 name");
		Expect(players[0].guildTag == "ABC", "player0 guildTag from name");
		Expect(players[0].account == "Alice.1234", "player0 account");
		Expect(players[0].dps == 12000, "player0 dps");
		Expect(players[0].downCount == 1, "player0 downCount");
		Expect(players[0].deadCount == 0, "player0 deadCount");
	}

	const std::string dps = ReadFile(dpsPath);
	Expect(!dps.empty(), "read dpsreport_players_sample.json");
	players.clear();
	LogManagerParse::ParseDpsReportPlayers(dps.c_str(), players);
	Expect(players.size() == 1, "ParseDpsReportPlayers count");
	if (players.size() >= 1)
	{
		Expect(players[0].name == "Carol", "dpsreport character_name");
		Expect(players[0].account == "Carol.9999", "dpsreport display_name");
	}

	if (gFails)
	{
		std::cerr << gFails << " failure(s)\n";
		return 1;
	}
	std::cout << "test_logmanager_parse: OK\n";
	return 0;
}

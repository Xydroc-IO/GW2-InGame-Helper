#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace Gw2IconsDetail
{
	enum class State : uint8_t { Unknown, Queued, Ready, Missing };
	enum class ApiKind : uint8_t { Item, Mini, Skin };

	struct Slot
	{
		State state = State::Unknown;
		std::string url;
		std::string texId;
		std::string name;
		bool uploadTried = false;
	};

	extern std::mutex gMu;
	extern std::unordered_map<int, Slot> gByItem;
	extern std::unordered_map<int, Slot> gByMini;
	extern std::unordered_map<int, Slot> gBySkin;
	extern std::unordered_map<int, Slot> gByCurrency;
	extern std::unordered_map<std::string, Slot> gByProfession;
	extern std::unordered_map<std::string, std::string> gUrlTex;
	extern std::unordered_map<std::string, std::vector<unsigned char>> gPngRetain;
	extern std::vector<int> gQueue;
	extern std::vector<int> gMiniQueue;
	extern std::vector<int> gSkinQueue;
	extern std::atomic<bool> gWorker;
	extern bool gProfessionsWarmed;

	std::string JsonStringKey(const char* json, size_t from, size_t end, const char* key);
	bool AllowedIconHost(const char* url);
	std::string MakeTexIdFromUrl(const std::string& url);
	void TryUpload(const std::string& url, const std::string& texId);
	bool DrawTex(const std::string& texId, float size);
	void QueueId(std::unordered_map<int, Slot>& map, std::vector<int>& q, int id);
	void RememberRender(std::unordered_map<int, Slot>& map, int id, const char* renderUrl);
	void WorkerMain(ApiKind kind, std::vector<int> batch);
	void PumpTick();
}

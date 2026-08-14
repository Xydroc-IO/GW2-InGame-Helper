#include "Gw2Icons.h"

#include "Globals.h"
#include "Gw2Http.h"

#include "imgui/imgui.h"
#include "nexus/Nexus.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <windows.h>

namespace
{
	enum class State : uint8_t { Unknown, Queued, Ready, Missing };

	struct Slot
	{
		State state = State::Unknown;
		std::string url;
		std::string texId;
		std::string name;
		bool uploadTried = false;
	};

	std::mutex gMu;
	enum class ApiKind : uint8_t { Item, Mini, Skin };

	std::unordered_map<int, Slot> gByItem;
	std::unordered_map<int, Slot> gByMini;
	std::unordered_map<int, Slot> gBySkin;
	std::unordered_map<int, Slot> gByCurrency; /* /v2/currencies ids — not items */
	std::unordered_map<std::string, Slot> gByProfession; /* Guardian, Warrior, … */
	std::unordered_map<std::string, std::string> gUrlTex; /* url -> texId */
	std::vector<int> gQueue;
	std::vector<int> gMiniQueue;
	std::vector<int> gSkinQueue;
	std::atomic<bool> gWorker{false};
	bool gProfessionsWarmed = false;

	std::string JsonStringKey(const char* json, size_t from, size_t end, const char* key)
	{
		if (!json || !key || from >= end)
			return {};
		char pat[48];
		std::snprintf(pat, sizeof(pat), "\"%s\"", key);
		const size_t keyLen = std::strlen(pat);
		size_t p = from;
		while (p + keyLen < end)
		{
			const char* hit = std::strstr(json + p, pat);
			if (!hit || static_cast<size_t>(hit - json) >= end)
				break;
			p = static_cast<size_t>(hit - json) + keyLen;
			while (p < end && (json[p] == ' ' || json[p] == '\t' || json[p] == ':'))
				++p;
			if (p >= end || json[p] != '"')
				continue;
			++p;
			std::string out;
			while (p < end && json[p] != '"')
			{
				if (json[p] == '\\' && p + 1 < end)
				{
					out.push_back(json[p + 1]);
					p += 2;
					continue;
				}
				out.push_back(json[p++]);
			}
			return out;
		}
		return {};
	}

	bool AllowedIconHost(const char* url)
	{
		if (!url)
			return false;
		return std::strncmp(url, "https://render.guildwars2.com/", 30) == 0 ||
			std::strncmp(url, "https://wiki.guildwars2.com/", 28) == 0;
	}

	bool SplitRenderUrl(const std::string& url, std::string& remote, std::string& endpoint)
	{
		static const char* kHosts[] = {
			"https://render.guildwars2.com",
			"https://wiki.guildwars2.com",
		};
		for (const char* host : kHosts)
		{
			const size_t n = std::strlen(host);
			if (url.rfind(host, 0) != 0)
				continue;
			if (url.size() <= n || url[n] != '/')
				return false;
			remote = host;
			endpoint = url.substr(n);
			return true;
		}
		return false;
	}

	std::string MakeTexIdFromUrl(const std::string& url)
	{
		std::string id = "GW2IGH_ICO_";
		for (char c : url)
		{
			if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
				(c >= '0' && c <= '9'))
				id += c;
			else
				id += '_';
		}
		if (id.size() > 120)
			id.resize(120);
		return id;
	}

	void TryUpload(const std::string& url, const std::string& texId)
	{
		if (!G::API || !G::API->Textures_GetOrCreateFromURL || url.empty())
			return;
		std::string remote, endpoint;
		if (!SplitRenderUrl(url, remote, endpoint))
			return;
		G::API->Textures_GetOrCreateFromURL(texId.c_str(), remote.c_str(), endpoint.c_str());
	}

	bool DrawTex(const std::string& texId, float size)
	{
		if (texId.empty() || !G::API || !G::API->Textures_Get)
			return false;
		Texture_t* tex = G::API->Textures_Get(texId.c_str());
		if (!tex || !tex->Resource)
			return false;
		ImGui::Image(reinterpret_cast<ImTextureID>(tex->Resource), ImVec2(size, size));
		return true;
	}

	std::unordered_map<int, Slot>& MapFor(ApiKind k)
	{
		if (k == ApiKind::Mini)
			return gByMini;
		if (k == ApiKind::Skin)
			return gBySkin;
		return gByItem;
	}

	const char* PathFor(ApiKind k)
	{
		if (k == ApiKind::Mini)
			return "/v2/minis?ids=";
		if (k == ApiKind::Skin)
			return "/v2/skins?ids=";
		return "/v2/items?ids=";
	}

	void QueueId(std::unordered_map<int, Slot>& map, std::vector<int>& q, int id)
	{
		if (id <= 0)
			return;
		Slot& s = map[id];
		if (s.state == State::Ready || s.state == State::Queued || s.state == State::Missing)
			return;
		s.state = State::Queued;
		q.push_back(id);
	}

	void ApplyObjects(ApiKind kind, const std::string& body)
	{
		size_t p = 0;
		while (p < body.size())
		{
			const size_t brace = body.find('{', p);
			if (brace == std::string::npos)
				break;
			size_t depth = 0;
			size_t end = brace;
			for (; end < body.size(); ++end)
			{
				if (body[end] == '{')
					++depth;
				else if (body[end] == '}')
				{
					if (--depth == 0)
					{
						++end;
						break;
					}
				}
			}
			if (depth != 0)
				break;
			long long id = 0;
			{
				const char* key = std::strstr(body.c_str() + brace, "\"id\"");
				if (key && static_cast<size_t>(key - body.c_str()) < end)
				{
					const char* colon = std::strchr(key, ':');
					if (colon && static_cast<size_t>(colon - body.c_str()) < end)
						id = std::strtoll(colon + 1, nullptr, 10);
				}
			}
			const std::string icon = JsonStringKey(body.c_str(), brace, end, "icon");
			const std::string name = JsonStringKey(body.c_str(), brace, end, "name");
			if (id > 0)
			{
				std::lock_guard<std::mutex> lock(gMu);
				Slot& s = MapFor(kind)[static_cast<int>(id)];
				if (!name.empty())
					s.name = name;
				if (!icon.empty() && icon.rfind("https://render.guildwars2.com/", 0) == 0)
				{
					s.url = icon;
					s.texId = MakeTexIdFromUrl(icon);
					s.state = State::Ready;
					gUrlTex[icon] = s.texId;
				}
				else if (!s.name.empty())
					s.state = State::Ready;
			}
			p = end;
		}
	}

	void FetchBatch(ApiKind kind, const std::vector<int>& ids)
	{
		if (ids.empty())
			return;
		std::string path = PathFor(kind);
		for (size_t i = 0; i < ids.size(); ++i)
		{
			if (i)
				path += ',';
			path += std::to_string(ids[i]);
		}
		auto r = Gw2Http::Api(path.c_str(), nullptr, 10000);
		if (r.ok && !r.body.empty())
			ApplyObjects(kind, r.body);
	}

	void WorkerMain(ApiKind kind, std::vector<int> batch)
	{
		if (batch.empty())
		{
			gWorker.store(false);
			return;
		}
		FetchBatch(kind, batch);
		std::vector<int> miss;
		{
			std::lock_guard<std::mutex> lock(gMu);
			for (int id : batch)
			{
				const Slot& s = MapFor(kind)[id];
				if (s.state == State::Queued)
					miss.push_back(id);
			}
		}
		/* Bulk /v2/items 404s if any id is hidden/unknown — retry one at a time. */
		for (int id : miss)
			FetchBatch(kind, std::vector<int>{id});
		{
			std::lock_guard<std::mutex> lock(gMu);
			auto& map = MapFor(kind);
			for (int id : batch)
			{
				Slot& s = map[id];
				if (s.state == State::Queued)
					s.state = State::Missing;
			}
		}
		gWorker.store(false);
	}
}

void Gw2Icons::RememberIcon(int id, const char* renderUrl)
{
	if (id <= 0 || !renderUrl || !renderUrl[0])
		return;
	if (std::strncmp(renderUrl, "https://render.guildwars2.com/", 30) != 0)
		return;
	std::lock_guard<std::mutex> lock(gMu);
	Slot& s = gByItem[id];
	s.url = renderUrl;
	s.texId = MakeTexIdFromUrl(s.url);
	s.state = State::Ready;
	gUrlTex[s.url] = s.texId;
	s.uploadTried = false;
}

void Gw2Icons::RememberIconFromJson(int id, const char* json, size_t brace, size_t end)
{
	if (!json || id <= 0 || brace >= end)
		return;
	const std::string icon = JsonStringKey(json, brace, end, "icon");
	if (!icon.empty())
		RememberIcon(id, icon.c_str());
}

void Gw2Icons::RequestItem(int itemId)
{
	std::lock_guard<std::mutex> lock(gMu);
	QueueId(gByItem, gQueue, itemId);
}

void Gw2Icons::RequestMini(int miniId)
{
	std::lock_guard<std::mutex> lock(gMu);
	QueueId(gByMini, gMiniQueue, miniId);
}

void Gw2Icons::RequestSkin(int skinId)
{
	std::lock_guard<std::mutex> lock(gMu);
	QueueId(gBySkin, gSkinQueue, skinId);
}

void Gw2Icons::RequestUrl(const char* renderUrl)
{
	if (!AllowedIconHost(renderUrl))
		return;
	std::string url = renderUrl;
	std::string texId;
	{
		std::lock_guard<std::mutex> lock(gMu);
		auto it = gUrlTex.find(url);
		if (it != gUrlTex.end())
			texId = it->second;
		else
		{
			texId = MakeTexIdFromUrl(url);
			gUrlTex[url] = texId;
		}
	}
	TryUpload(url, texId);
}

void Gw2Icons::Tick()
{
	/* Upload ready textures that Nexus has not seen yet. */
	std::vector<std::pair<std::string, std::string>> uploads;
	std::vector<int> batch;
	ApiKind kind = ApiKind::Item;
	{
		std::lock_guard<std::mutex> lock(gMu);
		auto enqueueReady = [&](std::unordered_map<int, Slot>& map) {
			for (auto& kv : map)
			{
				Slot& s = kv.second;
				if (s.state == State::Ready && !s.uploadTried && !s.texId.empty())
				{
					s.uploadTried = true;
					uploads.emplace_back(s.url, s.texId);
				}
			}
		};
		enqueueReady(gByItem);
		enqueueReady(gByMini);
		enqueueReady(gBySkin);
		enqueueReady(gByCurrency);
		for (auto& kv : gByProfession)
		{
			Slot& s = kv.second;
			if (s.state == State::Ready && !s.uploadTried && !s.texId.empty())
			{
				s.uploadTried = true;
				uploads.emplace_back(s.url, s.texId);
			}
		}
		if (!gWorker.load())
		{
			auto take = [&](std::vector<int>& q) {
				if (q.empty() || !batch.empty())
					return;
				const size_t n = q.size() < 50 ? q.size() : 50;
				batch.assign(q.begin(), q.begin() + static_cast<std::ptrdiff_t>(n));
				q.erase(q.begin(), q.begin() + static_cast<std::ptrdiff_t>(n));
			};
			ApiKind pick = ApiKind::Item;
			if (!gQueue.empty())
			{
				pick = ApiKind::Item;
				take(gQueue);
			}
			else if (!gMiniQueue.empty())
			{
				pick = ApiKind::Mini;
				take(gMiniQueue);
			}
			else if (!gSkinQueue.empty())
			{
				pick = ApiKind::Skin;
				take(gSkinQueue);
			}
			if (!batch.empty())
			{
				gWorker.store(true);
				kind = pick;
			}
		}
	}
	for (const auto& u : uploads)
		TryUpload(u.first, u.second);
	if (!batch.empty())
		std::thread(WorkerMain, kind, std::move(batch)).detach();
}

void Gw2Icons::RememberCurrencyIcon(int currencyId, const char* renderUrl)
{
	if (currencyId <= 0 || !renderUrl || !renderUrl[0])
		return;
	if (std::strncmp(renderUrl, "https://render.guildwars2.com/", 30) != 0)
		return;
	std::lock_guard<std::mutex> lock(gMu);
	Slot& s = gByCurrency[currencyId];
	s.url = renderUrl;
	s.texId = MakeTexIdFromUrl(s.url);
	s.state = State::Ready;
	gUrlTex[s.url] = s.texId;
	s.uploadTried = false;
}

void Gw2Icons::RememberCurrencyIconFromJson(int currencyId, const char* json, size_t brace, size_t end)
{
	if (!json || currencyId <= 0 || brace >= end)
		return;
	const std::string icon = JsonStringKey(json, brace, end, "icon");
	if (!icon.empty())
		RememberCurrencyIcon(currencyId, icon.c_str());
}

bool Gw2Icons::HasCurrencyIcon(int currencyId)
{
	if (currencyId <= 0)
		return false;
	std::lock_guard<std::mutex> lock(gMu);
	auto it = gByCurrency.find(currencyId);
	return it != gByCurrency.end() && it->second.state == State::Ready;
}

bool Gw2Icons::ImageCurrency(int currencyId, float size)
{
	if (currencyId <= 0 || size < 8.f)
		return false;
	std::string texId;
	std::string url;
	bool needUpload = false;
	{
		std::lock_guard<std::mutex> lock(gMu);
		auto it = gByCurrency.find(currencyId);
		if (it == gByCurrency.end() || it->second.state != State::Ready)
			return false;
		texId = it->second.texId;
		if (!it->second.uploadTried)
		{
			it->second.uploadTried = true;
			url = it->second.url;
			needUpload = true;
		}
	}
	if (needUpload)
		TryUpload(url, texId);
	return DrawTex(texId, size);
}

void Gw2Icons::RememberProfessionIcon(const char* professionId, const char* renderUrl)
{
	if (!professionId || !professionId[0] || !renderUrl || !renderUrl[0])
		return;
	if (std::strncmp(renderUrl, "https://render.guildwars2.com/", 30) != 0)
		return;
	std::lock_guard<std::mutex> lock(gMu);
	Slot& s = gByProfession[professionId];
	s.url = renderUrl;
	s.texId = MakeTexIdFromUrl(s.url);
	s.state = State::Ready;
	gUrlTex[s.url] = s.texId;
	s.uploadTried = false;
}

void Gw2Icons::WarmProfessionIcons()
{
	/* Official /v2/professions icon URLs — baked so roster is immersive immediately. */
	static const char* kPairs[][2] = {
		{ "Guardian", "https://render.guildwars2.com/file/C32BE61FC55C962524624F643897ECF1A9C80462/156634.png" },
		{ "Warrior", "https://render.guildwars2.com/file/0A97E13F29B3597A447EEC04A09BE5BD699A2250/156643.png" },
		{ "Engineer", "https://render.guildwars2.com/file/5CCB361F44CCC7256132405D31E3A24DACCF440A/156632.png" },
		{ "Ranger", "https://render.guildwars2.com/file/49B10316B424F4E20139EB5E51ADCF24A8724E9B/156640.png" },
		{ "Thief", "https://render.guildwars2.com/file/F9EC00E23F630D6DB20CDA985592EC010E2A5705/156641.png" },
		{ "Elementalist", "https://render.guildwars2.com/file/77B793123251931AFF9FCA24C07E0F704BC4DA49/156630.png" },
		{ "Mesmer", "https://render.guildwars2.com/file/E43730AD49A903C3A1B4F27E41DE04EA51A775EC/156636.png" },
		{ "Necromancer", "https://render.guildwars2.com/file/AE56F8670807B87CF6EEE3FC7E6CB9710959E004/156638.png" },
		{ "Revenant", "https://render.guildwars2.com/file/7C9309BE7A2A48C6A9FBCC70CC1EBEBFD7593C05/961390.png" },
	};
	bool need = false;
	{
		std::lock_guard<std::mutex> lock(gMu);
		need = !gProfessionsWarmed;
		if (need)
			gProfessionsWarmed = true;
	}
	if (!need)
		return;
	for (const auto& pair : kPairs)
		RememberProfessionIcon(pair[0], pair[1]);
}

bool Gw2Icons::ImageProfession(const char* professionId, float size)
{
	if (!professionId || !professionId[0] || size < 8.f)
		return false;
	WarmProfessionIcons();
	std::string texId;
	std::string url;
	bool needUpload = false;
	{
		std::lock_guard<std::mutex> lock(gMu);
		auto it = gByProfession.find(professionId);
		if (it == gByProfession.end() || it->second.state != State::Ready)
			return false;
		texId = it->second.texId;
		if (!it->second.uploadTried)
		{
			it->second.uploadTried = true;
			url = it->second.url;
			needUpload = true;
		}
	}
	if (needUpload)
		TryUpload(url, texId);
	return DrawTex(texId, size);
}

bool Gw2Icons::Image(int id, float size)
{
	if (id <= 0 || size < 8.f)
		return false;
	std::string texId;
	std::string url;
	bool needUpload = false;
	{
		std::lock_guard<std::mutex> lock(gMu);
		auto it = gByItem.find(id);
		if (it == gByItem.end() || it->second.state != State::Ready)
			return false;
		texId = it->second.texId;
		if (!it->second.uploadTried)
		{
			it->second.uploadTried = true;
			url = it->second.url;
			needUpload = true;
		}
	}
	if (needUpload)
		TryUpload(url, texId);
	return DrawTex(texId, size);
}

bool Gw2Icons::ImageItem(int itemId, float size)
{
	RequestItem(itemId);
	return Image(itemId, size);
}

bool Gw2Icons::ItemName(int itemId, char* out, size_t outLen)
{
	if (!out || outLen == 0 || itemId <= 0)
		return false;
	out[0] = '\0';
	RequestItem(itemId);
	std::lock_guard<std::mutex> lock(gMu);
	const auto it = gByItem.find(itemId);
	if (it == gByItem.end() || it->second.name.empty())
		return false;
	std::snprintf(out, outLen, "%s", it->second.name.c_str());
	return true;
}

namespace
{
	bool DrawFromMap(std::unordered_map<int, Slot>& map, int id, float size)
	{
		if (id <= 0 || size < 8.f)
			return false;
		std::string texId;
		std::string url;
		bool needUpload = false;
		{
			std::lock_guard<std::mutex> lock(gMu);
			auto it = map.find(id);
			if (it == map.end() || it->second.state != State::Ready || it->second.texId.empty())
				return false;
			texId = it->second.texId;
			if (!it->second.uploadTried)
			{
				it->second.uploadTried = true;
				url = it->second.url;
				needUpload = true;
			}
		}
		if (needUpload)
			TryUpload(url, texId);
		return DrawTex(texId, size);
	}

	bool NameFromMap(std::unordered_map<int, Slot>& map, int id, char* out, size_t outLen)
	{
		if (!out || outLen == 0 || id <= 0)
			return false;
		out[0] = '\0';
		std::lock_guard<std::mutex> lock(gMu);
		const auto it = map.find(id);
		if (it == map.end() || it->second.name.empty())
			return false;
		std::snprintf(out, outLen, "%s", it->second.name.c_str());
		return true;
	}
}

bool Gw2Icons::ImageMini(int miniId, float size)
{
	RequestMini(miniId);
	return DrawFromMap(gByMini, miniId, size);
}

bool Gw2Icons::MiniName(int miniId, char* out, size_t outLen)
{
	RequestMini(miniId);
	return NameFromMap(gByMini, miniId, out, outLen);
}

bool Gw2Icons::ImageSkin(int skinId, float size)
{
	RequestSkin(skinId);
	return DrawFromMap(gBySkin, skinId, size);
}

bool Gw2Icons::SkinName(int skinId, char* out, size_t outLen)
{
	RequestSkin(skinId);
	return NameFromMap(gBySkin, skinId, out, outLen);
}

bool Gw2Icons::ImageUrl(const char* renderUrl, float size)
{
	if (!renderUrl || size < 8.f)
		return false;
	RequestUrl(renderUrl);
	std::string texId;
	{
		std::lock_guard<std::mutex> lock(gMu);
		auto it = gUrlTex.find(renderUrl);
		if (it == gUrlTex.end())
			return false;
		texId = it->second;
	}
	return DrawTex(texId, size);
}

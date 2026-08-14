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
	std::unordered_map<int, Slot> gByItem;
	std::unordered_map<int, Slot> gByCurrency; /* /v2/currencies ids — not items */
	std::unordered_map<std::string, Slot> gByProfession; /* Guardian, Warrior, … */
	std::unordered_map<std::string, std::string> gUrlTex; /* url -> texId */
	std::vector<int> gQueue;
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

	void WorkerMain(std::vector<int> batch)
	{
		if (batch.empty())
		{
			gWorker.store(false);
			return;
		}
		std::string path = "/v2/items?ids=";
		for (size_t i = 0; i < batch.size(); ++i)
		{
			if (i)
				path += ',';
			path += std::to_string(batch[i]);
		}
		auto r = Gw2Http::Api(path.c_str(), nullptr, 10000);
		if (r.ok && !r.body.empty())
		{
			size_t p = 0;
			while (p < r.body.size())
			{
				const size_t brace = r.body.find('{', p);
				if (brace == std::string::npos)
					break;
				size_t depth = 0;
				size_t end = brace;
				for (; end < r.body.size(); ++end)
				{
					if (r.body[end] == '{')
						++depth;
					else if (r.body[end] == '}')
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
					const char* key = std::strstr(r.body.c_str() + brace, "\"id\"");
					if (key && static_cast<size_t>(key - r.body.c_str()) < end)
					{
						const char* colon = std::strchr(key, ':');
						if (colon && static_cast<size_t>(colon - r.body.c_str()) < end)
							id = std::strtoll(colon + 1, nullptr, 10);
					}
				}
				const std::string icon = JsonStringKey(r.body.c_str(), brace, end, "icon");
				const std::string name = JsonStringKey(r.body.c_str(), brace, end, "name");
				if (id > 0)
				{
					std::lock_guard<std::mutex> lock(gMu);
					Slot& s = gByItem[static_cast<int>(id)];
					if (!name.empty())
						s.name = name;
					if (!icon.empty() && icon.rfind("https://render.guildwars2.com/", 0) == 0)
					{
						s.url = icon;
						s.texId = MakeTexIdFromUrl(icon);
						s.state = State::Ready;
						gUrlTex[icon] = s.texId;
					}
					else
						s.state = State::Missing;
				}
				p = end;
			}
		}
		/* Mark unresolved batch entries missing so we don't spin. */
		{
			std::lock_guard<std::mutex> lock(gMu);
			for (int id : batch)
			{
				Slot& s = gByItem[id];
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
	if (itemId <= 0)
		return;
	std::lock_guard<std::mutex> lock(gMu);
	Slot& s = gByItem[itemId];
	if (s.state == State::Ready || s.state == State::Queued || s.state == State::Missing)
		return;
	s.state = State::Queued;
	gQueue.push_back(itemId);
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
		if (!gWorker.load() && !gQueue.empty())
		{
			const size_t n = gQueue.size() < 50 ? gQueue.size() : 50;
			batch.assign(gQueue.begin(), gQueue.begin() + static_cast<std::ptrdiff_t>(n));
			gQueue.erase(gQueue.begin(), gQueue.begin() + static_cast<std::ptrdiff_t>(n));
			gWorker.store(true);
		}
	}
	for (const auto& u : uploads)
		TryUpload(u.first, u.second);
	if (!batch.empty())
	{
		std::thread(WorkerMain, std::move(batch)).detach();
	}
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

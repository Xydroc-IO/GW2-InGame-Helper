#include "Gw2IconsInternal.h"

#include "Globals.h"
#include "Gw2Catalog.h"
#include "Gw2Http.h"

#include "imgui/imgui.h"
#include "nexus/Nexus.h"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <thread>

namespace Gw2IconsDetail
{
	std::mutex gMu;
	std::unordered_map<int, Slot> gByItem;
	std::unordered_map<int, Slot> gByMini;
	std::unordered_map<int, Slot> gBySkin;
	std::unordered_map<int, Slot> gByCurrency;
	std::unordered_map<std::string, Slot> gByProfession;
	std::unordered_map<std::string, std::string> gUrlTex;
	std::unordered_map<std::string, std::vector<unsigned char>> gPngRetain;
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
		if (!G::API || url.empty() || texId.empty())
			return;
		std::vector<unsigned char> png;
		if (Gw2Catalog::IconPng(url.c_str(), &png) && !png.empty() &&
			G::API->Textures_GetOrCreateFromMemory)
		{
			void* data = nullptr;
			uint64_t sz = 0;
			{
				std::lock_guard<std::mutex> lock(gMu);
				auto& slot = gPngRetain[texId];
				slot = std::move(png);
				data = slot.data();
				sz = slot.size();
			}
			G::API->Textures_GetOrCreateFromMemory(texId.c_str(), data, sz);
			return;
		}
		if (!G::API->Textures_GetOrCreateFromURL)
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

	void RememberRender(std::unordered_map<int, Slot>& map, int id, const char* renderUrl)
	{
		if (id <= 0 || !renderUrl || !renderUrl[0])
			return;
		if (std::strncmp(renderUrl, "https://render.guildwars2.com/", 30) != 0)
			return;
		std::lock_guard<std::mutex> lock(gMu);
		Slot& s = map[id];
		s.url = renderUrl;
		s.texId = MakeTexIdFromUrl(s.url);
		s.state = State::Ready;
		gUrlTex[s.url] = s.texId;
		s.uploadTried = false;
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

	void PumpTick()
	{
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
				if (!gQueue.empty())
				{
					kind = ApiKind::Item;
					take(gQueue);
				}
				else if (!gMiniQueue.empty())
				{
					kind = ApiKind::Mini;
					take(gMiniQueue);
				}
				else if (!gSkinQueue.empty())
				{
					kind = ApiKind::Skin;
					take(gSkinQueue);
				}
				if (!batch.empty())
					gWorker.store(true);
			}
		}
		for (const auto& u : uploads)
			TryUpload(u.first, u.second);
		if (G::API && G::API->Textures_Get && !gPngRetain.empty())
		{
			std::vector<std::string> ids;
			{
				std::lock_guard<std::mutex> lock(gMu);
				ids.reserve(gPngRetain.size());
				for (const auto& kv : gPngRetain)
					ids.push_back(kv.first);
			}
			for (const std::string& id : ids)
			{
				Texture_t* tex = G::API->Textures_Get(id.c_str());
				if (!tex || !tex->Resource)
					continue;
				std::lock_guard<std::mutex> lock(gMu);
				gPngRetain.erase(id);
			}
		}
		if (!batch.empty())
			std::thread(WorkerMain, kind, std::move(batch)).detach();
	}
}

#include "Gw2Icons.h"

#include "Gw2IconsInternal.h"

#include "Gw2Catalog.h"

#include <cstdio>
#include <cstring>

using namespace Gw2IconsDetail;

void Gw2Icons::RememberIcon(int id, const char* renderUrl)
{
	RememberRender(gByItem, id, renderUrl);
}

void Gw2Icons::RememberIconFromJson(int id, const char* json, size_t brace, size_t end)
{
	if (!json || id <= 0 || brace >= end)
		return;
	const std::string icon = JsonStringKey(json, brace, end, "icon");
	if (!icon.empty())
		RememberIcon(id, icon.c_str());
}

static void RequestCatalog(int id,
	bool (*nameFn)(int, std::string*),
	bool (*iconFn)(int, std::string*),
	std::unordered_map<int, Slot>& map,
	std::vector<int>& queue)
{
	if (id <= 0)
		return;
	std::string name;
	std::string icon;
	nameFn(id, &name);
	iconFn(id, &icon);
	if (!icon.empty())
		RememberRender(map, id, icon.c_str());
	if (!name.empty())
	{
		std::lock_guard<std::mutex> lock(gMu);
		map[id].name = name;
	}
	if (!icon.empty())
		return;
	std::lock_guard<std::mutex> lock(gMu);
	QueueId(map, queue, id);
}

void Gw2Icons::RequestItem(int itemId)
{
	RequestCatalog(itemId, Gw2Catalog::ItemName, Gw2Catalog::ItemIcon, gByItem, gQueue);
}

void Gw2Icons::RequestMini(int miniId)
{
	RequestCatalog(miniId, Gw2Catalog::MiniName, Gw2Catalog::MiniIcon, gByMini, gMiniQueue);
}

void Gw2Icons::RequestSkin(int skinId)
{
	RequestCatalog(skinId, Gw2Catalog::SkinName, Gw2Catalog::SkinIcon, gBySkin, gSkinQueue);
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
	PumpTick();
}

void Gw2Icons::RememberCurrencyIcon(int currencyId, const char* renderUrl)
{
	RememberRender(gByCurrency, currencyId, renderUrl);
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
	{
		std::lock_guard<std::mutex> lock(gMu);
		auto it = gByCurrency.find(currencyId);
		if (it != gByCurrency.end() && it->second.state == State::Ready)
			return true;
	}
	std::string icon;
	if (Gw2Catalog::CurrencyIcon(currencyId, &icon))
	{
		RememberCurrencyIcon(currencyId, icon.c_str());
		return true;
	}
	return false;
}

static bool DrawReady(std::unordered_map<int, Slot>& map, int id, float size)
{
	if (id <= 0 || size < 8.f)
		return false;
	std::string texId;
	std::string url;
	bool needUpload = false;
	{
		std::lock_guard<std::mutex> lock(gMu);
		auto it = map.find(id);
		if (it == map.end() || it->second.state != State::Ready)
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

bool Gw2Icons::ImageCurrency(int currencyId, float size)
{
	std::string catIcon;
	if (Gw2Catalog::CurrencyIcon(currencyId, &catIcon))
		RememberCurrencyIcon(currencyId, catIcon.c_str());
	return DrawReady(gByCurrency, currencyId, size);
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
	return DrawReady(gByItem, id, size);
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
	std::string cat;
	if (Gw2Catalog::ItemName(itemId, &cat))
	{
		std::snprintf(out, outLen, "%s", cat.c_str());
		RequestItem(itemId);
		return true;
	}
	RequestItem(itemId);
	std::lock_guard<std::mutex> lock(gMu);
	const auto it = gByItem.find(itemId);
	if (it == gByItem.end() || it->second.name.empty())
		return false;
	std::snprintf(out, outLen, "%s", it->second.name.c_str());
	return true;
}

static bool NameFromMap(std::unordered_map<int, Slot>& map, int id, char* out, size_t outLen)
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

bool Gw2Icons::ImageMini(int miniId, float size)
{
	RequestMini(miniId);
	return DrawReady(gByMini, miniId, size);
}

bool Gw2Icons::MiniName(int miniId, char* out, size_t outLen)
{
	std::string cat;
	if (Gw2Catalog::MiniName(miniId, &cat) && out && outLen)
	{
		std::snprintf(out, outLen, "%s", cat.c_str());
		RequestMini(miniId);
		return true;
	}
	RequestMini(miniId);
	return NameFromMap(gByMini, miniId, out, outLen);
}

bool Gw2Icons::ImageSkin(int skinId, float size)
{
	RequestSkin(skinId);
	return DrawReady(gBySkin, skinId, size);
}

bool Gw2Icons::SkinName(int skinId, char* out, size_t outLen)
{
	std::string cat;
	if (Gw2Catalog::SkinName(skinId, &cat) && out && outLen)
	{
		std::snprintf(out, outLen, "%s", cat.c_str());
		RequestSkin(skinId);
		return true;
	}
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

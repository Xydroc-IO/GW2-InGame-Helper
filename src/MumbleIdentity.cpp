#include "MumbleIdentity.h"

#include "Globals.h"

#include <cstring>
#include <string>

#include <windows.h>

namespace
{
	std::string gName;
	std::string gPrevName;
	bool gChanged = false;

	std::string WideToUtf8(const wchar_t* w, size_t maxChars)
	{
		if (!w || !w[0])
			return {};
		size_t n = 0;
		while (n < maxChars && w[n])
			++n;
		if (n == 0)
			return {};
		const int need = WideCharToMultiByte(CP_UTF8, 0, w, static_cast<int>(n),
			nullptr, 0, nullptr, nullptr);
		if (need <= 0)
			return {};
		std::string out(static_cast<size_t>(need), '\0');
		WideCharToMultiByte(CP_UTF8, 0, w, static_cast<int>(n),
			out.data(), need, nullptr, nullptr);
		return out;
	}

	/* Identity is compact JSON: {"name":"Foo","profession":…,"fov":…} */
	std::string ParseNameField(const std::string& id)
	{
		const char* key = "\"name\"";
		size_t k = id.find(key);
		if (k == std::string::npos)
			return {};
		k = id.find(':', k + 6);
		if (k == std::string::npos)
			return {};
		++k;
		while (k < id.size() && (id[k] == ' ' || id[k] == '\t'))
			++k;
		if (k >= id.size() || id[k] != '"')
			return {};
		++k;
		std::string out;
		while (k < id.size())
		{
			const char c = id[k++];
			if (c == '\\' && k < id.size())
			{
				out.push_back(id[k++]);
				continue;
			}
			if (c == '"')
				break;
			out.push_back(c);
		}
		return out;
	}
}

void MumbleIdentity::Tick()
{
	std::string next;
	if (G::Mumble && G::Mumble->uiTick != 0)
		next = ParseNameField(WideToUtf8(G::Mumble->identity, 256));

	if (next == gName)
		return;

	gPrevName = gName;
	gName = std::move(next);
	gChanged = true;
}

const char* MumbleIdentity::CharacterName()
{
	return gName.c_str();
}

std::string MumbleIdentity::CharacterNameStr()
{
	return gName;
}

bool MumbleIdentity::TakeChanged()
{
	if (!gChanged)
		return false;
	gChanged = false;
	return true;
}

#include "VaultPadInternal.h"

#include "AddonPaths.h"
#include "BgFetch.h"
#include "Globals.h"
#include "Gw2Http.h"
#include "Settings.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include <windows.h>

namespace VaultDetail
{
	std::wstring StemJson(const std::wstring& dir, const char* stem)
	{
		std::wstring p = dir + L"\\";
		for (const char* s = stem; *s; ++s)
			p.push_back(static_cast<wchar_t>(*s));
		p += L".json";
		return p;
	}

	bool FileFresh(const std::wstring& path, DWORD ttlSec)
	{
		WIN32_FILE_ATTRIBUTE_DATA fad{};
		if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad))
			return false;
		ULARGE_INTEGER wr{};
		wr.LowPart = fad.ftLastWriteTime.dwLowDateTime;
		wr.HighPart = fad.ftLastWriteTime.dwHighDateTime;
		FILETIME nowFt{};
		GetSystemTimeAsFileTime(&nowFt);
		ULARGE_INTEGER now{};
		now.LowPart = nowFt.dwLowDateTime;
		now.HighPart = nowFt.dwHighDateTime;
		const ULONGLONG age100ns = now.QuadPart - wr.QuadPart;
		const ULONGLONG ttl100ns = static_cast<ULONGLONG>(ttlSec) * 10000000ull;
		return age100ns <= ttl100ns;
	}

	std::string ReadUtf8File(const std::wstring& path)
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE) return {};
		LARGE_INTEGER sz{};
		if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > 8 * 1024 * 1024)
		{
			CloseHandle(h);
			return {};
		}
		std::string out(static_cast<size_t>(sz.QuadPart), '\0');
		DWORD rd = 0;
		ReadFile(h, out.data(), static_cast<DWORD>(out.size()), &rd, nullptr);
		CloseHandle(h);
		out.resize(rd);
		return out;
	}

	bool TryLiveCache(const std::wstring& dir, const char* stem, DWORD ttlSec, Gw2Http::Result& out)
	{
		const std::wstring path = StemJson(dir, stem);
		if (!FileFresh(path, ttlSec)) return false;
		std::string body = ReadUtf8File(path);
		if (body.empty()) return false;
		out.ok = true;
		out.status = 200;
		out.body = std::move(body);
		return true;
	}

	struct FetchPack
	{
		Gw2Http::Result season, vd, vw, vs, pub;
		const char* key = nullptr;
		std::wstring dir;
	};

	DWORD WINAPI FetchSeason(void* p)
	{
		FetchPack* f = static_cast<FetchPack*>(p);
		if (!TryLiveCache(f->dir, "live-season", 6 * 3600, f->season))
			f->season = Gw2Http::Api("/v2/wizardsvault", nullptr, kHttpTimeoutMs);
		return 0;
	}
	DWORD WINAPI FetchVd(void* p)
	{
		FetchPack* f = static_cast<FetchPack*>(p);
		if (!f->key || !f->key[0]) return 0;
		if (!TryLiveCache(f->dir, "live-vault-daily", 180, f->vd))
			f->vd = Gw2Http::Api("/v2/account/wizardsvault/daily", f->key, kHttpTimeoutMs);
		return 0;
	}
	DWORD WINAPI FetchVw(void* p)
	{
		FetchPack* f = static_cast<FetchPack*>(p);
		if (!f->key || !f->key[0]) return 0;
		if (!TryLiveCache(f->dir, "live-vault-weekly", 180, f->vw))
			f->vw = Gw2Http::Api("/v2/account/wizardsvault/weekly", f->key, kHttpTimeoutMs);
		return 0;
	}
	DWORD WINAPI FetchVs(void* p)
	{
		FetchPack* f = static_cast<FetchPack*>(p);
		if (!f->key || !f->key[0]) return 0;
		if (!TryLiveCache(f->dir, "live-vault-special", 180, f->vs))
			f->vs = Gw2Http::Api("/v2/account/wizardsvault/special", f->key, kHttpTimeoutMs);
		return 0;
	}
	DWORD WINAPI FetchPub(void* p)
	{
		FetchPack* f = static_cast<FetchPack*>(p);
		if (f->key && f->key[0]) return 0;
		if (!TryLiveCache(f->dir, "live-vault-obj", 3600, f->pub))
			f->pub = Gw2Http::Api("/v2/wizardsvault/objectives?ids=all", nullptr, kBulkTimeoutMs);
		return 0;
	}

	void TickDeferredFetch()
	{
		if (!gDeferredFetch.load())
			return;
		if (!BgFetch::AllowWork(BgFetch::Channel::Vault))
			return;
		const bool force = gDeferredForce.load();
		gDeferredFetch = false;
		StartFetch(force);
	}

	DWORD WINAPI MasterProc(void*)
	{
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
		struct UnpinIfClosed
		{
			~UnpinIfClosed()
			{
				if (!G::ShowVault)
					BgFetch::SetWanted(BgFetch::Channel::Vault, false);
			}
		} unpin;
		while (!BgFetch::AllowWork(BgFetch::Channel::Vault))
		{
			if (!BgFetch::Wanted(BgFetch::Channel::Vault))
			{
				gBusy = false;
				return 0;
			}
			Sleep(40);
		}
		Snapshot snap;
		snap.hasKey = G::Gw2ApiKey[0] != 0;

		FetchPack pack;
		pack.key = G::Gw2ApiKey;
		pack.dir = AddonPaths::DataDir();

		HANDLE th[5]{};
		th[0] = CreateThread(nullptr, 0, FetchSeason, &pack, 0, nullptr);
		th[1] = CreateThread(nullptr, 0, FetchVd, &pack, 0, nullptr);
		th[2] = CreateThread(nullptr, 0, FetchVw, &pack, 0, nullptr);
		th[3] = CreateThread(nullptr, 0, FetchVs, &pack, 0, nullptr);
		th[4] = CreateThread(nullptr, 0, FetchPub, &pack, 0, nullptr);
		for (HANDLE h : th)
		{
			if (h)
			{
				WaitForSingleObject(h, 20000);
				CloseHandle(h);
			}
		}

		if (pack.season.ok)
		{
			snap.seasonTitle = JsonStringAfterKey(pack.season.body, "title");
			if (snap.seasonTitle.empty())
				snap.seasonTitle = "Wizard's Vault";
			snap.seasonBlurb = SeasonBlurb(
				JsonStringAfterKey(pack.season.body, "start"),
				JsonStringAfterKey(pack.season.body, "end"));
		}
		else
		{
			snap.seasonTitle = "Wizard's Vault";
			snap.seasonBlurb = pack.season.error.empty() ? "Season fetch failed." : pack.season.error;
		}

		if (snap.hasKey)
		{
			if (pack.vd.ok)
				ParseVaultObjs(pack.vd.body, snap.daily);
			else if (pack.vd.status == 401 || pack.vd.status == 403)
				snap.scopeFail = true;
			if (pack.vw.ok)
				ParseVaultObjs(pack.vw.body, snap.weekly);
			if (pack.vs.ok)
				ParseVaultObjs(pack.vs.body, snap.special);
		}
		else if (pack.pub.ok)
			ParseVaultObjs(pack.pub.body, snap.easyPreview, 40, 10);

		snap.ok = true;
		snap.fetchedAt = GetTickCount();
		char st[160];
		if (snap.scopeFail)
			std::snprintf(st, sizeof(st), "Need account + progression scopes.");
		else if (snap.hasKey)
			std::snprintf(st, sizeof(st), "Daily %d | Weekly %d | Special %d",
				static_cast<int>(snap.daily.size()),
				static_cast<int>(snap.weekly.size()),
				static_cast<int>(snap.special.size()));
		else
			std::snprintf(st, sizeof(st), "Public preview - add API key for live Vault.");
		snap.status = st;

		{
			std::lock_guard<std::mutex> lock(gMu);
			gSnap = std::move(snap);
			gGen.fetch_add(1);
		}
		gBusy = false;
		return 0;
	}

	void StartFetch(bool force)
	{
		BgFetch::SetWanted(BgFetch::Channel::Vault, true);
		if (!BgFetch::AllowWork(BgFetch::Channel::Vault))
		{
			gDeferredFetch = true;
			gDeferredForce = force;
			return;
		}
		if (!force)
		{
			std::lock_guard<std::mutex> lock(gMu);
			if (gSnap.ok && gSnap.fetchedAt != 0 &&
				(GetTickCount() - gSnap.fetchedAt) < kCacheTtlMs)
				return;
		}
		gDeferredFetch = false;
		if (gBusy.exchange(true))
			return;
		if (gThread)
		{
			if (WaitForSingleObject(gThread, 0) == WAIT_OBJECT_0)
			{
				CloseHandle(gThread);
				gThread = nullptr;
			}
			else
			{
				gBusy = false;
				return;
			}
		}
		{
			std::lock_guard<std::mutex> lock(gMu);
			gSnap.status = gSnap.ok ? "Refreshing..." : "Loading...";
			gGen.fetch_add(1);
		}
		gThread = CreateThread(nullptr, 0, MasterProc, nullptr, 0, nullptr);
		if (!gThread)
		{
			gBusy = false;
			std::lock_guard<std::mutex> lock(gMu);
			gSnap.status = "Could not start fetch.";
			gGen.fetch_add(1);
		}
	}
} // namespace VaultDetail

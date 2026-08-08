#include "TpWatchPad.h"

#include "TpWatchShared.h"

#include "Globals.h"
#include "Settings.h"

#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <windows.h>

namespace TpWatchDetail
{
	std::string FormatCoins(long long copper)
	{
		if (copper < 0) copper = 0;
		const long long g = copper / 10000;
		const long long s = (copper % 10000) / 100;
		const long long c = copper % 100;
		char buf[64];
		if (g > 0)
			std::snprintf(buf, sizeof(buf), "%lldg %02llds %02lldc", g, s, c);
		else if (s > 0)
			std::snprintf(buf, sizeof(buf), "%llds %02lldc", s, c);
		else
			std::snprintf(buf, sizeof(buf), "%lldc", c);
		return buf;
	}

	/* Compact for the alert field: "5g", "50s", "12c", or "1g 50s". */
	void FormatAlertEdit(long long copper, char* out, size_t outLen)
	{
		if (!out || outLen == 0) return;
		out[0] = 0;
		if (copper <= 0) return;
		const long long g = copper / 10000;
		const long long s = (copper % 10000) / 100;
		const long long c = copper % 100;
		if (g > 0 && s == 0 && c == 0)
			std::snprintf(out, outLen, "%lldg", g);
		else if (g > 0 && c == 0)
			std::snprintf(out, outLen, "%lldg %llds", g, s);
		else if (g > 0)
			std::snprintf(out, outLen, "%lldg %llds %lldc", g, s, c);
		else if (s > 0 && c == 0)
			std::snprintf(out, outLen, "%llds", s);
		else if (s > 0)
			std::snprintf(out, outLen, "%llds %lldc", s, c);
		else
			std::snprintf(out, outLen, "%lldc", c);
	}

	/* "5g", "50s", "1g 20s", "12345" (copper). Empty / junk -> 0. */
	long long ParseCoinsInput(const char* text)
	{
		if (!text) return 0;
		long long total = 0;
		bool anyUnit = false;
		const char* p = text;
		while (*p)
		{
			while (*p == ' ' || *p == '\t' || *p == ',' || *p == '+') ++p;
			if (!*p) break;
			long long v = 0;
			bool digits = false;
			while (*p >= '0' && *p <= '9')
			{
				digits = true;
				v = v * 10 + (*p - '0');
				++p;
			}
			if (!digits) break;
			char u = *p;
			if (u >= 'A' && u <= 'Z') u = static_cast<char>(u - 'A' + 'a');
			if (u == 'g')
			{
				anyUnit = true;
				total += v * 10000;
				++p;
			}
			else if (u == 's')
			{
				anyUnit = true;
				total += v * 100;
				++p;
			}
			else if (u == 'c')
			{
				anyUnit = true;
				total += v;
				++p;
			}
			else
			{
				/* Bare number: copper if alone, otherwise stop. */
				if (!anyUnit && total == 0)
					return v;
				break;
			}
		}
		return total > 0 ? total : 0;
	}

	void ParseIds(const char* csv, std::vector<int>& out)
	{
		out.clear();
		if (!csv) return;
		const char* p = csv;
		while (*p && out.size() < static_cast<size_t>(kMaxItems))
		{
			while (*p == ' ' || *p == ',' || *p == ';' || *p == '\t') ++p;
			if (!*p) break;
			int v = 0;
			bool any = false;
			while (*p >= '0' && *p <= '9')
			{
				any = true;
				v = v * 10 + (*p - '0');
				++p;
			}
			if (any && v > 0)
			{
				bool dup = false;
				for (int x : out) if (x == v) { dup = true; break; }
				if (!dup) out.push_back(v);
			}
			while (*p && *p != ',' && *p != ';' && !(*p >= '0' && *p <= '9')) ++p;
		}
	}

	void ParseAlerts(const char* csv, std::vector<std::pair<int, long long>>& out)
	{
		out.clear();
		if (!csv) return;
		const char* p = csv;
		while (*p && out.size() < static_cast<size_t>(kMaxItems))
		{
			while (*p == ' ' || *p == ',' || *p == ';' || *p == '\t') ++p;
			if (!*p) break;
			int id = 0;
			bool anyId = false;
			while (*p >= '0' && *p <= '9')
			{
				anyId = true;
				id = id * 10 + (*p - '0');
				++p;
			}
			long long thresh = 0;
			if (anyId && *p == ':')
			{
				++p;
				bool anyT = false;
				while (*p >= '0' && *p <= '9')
				{
					anyT = true;
					thresh = thresh * 10 + (*p - '0');
					++p;
				}
				if (!anyT) thresh = 0;
			}
			if (anyId && id > 0 && thresh > 0)
			{
				bool dup = false;
				for (auto& e : out)
				{
					if (e.first == id)
					{
						e.second = thresh;
						dup = true;
						break;
					}
				}
				if (!dup) out.emplace_back(id, thresh);
			}
			while (*p && *p != ',' && *p != ';') ++p;
		}
	}

	void SaveAlerts(const std::vector<std::pair<int, long long>>& alerts)
	{
		std::string s;
		for (size_t i = 0; i < alerts.size(); ++i)
		{
			if (alerts[i].second <= 0) continue;
			if (!s.empty()) s += ',';
			s += std::to_string(alerts[i].first);
			s += ':';
			s += std::to_string(alerts[i].second);
		}
		if (s.size() >= sizeof(G::TpWatchAlerts))
			s.resize(sizeof(G::TpWatchAlerts) - 1);
		std::snprintf(G::TpWatchAlerts, sizeof(G::TpWatchAlerts), "%s", s.c_str());
		Settings::SetDirty();
	}

	void ParseBuyAlerts(const char* csv, std::vector<std::pair<int, long long>>& out)
	{
		ParseAlerts(csv, out);
	}

	void SaveBuyAlerts(const std::vector<std::pair<int, long long>>& alerts)
	{
		std::string s;
		for (size_t i = 0; i < alerts.size(); ++i)
		{
			if (alerts[i].second <= 0) continue;
			if (!s.empty()) s += ',';
			s += std::to_string(alerts[i].first);
			s += ':';
			s += std::to_string(alerts[i].second);
		}
		if (s.size() >= sizeof(G::TpWatchBuyAlerts))
			s.resize(sizeof(G::TpWatchBuyAlerts) - 1);
		std::snprintf(G::TpWatchBuyAlerts, sizeof(G::TpWatchBuyAlerts), "%s", s.c_str());
		Settings::SetDirty();
	}

	void SetBuyAlertForId(int id, long long thresh)
	{
		if (id <= 0) return;
		std::vector<std::pair<int, long long>> alerts;
		ParseBuyAlerts(G::TpWatchBuyAlerts, alerts);
		bool found = false;
		for (size_t i = 0; i < alerts.size(); ++i)
		{
			if (alerts[i].first != id) continue;
			found = true;
			if (thresh <= 0)
				alerts.erase(alerts.begin() + static_cast<std::ptrdiff_t>(i));
			else
				alerts[i].second = thresh;
			break;
		}
		if (!found && thresh > 0)
			alerts.emplace_back(id, thresh);
		SaveBuyAlerts(alerts);
	}

	void SetAlertForId(int id, long long thresh)
	{
		if (id <= 0) return;
		std::vector<std::pair<int, long long>> alerts;
		ParseAlerts(G::TpWatchAlerts, alerts);
		bool found = false;
		for (size_t i = 0; i < alerts.size(); ++i)
		{
			if (alerts[i].first != id) continue;
			found = true;
			if (thresh <= 0)
				alerts.erase(alerts.begin() + static_cast<std::ptrdiff_t>(i));
			else
				alerts[i].second = thresh;
			break;
		}
		if (!found && thresh > 0)
			alerts.emplace_back(id, thresh);
		SaveAlerts(alerts);
	}

	void PruneAlertsToIds(const std::vector<int>& ids)
	{
		auto prune = [&](const char* src, void (*save)(const std::vector<std::pair<int, long long>>&)) {
			std::vector<std::pair<int, long long>> alerts;
			ParseAlerts(src, alerts);
			std::vector<std::pair<int, long long>> next;
			for (const auto& e : alerts)
			{
				bool keep = false;
				for (int id : ids) if (id == e.first) { keep = true; break; }
				if (keep) next.push_back(e);
			}
			if (next.size() != alerts.size())
				save(next);
		};
		prune(G::TpWatchAlerts, SaveAlerts);
		prune(G::TpWatchBuyAlerts, SaveBuyAlerts);
	}

	void SaveIds(const std::vector<int>& ids)
	{
		std::string s;
		for (size_t i = 0; i < ids.size(); ++i)
		{
			if (i) s += ',';
			s += std::to_string(ids[i]);
		}
		if (s.size() >= sizeof(G::TpWatchIds))
			s.resize(sizeof(G::TpWatchIds) - 1);
		std::snprintf(G::TpWatchIds, sizeof(G::TpWatchIds), "%s", s.c_str());
		PruneAlertsToIds(ids);
		Settings::SetDirty();
	}

	/* Attach thresholds + hit flags; returns number of hits. */
	int ApplyAlerts(std::vector<Row>& rows)
	{
		std::vector<std::pair<int, long long>> sellAlerts;
		std::vector<std::pair<int, long long>> buyAlerts;
		ParseAlerts(G::TpWatchAlerts, sellAlerts);
		ParseBuyAlerts(G::TpWatchBuyAlerts, buyAlerts);
		int hits = 0;
		for (Row& r : rows)
		{
			r.alertSell = 0;
			r.alertBuy = 0;
			for (const auto& e : sellAlerts)
				if (e.first == r.id) { r.alertSell = e.second; break; }
			for (const auto& e : buyAlerts)
				if (e.first == r.id) { r.alertBuy = e.second; break; }
			const bool sellHit = (r.alertSell > 0 && r.sell > 0 && r.sell <= r.alertSell);
			const bool buyHit = (r.alertBuy > 0 && r.buy > 0 && r.buy >= r.alertBuy);
			r.alertHit = sellHit || buyHit;
			if (r.alertHit) ++hits;
		}
		return hits;
	}

	int ParseItemInput(const char* text)
	{
		if (!text || !text[0]) return 0;
		const char* a = std::strstr(text, "[&");
		if (a)
		{
			a += 2;
			const char* b = std::strchr(a, ']');
			if (b && b > a)
			{
				static const char kB64[] =
					"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
				int buf = 0, bits = 0;
				unsigned char out[16]{};
				size_t n = 0;
				for (const char* p = a; p < b && n < sizeof(out); ++p)
				{
					if (*p == '=' || *p == ' ') break;
					const char* q = std::strchr(kB64, *p);
					if (!q) continue;
					buf = (buf << 6) | static_cast<int>(q - kB64);
					bits += 6;
					if (bits >= 8)
					{
						bits -= 8;
						out[n++] = static_cast<unsigned char>((buf >> bits) & 0xFF);
					}
				}
				if (n >= 5 && out[0] == 0x02)
				{
					const int id = out[2] | (out[3] << 8) | (out[4] << 16);
					if (id > 0) return id;
				}
			}
		}
		/* Pure numeric ID only - names must go through wiki resolve. */
		int id = 0;
		bool onlyDigits = true;
		for (const char* p = text; *p; ++p)
		{
			if (*p == ' ' || *p == '\t') continue;
			if (*p >= '0' && *p <= '9')
				id = id * 10 + (*p - '0');
			else { onlyDigits = false; break; }
		}
		return (onlyDigits && id > 0) ? id : 0;
	}


	void SyncRowsFromSettings()
	{
		std::vector<int> ids;
		ParseIds(G::TpWatchIds, ids);
		std::lock_guard<std::mutex> lock(gMu);
		std::vector<Row> next;
		next.reserve(ids.size());
		for (int id : ids)
		{
			Row r;
			r.id = id;
			for (const Row& old : gRows)
			{
				if (old.id == id)
				{
					r = old;
					break;
				}
			}
			next.push_back(std::move(r));
		}
		ApplyAlerts(next);
		gRows = std::move(next);
	}
} // namespace TpWatchDetail

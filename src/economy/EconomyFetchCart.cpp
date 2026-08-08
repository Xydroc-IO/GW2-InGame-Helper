#include "EconomyShared.h"

#include "EconomyInternal.h"

#include "AddonPaths.h"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace EconomyDetail
{
	void AddToCart(int id, const char* name, int qty)
	{
		for (auto& c : gCart)
		{
			if (c.id == id)
			{
				c.qty += qty;
				SaveCart();
				return;
			}
		}
		CartItem c{};
		c.id = id;
		c.qty = qty < 1 ? 1 : qty;
		std::snprintf(c.name, sizeof(c.name), "%s", name && name[0] ? name : FallbackName(id));
		gCart.push_back(c);
		SaveCart();
	}

	void RemoveCart(size_t idx)
	{
		if (idx < gCart.size())
		{
			gCart.erase(gCart.begin() + static_cast<std::ptrdiff_t>(idx));
			SaveCart();
		}
	}

	void ClearCart()
	{
		gCart.clear();
		SaveCart();
	}

	static std::wstring CartPath()
	{
		std::wstring dir = AddonPaths::ConfigDir();
		if (dir.empty()) dir = AddonPaths::DataDir();
		if (dir.empty()) return {};
		if (dir.back() != L'\\' && dir.back() != L'/') dir.push_back(L'\\');
		dir += L"economy-cart.txt";
		return dir;
	}

	static std::wstring ChartsPath()
	{
		std::wstring dir = AddonPaths::ConfigDir();
		if (dir.empty()) dir = AddonPaths::DataDir();
		if (dir.empty()) return {};
		if (dir.back() != L'\\' && dir.back() != L'/') dir.push_back(L'\\');
		dir += L"economy-charts.txt";
		return dir;
	}

	static std::wstring HistPath()
	{
		std::wstring dir = AddonPaths::ConfigDir();
		if (dir.empty()) dir = AddonPaths::DataDir();
		if (dir.empty()) return {};
		if (dir.back() != L'\\' && dir.back() != L'/') dir.push_back(L'\\');
		dir += L"economy-prices.txt";
		return dir;
	}

	static bool WriteUtf8File(const std::wstring& path, const std::string& body)
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return false;
		DWORD written = 0;
		const BOOL ok = WriteFile(h, body.data(), static_cast<DWORD>(body.size()), &written, nullptr);
		CloseHandle(h);
		return ok != 0;
	}

	static bool ReadUtf8File(const std::wstring& path, std::string& out)
	{
		out.clear();
		HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return false;
		LARGE_INTEGER sz{};
		if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > 16 * 1024 * 1024)
		{
			CloseHandle(h);
			return false;
		}
		out.resize(static_cast<size_t>(sz.QuadPart));
		DWORD got = 0;
		const BOOL ok = ReadFile(h, out.data(), static_cast<DWORD>(out.size()), &got, nullptr);
		CloseHandle(h);
		if (!ok) { out.clear(); return false; }
		out.resize(got);
		return true;
	}

	void SaveCart()
	{
		const std::wstring path = CartPath();
		if (path.empty()) return;
		std::string body;
		for (const auto& c : gCart)
			body += std::to_string(c.id) + "\t" + std::to_string(c.qty) + "\t" + c.name + "\n";
		WriteUtf8File(path, body);
	}

	void LoadCart()
	{
		gCart.clear();
		const std::wstring path = CartPath();
		if (path.empty()) return;
		std::string body;
		if (!ReadUtf8File(path, body)) return;
		size_t i = 0;
		while (i < body.size())
		{
			size_t e = body.find('\n', i);
			if (e == std::string::npos) e = body.size();
			std::string line = body.substr(i, e - i);
			i = e + 1;
			CartItem c{};
			int qty = 1;
			char name[96]{};
			if (std::sscanf(line.c_str(), "%d\t%d\t%95[^\n]", &c.id, &qty, name) >= 2)
			{
				c.qty = qty;
				std::snprintf(c.name, sizeof(c.name), "%s", name[0] ? name : FallbackName(c.id));
				gCart.push_back(c);
			}
		}
	}

	void AddChart(int id)
	{
		if (id <= 0)
			return;
		for (int existing : gChartIds)
		{
			if (existing == id)
			{
				gChartItemId = id;
				return;
			}
		}
		gChartIds.push_back(id);
		gChartItemId = id;
		SaveCharts();
	}

	void RemoveChart(size_t idx)
	{
		if (idx >= gChartIds.size())
			return;
		const int removed = gChartIds[idx];
		gChartIds.erase(gChartIds.begin() + static_cast<std::ptrdiff_t>(idx));
		if (gChartItemId == removed)
			gChartItemId = gChartIds.empty() ? 0 : gChartIds.back();
		SaveCharts();
	}

	void ClearCharts()
	{
		gChartIds.clear();
		gChartItemId = 0;
		SaveCharts();
	}

	void SaveCharts()
	{
		const std::wstring path = ChartsPath();
		if (path.empty()) return;
		std::string body;
		for (int id : gChartIds)
			body += std::to_string(id) + "\n";
		WriteUtf8File(path, body);
	}

	void LoadCharts()
	{
		gChartIds.clear();
		const std::wstring path = ChartsPath();
		if (path.empty()) return;
		std::string body;
		if (!ReadUtf8File(path, body))
		{
			/* Migrate: previous builds only had a single gChartItemId. */
			if (gChartItemId > 0)
				gChartIds.push_back(gChartItemId);
			return;
		}
		size_t i = 0;
		while (i < body.size())
		{
			size_t e = body.find('\n', i);
			if (e == std::string::npos) e = body.size();
			std::string line = body.substr(i, e - i);
			i = e + 1;
			int id = 0;
			if (std::sscanf(line.c_str(), "%d", &id) == 1 && id > 0)
			{
				bool dup = false;
				for (int existing : gChartIds)
				{
					if (existing == id) { dup = true; break; }
				}
				if (!dup)
					gChartIds.push_back(id);
			}
		}
		if (!gChartIds.empty() && gChartItemId <= 0)
			gChartItemId = gChartIds.front();
	}

	static void TrimHistoryLocked()
	{
		std::unordered_map<int, size_t> counts;
		counts.reserve(gHistory.size() / 4 + 8);
		for (const auto& s : gHistory)
			++counts[s.id];

		std::unordered_map<int, size_t> drop;
		for (const auto& kv : counts)
		{
			if (kv.second > kMaxSamplesPerId)
				drop[kv.first] = kv.second - kMaxSamplesPerId;
		}

		if (!drop.empty())
		{
			std::vector<PriceSample> kept;
			kept.reserve(gHistory.size());
			for (const auto& s : gHistory)
			{
				auto it = drop.find(s.id);
				if (it != drop.end() && it->second > 0)
				{
					--it->second;
					continue;
				}
				kept.push_back(s);
			}
			gHistory = std::move(kept);
		}

		if (gHistory.size() > kMaxSamplesGlobal)
		{
			const size_t excess = gHistory.size() - kMaxSamplesGlobal;
			gHistory.erase(gHistory.begin(),
				gHistory.begin() + static_cast<std::ptrdiff_t>(excess));
		}
	}

	void AppendSamples(const std::vector<PriceSample>& samples)
	{
		if (samples.empty())
			return;
		std::lock_guard<std::mutex> lock(gMu);
		gHistory.insert(gHistory.end(), samples.begin(), samples.end());
		TrimHistoryLocked();
	}

	void RecordSample(int id, long long buy, long long sell)
	{
		PriceSample s{};
		s.id = id;
		s.buy = buy;
		s.sell = sell;
		s.ts = static_cast<unsigned>(std::time(nullptr));
		AppendSamples({s});
	}

	void SaveHistory()
	{
		const std::wstring path = HistPath();
		if (path.empty()) return;
		std::string body;
		std::vector<PriceSample> snap;
		{
			std::lock_guard<std::mutex> lock(gMu);
			snap = gHistory;
		}
		body.reserve(snap.size() * 40);
		for (const auto& s : snap)
		{
			body += std::to_string(s.id) + "\t" + std::to_string(s.buy) + "\t" +
				std::to_string(s.sell) + "\t" + std::to_string(s.ts) + "\n";
		}
		WriteUtf8File(path, body);
	}

	void LoadHistory()
	{
		std::lock_guard<std::mutex> lock(gMu);
		gHistory.clear();
		const std::wstring path = HistPath();
		if (path.empty()) return;
		std::string body;
		if (!ReadUtf8File(path, body)) return;
		size_t i = 0;
		while (i < body.size())
		{
			size_t e = body.find('\n', i);
			if (e == std::string::npos) e = body.size();
			std::string line = body.substr(i, e - i);
			i = e + 1;
			PriceSample s{};
			if (std::sscanf(line.c_str(), "%d\t%lld\t%lld\t%u", &s.id, &s.buy, &s.sell, &s.ts) >= 3)
				gHistory.push_back(s);
		}
		TrimHistoryLocked();
	}
}

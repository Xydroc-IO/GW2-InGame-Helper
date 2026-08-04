#pragma once

#include <string>
#include <unordered_map>

/* Shared helpers for legacy CSS downlevel (reference / #if 0 path). */
namespace CssCompatDetail
{
	struct Rgba
	{
		int r = 0;
		int g = 0;
		int b = 0;
		float a = 1.f;
	};

	float Clamp01(float x);
	float LinearToSrgb(float x);
	Rgba OklchToRgba(float L, float C, float hDeg, float alpha);
	bool IsDigit(char c);
	bool IsIdentStart(char c);
	size_t SkipWs(const std::string& s, size_t i);
	bool ParseNumber(const std::string& s, size_t& i, float* out, bool* hadPercent);
	std::string FormatRgba(const Rgba& c);
	std::string ReplaceOklch(const std::string& input);
	std::string StripGradientColorSpaces(std::string css);
	void ReplaceAll(std::string& s, const std::string& from, const std::string& to);
	bool ParseHexColor(const std::string& s, Rgba* out);
	bool ParseRgbFunc(const std::string& s, Rgba* out);
	bool ResolveColor(const std::string& raw, const std::unordered_map<std::string, Rgba>& vars, Rgba* out);
	std::unordered_map<std::string, Rgba> CollectVars(const std::string& css);
	std::string RewriteColorMix(const std::string& css, const std::unordered_map<std::string, Rgba>& vars);
	std::string RewriteDisplayP3(const std::string& css);
	std::string StripPropertyKeepInitials(std::string css);
	std::string RewriteContainerQueries(std::string css);
}

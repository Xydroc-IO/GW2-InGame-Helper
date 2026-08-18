#pragma once

#include <string>
#include <utility>
#include <vector>

/* Public ArenaNet data pack (no API keys). GitHub pre-release tag
   `gw2-helper-catalog` (title: GW2 Helper Catalog) — same tag as the CEF zip.
   `.igh` is IGH1 (custom, not zip). Do not attach to DLL tags.
   Cheap check: gw2-helper-catalog.manifest (catalog / icons / cef).
   Cache: addons/.../cache/ names TSV, recipes TSV, .manifest, icons.igh,
   gw2-helper-recipes.igh, gw2-helper-achievements.igh. */
namespace Gw2Catalog
{
	inline constexpr const char* kReleaseTag = "gw2-helper-catalog";
	inline constexpr const char* kManifestFile = "gw2-helper-catalog.manifest";
	inline constexpr const char* kPackFile = "gw2-helper-catalog.igh";
	inline constexpr const char* kRecipesFile = "gw2-helper-recipes.igh";
	inline constexpr const char* kAchievementsFile = "gw2-helper-achievements.igh";
	inline constexpr const char* kIconsFile = "gw2-helper-icons.igh";
	inline constexpr const char* kManifestMagic = "IGH1";

	inline constexpr const char* kManifestUrl =
		"https://github.com/Xydroc-IO/GW2-InGame-Helper/releases/download/"
		"gw2-helper-catalog/gw2-helper-catalog.manifest";
	inline constexpr const char* kPackUrl =
		"https://github.com/Xydroc-IO/GW2-InGame-Helper/releases/download/"
		"gw2-helper-catalog/gw2-helper-catalog.igh";
	inline constexpr const char* kRecipesUrl =
		"https://github.com/Xydroc-IO/GW2-InGame-Helper/releases/download/"
		"gw2-helper-catalog/gw2-helper-recipes.igh";
	inline constexpr const char* kAchievementsUrl =
		"https://github.com/Xydroc-IO/GW2-InGame-Helper/releases/download/"
		"gw2-helper-catalog/gw2-helper-achievements.igh";
	inline constexpr const char* kIconsUrl =
		"https://github.com/Xydroc-IO/GW2-InGame-Helper/releases/download/"
		"gw2-helper-catalog/gw2-helper-icons.igh";

	struct Recipe
	{
		int recipeId = 0;
		int outputId = 0;
		int outputCount = 1;
		int minRating = 0;
		std::string disciplines; /* pipe-separated */
		std::vector<std::pair<int, int>> ings; /* item_id, count */
	};

	/* Load disk cache; GET GitHub manifest on a new shipping revision or while
	   packs are still missing. UI-safe. */
	void Tick();
	bool RecipesReady();

	bool ItemName(int id, std::string* out);
	bool CurrencyName(int id, std::string* out);
	bool ItemIcon(int id, std::string* out);
	bool CurrencyIcon(int id, std::string* out);
	bool SkinName(int id, std::string* out);
	bool SkinIcon(int id, std::string* out);
	bool MiniName(int id, std::string* out);
	bool MiniIcon(int id, std::string* out);
	bool MaterialCategoryName(int id, std::string* out);
	bool HasMaterialCategories();

	/* Any names-pack kind: i c s n m d f o g u v t a y */
	bool Name(char kind, int id, std::string* out);
	bool Icon(char kind, int id, std::string* out);
	/* 4th TSV column when it is not a render icon (armory max_count, dye rgb). */
	bool Extra(char kind, int id, std::string* out);
	/* Cloth/leather/metal RGB from names-pack extra (`r,g,b` or hex). */
	bool DyeRgb(int id, int* r, int* g, int* b);

	struct ArmoryRow
	{
		int id = 0;
		int maxCount = 1;
		std::string name;
	};
	bool ArmoryAll(std::vector<ArmoryRow>* out);

	bool RecipeById(int recipeId, Recipe* out);
	bool RecipeForOutput(int outputId, Recipe* out, const char* preferDiscipline = nullptr);
	bool RecipesForOutput(int outputId, std::vector<int>* ids);

	/* PNG from gw2-helper-icons.igh (render.guildwars2.com keys). */
	bool IconPng(const char* renderUrl, std::vector<unsigned char>* out);

	/* Groups / categories / defs TSV from gw2-helper-achievements.igh. */
	bool AchievementPack(std::string* groupsTsv, std::string* categoriesTsv,
		std::string* defsTsv);
	bool AchievementPackReady();
}

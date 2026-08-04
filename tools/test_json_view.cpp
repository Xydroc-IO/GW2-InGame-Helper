/* Host-side smoke test for JsonView bounds-checked scrapers. */
#include "api/JsonView.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_set>

static int gFails = 0;

#define CHECK(cond) do { if (!(cond)) { \
	std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
	++gFails; } } while (0)

int main()
{
	const std::string j =
		R"({"id":42,"name":"Sword \"X\"","count":3,"ok":true,"coord":[1.5,-2.25],"bag":{"id":9}})";

	CHECK(JsonView::IntAfterKey(j, "id") == 42);
	CHECK(JsonView::IntAfterKey(j, "count") == 3);
	CHECK(JsonView::IntAfterKey(j, "missing") == -1);
	CHECK(JsonView::StringAfterKey(j, "name") == "Sword \"X\"");
	CHECK(JsonView::BoolAfterKey(j, "ok"));
	CHECK(!JsonView::BoolAfterKey(j, "count"));

	float x = 0, y = 0;
	CHECK(JsonView::CoordAfterKey(j, "coord", 0, x, y));
	CHECK(x > 1.49f && x < 1.51f);
	CHECK(y < -2.24f && y > -2.26f);

	const size_t bag = j.find("\"bag\"");
	const size_t brace = j.find('{', bag);
	const size_t end = JsonView::ObjectEnd(j, brace);
	CHECK(end != std::string::npos);
	CHECK(JsonView::IntAfterKey(j, "id", brace) == 9);

	/* Truncated / non-null-terminated view must not read past size. */
	const char raw[] = {"\"id\":99XXXX"};
	JsonView::View v(raw, 7); /* "\"id\":99" only — no trailing NUL in view length sense */
	/* ValueStart looks for "id" inside; feed a proper mini doc: */
	const char mini[] = {"{\"id\":99}"};
	JsonView::Bytes b(mini, sizeof(mini) - 1);
	CHECK(JsonView::IntAfterKey(b.view(), "id") == 99);

	std::unordered_set<int> ids;
	JsonView::ParseIdArray(std::string("[1,2,99,0,-3]"), ids);
	CHECK(ids.count(1) && ids.count(2) && ids.count(99));
	CHECK(!ids.count(0));

	/* Overflow digit run should fail ParseInt64 rather than wrap. */
	long long ov = 0;
	const std::string huge = "999999999999999999999999999999";
	CHECK(!JsonView::ParseInt64(JsonView::AsView(huge), 0, &ov, nullptr));

	if (gFails)
	{
		std::fprintf(stderr, "%d checks failed\n", gFails);
		return 1;
	}
	std::puts("JsonView ok");
	return 0;
}

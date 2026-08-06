#include "PathingLua.h"
#include "PathingLuaInternal.h"
#include "PathingParse.h"

#include "miniz/miniz.h"

#include <string>
#include <vector>

/* Extract .lua sources from an open pack zip into the Lua runtime. */
namespace PathingLuaLoad
{
	void FromZip(mz_zip_archive& zip)
	{
		const mz_uint n = mz_zip_reader_get_num_files(&zip);
		for (mz_uint i = 0; i < n; ++i)
		{
			mz_zip_archive_file_stat st{};
			if (!mz_zip_reader_file_stat(&zip, i, &st) || st.m_is_directory)
				continue;
			std::string name = st.m_filename ? st.m_filename : "";
			std::string low = PathingParse::ToLower(name);
			if (low.size() < 4 || low.compare(low.size() - 4, 4, ".lua") != 0)
				continue;
			if (st.m_uncomp_size == 0 || st.m_uncomp_size > 2u * 1024u * 1024u)
				continue;
			std::vector<uint8_t> bytes;
			if (!PathingParse::ZipExtractIndex(zip, static_cast<int>(i), bytes,
				2u * 1024u * 1024u))
				continue;
			const std::string src(reinterpret_cast<const char*>(bytes.data()), bytes.size());
			PathingLua::AddScriptSource(name, src);
		}
		/* Blish-style: store all scripts, then run pack.lua entry points which Require the rest. */
		if (PathingLua::Enabled())
			PathingLua::RunPendingPackEntries();
	}
}

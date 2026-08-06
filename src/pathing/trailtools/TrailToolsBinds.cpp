#include "TrailToolsBinds.h"

namespace
{
	using TrailToolsBinds::State;

	State gBinds{};
}

TrailToolsBinds::State& TrailToolsBinds::Get()
{
	return gBinds;
}

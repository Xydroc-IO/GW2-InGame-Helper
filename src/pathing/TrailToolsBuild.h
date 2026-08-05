#pragma once

#include <string>

namespace TrailToolsBuild
{
	/* Ensure authoring dirs, write XML + active trail .trl, zip to pathing/<Pack>.taco. */
	bool BuildTaco(std::string& errOut);
}

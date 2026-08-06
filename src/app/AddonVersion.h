#pragma once

/* Single source of truth for the shipping addon revision.
 * Bump these when shipping (see docs/DOCUMENTATION.md version stamp checklist).
 * CMake project() VERSION is derived from this header at configure time. */
#define ADDON_VERSION_MAJOR    2
#define ADDON_VERSION_MINOR    2
#define ADDON_VERSION_BUILD    3
#define ADDON_VERSION_REVISION 1

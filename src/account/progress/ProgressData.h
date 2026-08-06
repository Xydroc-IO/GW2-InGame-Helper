#pragma once

/* Account Progress - legendary armory + character roster (official API).
   Drawn inside AccountPad; no third-party site ties. */
namespace ProgressData
{
	void RefreshIfNeeded(bool force = false);
	void RenderContents();
}

#include "WatchPadInternal.h"

#include "EiRuntime.h"
#include "PadNav.h"

#include "imgui/imgui.h"

namespace WatchPadDetail
{
	void DrawHelp()
	{
		const bool wine = EiRuntime::IsWine();
		ImGui::BeginChild("###gw2igh_watch_about", ImVec2(0.f, 0.f), true);
		PadNav::SectionTitle("What is Watch?");
		PadNav::Blurb(
			"A look-only mirror of another desktop window (browser, Twitch app, guide "
			"video, etc.). Playback stays in that app — Helper only shows pixels inside GW2.");
		ImGui::Spacing();
		PadNav::SectionTitle("What it does not do");
		PadNav::Meta("No click-through, no key inject, no audio routing.");
		PadNav::Meta("Guild Wars 2 never sees this capture traffic (local only).");
		PadNav::Meta("Not a CEF/Widevine player — use your system app for DRM video.");
		ImGui::Spacing();
		PadNav::SectionTitle("How to use");
		if (wine)
		{
			PadNav::Meta("1. On the Watch tab, press Start — the Linux share picker opens.");
			PadNav::Meta("2. Pick the window or screen to share.");
			PadNav::Meta("3. Watch Mirror opens the picture (~60 FPS, up to 1280×720).");
			PadNav::Meta("4. Stop on the Watch tab, or close Mirror, to end capture.");
		}
		else
		{
			PadNav::Meta("1. On the Watch tab, press Start — the Windows capture picker opens.");
			PadNav::Meta("2. Pick a window or screen from the system thumbnail UI.");
			PadNav::Meta("3. Watch Mirror shows the picture (~60 FPS, up to 1280×720).");
			PadNav::Meta("4. Stop on the Watch tab, or close Mirror, to end capture.");
			PadNav::Meta("Optional: Classic list… falls back to titled HWND + GDI capture.");
		}
		ImGui::Spacing();
		PadNav::SectionTitle("Tips");
		PadNav::Meta("Crop chrome on the Watch tab trims browser title bars.");
		PadNav::Meta("Resize Mirror freely — the picture letterboxes to keep aspect.");
		PadNav::Meta("Black frames often mean DRM or a hardware overlay in the source app.");
		PadNav::Meta("Closing this control pad hides Start/Stop only — Mirror keeps running until Stop.");
		ImGui::EndChild();
	}
}

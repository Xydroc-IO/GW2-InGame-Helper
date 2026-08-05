#pragma once

/* Per-character pad layout store (profiles.json under addon config/).
   Not the Account pad — AccountPad is API stash/vault UI.
   Profiles remember which allowlisted panels are open + font/window prefs. */
namespace CharacterProfiles
{
	void Load();
	void Save(bool force = false);

	/* Call each frame after MumbleIdentity::Tick(). */
	void Tick();

	/* Capture allowlisted panel state into the active character's profile. */
	void CaptureCurrent();

	/* Apply stored layout for name (no-op if empty / unknown name). */
	void ApplyProfile(const char* characterName);

	const char* ActiveCharacter();
}

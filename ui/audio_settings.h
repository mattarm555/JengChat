#pragma once

enum class JengSoundEffect
{
    ARENA_SHOT,
    ARENA_MINE,
    ARENA_DEATH
};

// Reads saved settings only. Safe before InitWindow().
void LoadAudioSettings();

// Initializes Raylib's audio device and creates JENG CHAT's procedural
// music + sound effects. Call after InitWindow().
void InitializeJengAudio();

// Call once per frame.
// mainScreen = user is inside the connected JENG CHAT client.
// arenaActive = JENG ARENA currently owns the window.
void UpdateJengAudio(
    bool mainScreen,
    bool arenaActive
);

void ShutdownJengAudio();

void PlayJengSound(
    JengSoundEffect effect
);

// Audio settings modal.
void OpenAudioSettings();
void DrawAudioSettings();
bool IsAudioSettingsOpen();
void CancelAudioSettings();

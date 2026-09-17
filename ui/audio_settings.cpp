#include "audio_settings.h"

#include "../config.h"
#include "../theme.h"
#include "ui_common.h"

#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

namespace
{
    constexpr unsigned int SAMPLE_RATE = 44100;
    constexpr float TWO_PI = 6.28318530717958647692f;

    struct AudioSettings
    {
        bool masterEnabled = true;
        bool musicEnabled = true;
        bool sfxEnabled = true;

        float musicVolume = 0.28f;
        float sfxVolume = 0.70f;
    };

    enum class MusicTrack
    {
        NONE,
        LOBBY,
        ARENA
    };

    AudioSettings gSettings{};
    AudioSettings gBeforeOpen{};

    bool gSettingsOpen = false;
    bool gAudioLoaded = false;

    Sound gLobbyMusic{};
    Sound gArenaMusic{};

    Sound gArenaShot{};
    Sound gArenaMine{};
    Sound gArenaDeath{};

    MusicTrack gCurrentTrack = MusicTrack::NONE;


    float ClampSample(float sample)
    {
        return std::clamp(sample, -1.0f, 1.0f);
    }


    float Noise(std::uint32_t& state)
    {
        state =
            state * 1664525u +
            1013904223u;

        float normalized =
            (float)((state >> 8) & 0x00FFFFFFu) /
            (float)0x00FFFFFFu;

        return
            normalized * 2.0f -
            1.0f;
    }


    float SquareWave(float phase)
    {
        return
            std::sin(phase) >= 0.0f
            ? 1.0f
            : -1.0f;
    }


    float TriangleWave(float phase)
    {
        return
            (2.0f / 3.14159265358979323846f) *
            std::asin(std::sin(phase));
    }


    Sound SoundFromSamples(
        std::vector<short>& samples)
    {
        Wave wave{};
        wave.frameCount =
            (unsigned int)samples.size();

        wave.sampleRate = SAMPLE_RATE;
        wave.sampleSize = 16;
        wave.channels = 1;
        wave.data = samples.data();

        return LoadSoundFromWave(wave);
    }


    Sound GenerateArenaShot()
    {
        constexpr float duration = 0.12f;

        unsigned int frames =
            (unsigned int)(duration * SAMPLE_RATE);

        std::vector<short> samples(frames);
        std::uint32_t noiseState = 0x4A454E47u;

        for (unsigned int i = 0; i < frames; i++)
        {
            float t = (float)i / SAMPLE_RATE;
            float envelope = std::exp(-24.0f * t);

            float punch =
                std::sin(
                    TWO_PI *
                    (185.0f - 70.0f * t) *
                    t
                );

            float sample =
                envelope *
                (
                    punch * 0.72f +
                    Noise(noiseState) * 0.38f
                );

            samples[i] =
                (short)(
                    ClampSample(sample) *
                    32767.0f
                );
        }

        return SoundFromSamples(samples);
    }


    Sound GenerateArenaMine()
    {
        constexpr float duration = 0.20f;

        unsigned int frames =
            (unsigned int)(duration * SAMPLE_RATE);

        std::vector<short> samples(frames);

        for (unsigned int i = 0; i < frames; i++)
        {
            float t = (float)i / SAMPLE_RATE;
            float envelope = std::exp(-10.0f * t);

            float chirp =
                std::sin(TWO_PI * 620.0f * t) * 0.70f +
                std::sin(TWO_PI * 930.0f * t) * 0.30f;

            samples[i] =
                (short)(
                    ClampSample(envelope * chirp) *
                    32767.0f
                );
        }

        return SoundFromSamples(samples);
    }


    Sound GenerateArenaDeath()
    {
        constexpr float duration = 0.78f;

        unsigned int frames =
            (unsigned int)(duration * SAMPLE_RATE);

        std::vector<short> samples(frames);
        std::uint32_t noiseState = 0x44454144u;

        for (unsigned int i = 0; i < frames; i++)
        {
            float t = (float)i / SAMPLE_RATE;
            float envelope = std::exp(-4.6f * t);

            float boom =
                std::sin(TWO_PI * 62.0f * t) * 0.68f +
                std::sin(TWO_PI * 41.0f * t) * 0.25f;

            float sample =
                envelope *
                (
                    boom +
                    Noise(noiseState) * 0.55f
                );

            samples[i] =
                (short)(
                    ClampSample(sample) *
                    32767.0f
                );
        }

        return SoundFromSamples(samples);
    }


    float NoteFrequency(int semitonesFromA4)
    {
        return
            440.0f *
            std::pow(
                2.0f,
                (float)semitonesFromA4 / 12.0f
            );
    }


    Sound GenerateLobbyMusic()
    {
        // 8-second original retro/chiptune loop.
        // Calm enough to sit behind chat without being distracting.
        constexpr float duration = 8.0f;
        constexpr float stepLength = 0.25f;

        // C minor-ish arpeggio:
        // C4, Eb4, G4, Bb4, G4, Eb4...
        const int melody[] = {
            -9, -6, -2, 1,
            -2, -6, -9, -6,
            -4, -1, 3, 6,
            3, -1, -4, -1,
            -9, -6, -2, 1,
            3, 1, -2, -6,
            -4, -1, 3, -1,
            -6, -4, -6, -9
        };

        const int bass[] = {
            -21, -21, -21, -21,
            -21, -21, -21, -21,
            -16, -16, -16, -16,
            -16, -16, -16, -16,
            -21, -21, -21, -21,
            -14, -14, -14, -14,
            -16, -16, -16, -16,
            -21, -21, -21, -21
        };

        unsigned int frames =
            (unsigned int)(duration * SAMPLE_RATE);

        std::vector<short> samples(frames);

        for (unsigned int i = 0; i < frames; i++)
        {
            float t = (float)i / SAMPLE_RATE;

            int step =
                std::min(
                    31,
                    (int)(t / stepLength)
                );

            float withinStep =
                std::fmod(t, stepLength);

            // Tiny decay keeps each chip note crisp.
            float noteEnvelope =
                0.78f +
                0.22f *
                std::exp(-8.0f * withinStep);

            float melodyHz =
                NoteFrequency(melody[step]);

            float bassHz =
                NoteFrequency(bass[step]);

            float melodyWave =
                SquareWave(
                    TWO_PI *
                    melodyHz *
                    t
                ) * 0.11f;

            float softHarmony =
                TriangleWave(
                    TWO_PI *
                    melodyHz *
                    0.5f *
                    t
                ) * 0.06f;

            float bassWave =
                TriangleWave(
                    TWO_PI *
                    bassHz *
                    t
                ) * 0.12f;

            // Quiet pulse on the quarter note.
            float beatPosition =
                std::fmod(t, 0.50f);

            float pulse =
                std::exp(
                    -18.0f *
                    beatPosition
                ) *
                std::sin(
                    TWO_PI *
                    78.0f *
                    beatPosition
                ) *
                0.045f;

            float sample =
                noteEnvelope *
                (
                    melodyWave +
                    softHarmony
                ) +
                bassWave +
                pulse;

            samples[i] =
                (short)(
                    ClampSample(sample) *
                    32767.0f
                );
        }

        return SoundFromSamples(samples);
    }


    Sound GenerateArenaMusic()
    {
        // 6.4-second original faster combat loop (150 BPM).
        constexpr float duration = 6.4f;
        constexpr float stepLength = 0.20f;

        const int lead[] = {
            -9, -2, -6, 3,
            -9, 1, -6, 6,
            -4, 3, -1, 8,
            -4, 1, -1, 6,
            -9, -2, -6, 3,
            -7, 1, -4, 6,
            -4, 3, -1, 8,
            -6, 1, -4, -9
        };

        const int bass[] = {
            -21, -21, -21, -21,
            -21, -21, -21, -21,
            -16, -16, -16, -16,
            -16, -16, -16, -16,
            -21, -21, -21, -21,
            -19, -19, -19, -19,
            -16, -16, -16, -16,
            -21, -21, -21, -21
        };

        unsigned int frames =
            (unsigned int)(duration * SAMPLE_RATE);

        std::vector<short> samples(frames);
        std::uint32_t noiseState = 0x4152454Eu;

        for (unsigned int i = 0; i < frames; i++)
        {
            float t = (float)i / SAMPLE_RATE;

            int step =
                std::min(
                    31,
                    (int)(t / stepLength)
                );

            float withinStep =
                std::fmod(t, stepLength);

            float leadHz =
                NoteFrequency(lead[step]);

            float bassHz =
                NoteFrequency(bass[step]);

            float leadEnvelope =
                std::exp(
                    -2.8f *
                    withinStep
                );

            float leadWave =
                (
                    SquareWave(
                        TWO_PI *
                        leadHz *
                        t
                    ) *
                    0.10f
                ) +
                (
                    TriangleWave(
                        TWO_PI *
                        leadHz *
                        2.0f *
                        t
                    ) *
                    0.045f
                );

            float bassWave =
                TriangleWave(
                    TWO_PI *
                    bassHz *
                    t
                ) *
                0.15f;

            // 150 BPM kick every beat.
            float beatPosition =
                std::fmod(t, 0.40f);

            float kickEnvelope =
                std::exp(
                    -16.0f *
                    beatPosition
                );

            float kick =
                std::sin(
                    TWO_PI *
                    (
                        82.0f -
                        32.0f * beatPosition
                    ) *
                    beatPosition
                ) *
                kickEnvelope *
                0.13f;

            // Short noisy snare between beats.
            float halfBeatPosition =
                std::fmod(
                    t + 0.20f,
                    0.40f
                );

            float snareEnvelope =
                std::exp(
                    -23.0f *
                    halfBeatPosition
                );

            float snare =
                Noise(noiseState) *
                snareEnvelope *
                0.055f;

            float sample =
                leadWave *
                leadEnvelope +
                bassWave +
                kick +
                snare;

            samples[i] =
                (short)(
                    ClampSample(sample) *
                    32767.0f
                );
        }

        return SoundFromSamples(samples);
    }


    filesystem::path AudioSettingsPath()
    {
#ifdef _WIN32
        const char* appData = getenv("APPDATA");

        if (appData && *appData)
        {
            return
                filesystem::path(appData) /
                "JengChat" /
                "audio.cfg";
        }

        return filesystem::path("audio.cfg");

#elif defined(__APPLE__)
        const char* home = getenv("HOME");

        if (home && *home)
        {
            return
                filesystem::path(home) /
                "Library" /
                "Application Support" /
                "JengChat" /
                "audio.cfg";
        }

        return filesystem::path("audio.cfg");

#else
        const char* home = getenv("HOME");

        if (home && *home)
        {
            return
                filesystem::path(home) /
                ".config" /
                "JengChat" /
                "audio.cfg";
        }

        return filesystem::path("audio.cfg");
#endif
    }


    bool ParseBool(
        const std::string& value,
        bool fallback)
    {
        if (
            value == "1" ||
            value == "true" ||
            value == "TRUE"
        )
        {
            return true;
        }

        if (
            value == "0" ||
            value == "false" ||
            value == "FALSE"
        )
        {
            return false;
        }

        return fallback;
    }


    float ParseVolume(
        const std::string& value,
        float fallback)
    {
        try
        {
            return
                std::clamp(
                    std::stof(value),
                    0.0f,
                    1.0f
                );
        }
        catch (...)
        {
            return fallback;
        }
    }


    void SaveAudioSettings()
    {
        filesystem::path path =
            AudioSettingsPath();

        error_code ec;
        filesystem::create_directories(
            path.parent_path(),
            ec
        );

        ofstream file(path);

        if (!file)
            return;

        file
            << "master="
            << (gSettings.masterEnabled ? 1 : 0)
            << "\n";

        file
            << "music="
            << (gSettings.musicEnabled ? 1 : 0)
            << "\n";

        file
            << "sfx="
            << (gSettings.sfxEnabled ? 1 : 0)
            << "\n";

        file
            << "music_volume="
            << gSettings.musicVolume
            << "\n";

        file
            << "sfx_volume="
            << gSettings.sfxVolume
            << "\n";
    }


    void StopMusic()
    {
        if (!gAudioLoaded)
            return;

        if (IsSoundPlaying(gLobbyMusic))
            StopSound(gLobbyMusic);

        if (IsSoundPlaying(gArenaMusic))
            StopSound(gArenaMusic);

        gCurrentTrack =
            MusicTrack::NONE;
    }


    void ApplyVolumes()
    {
        if (!gAudioLoaded)
            return;

        float music =
            gSettings.masterEnabled &&
            gSettings.musicEnabled
            ? gSettings.musicVolume
            : 0.0f;

        float sfx =
            gSettings.masterEnabled &&
            gSettings.sfxEnabled
            ? gSettings.sfxVolume
            : 0.0f;

        SetSoundVolume(
            gLobbyMusic,
            music * 0.72f
        );

        SetSoundVolume(
            gArenaMusic,
            music * 0.82f
        );

        SetSoundVolume(
            gArenaShot,
            sfx * 0.74f
        );

        SetSoundVolume(
            gArenaMine,
            sfx * 0.68f
        );

        SetSoundVolume(
            gArenaDeath,
            sfx * 1.00f
        );
    }


    bool DrawToggle(
        Rectangle rect,
        const char* label,
        bool& value)
    {
        bool hover =
            IsMouseInside(rect);

        DrawRectangleRounded(
            rect,
            0.12f,
            8,
            hover
            ? PANEL_LIGHT
            : PANEL_ALT
        );

        DrawRectangleRoundedLinesEx(
            rect,
            0.12f,
            8,
            1.5f,
            value
            ? SUCCESS
            : Color{90, 92, 104, 255}
        );

        DrawText(
            label,
            (int)rect.x + 14,
            (int)rect.y + 13,
            15,
            TEXT_MAIN
        );

        const char* status =
            value
            ? "ON"
            : "OFF";

        Color statusColor =
            value
            ? SUCCESS
            : TEXT_MUTED;

        int width =
            MeasureText(
                status,
                15
            );

        DrawText(
            status,
            (int)(
                rect.x +
                rect.width -
                width -
                14
            ),
            (int)rect.y + 13,
            15,
            statusColor
        );

        if (
            hover &&
            IsMouseButtonPressed(
                MOUSE_BUTTON_LEFT
            )
        )
        {
            value = !value;
            return true;
        }

        return false;
    }


    bool DrawVolumeSlider(
        const char* label,
        Rectangle bar,
        float& value)
    {
        DrawText(
            label,
            (int)bar.x,
            (int)bar.y - 28,
            15,
            TEXT_MAIN
        );

        int percent =
            (int)std::round(
                value * 100.0f
            );

        std::string valueText =
            std::to_string(percent) +
            "%";

        int valueWidth =
            MeasureText(
                valueText.c_str(),
                14
            );

        DrawText(
            valueText.c_str(),
            (int)(
                bar.x +
                bar.width -
                valueWidth
            ),
            (int)bar.y - 27,
            14,
            JENG_YELLOW
        );

        Rectangle hitbox = {
            bar.x,
            bar.y - 8.0f,
            bar.width,
            bar.height + 16.0f
        };

        bool hover =
            IsMouseInside(hitbox);

        DrawRectangleRounded(
            bar,
            0.5f,
            8,
            Color{48, 50, 58, 255}
        );

        Rectangle fill = {
            bar.x,
            bar.y,
            bar.width * value,
            bar.height
        };

        DrawRectangleRounded(
            fill,
            0.5f,
            8,
            JENG_RED
        );

        DrawCircle(
            (int)(
                bar.x +
                bar.width * value
            ),
            (int)(
                bar.y +
                bar.height / 2.0f
            ),
            hover ? 8.0f : 7.0f,
            JENG_YELLOW
        );

        if (
            hover &&
            IsMouseButtonDown(
                MOUSE_BUTTON_LEFT
            )
        )
        {
            Vector2 mouse =
                GetUIMousePosition();

            value =
                std::clamp(
                    (mouse.x - bar.x) /
                    bar.width,
                    0.0f,
                    1.0f
                );

            ApplyVolumes();
            return true;
        }

        return false;
    }
}


void LoadAudioSettings()
{
    ifstream file(
        AudioSettingsPath()
    );

    if (!file)
        return;

    string line;

    while (getline(file, line))
    {
        size_t equals =
            line.find('=');

        if (equals == string::npos)
            continue;

        string key =
            line.substr(0, equals);

        string value =
            line.substr(equals + 1);

        if (key == "master")
        {
            gSettings.masterEnabled =
                ParseBool(
                    value,
                    gSettings.masterEnabled
                );
        }
        else if (key == "music")
        {
            gSettings.musicEnabled =
                ParseBool(
                    value,
                    gSettings.musicEnabled
                );
        }
        else if (key == "sfx")
        {
            gSettings.sfxEnabled =
                ParseBool(
                    value,
                    gSettings.sfxEnabled
                );
        }
        else if (key == "music_volume")
        {
            gSettings.musicVolume =
                ParseVolume(
                    value,
                    gSettings.musicVolume
                );
        }
        else if (key == "sfx_volume")
        {
            gSettings.sfxVolume =
                ParseVolume(
                    value,
                    gSettings.sfxVolume
                );
        }
    }
}


void InitializeJengAudio()
{
    if (gAudioLoaded)
        return;

    if (!IsAudioDeviceReady())
        InitAudioDevice();

    if (!IsAudioDeviceReady())
        return;

    gLobbyMusic =
        GenerateLobbyMusic();

    gArenaMusic =
        GenerateArenaMusic();

    gArenaShot =
        GenerateArenaShot();

    gArenaMine =
        GenerateArenaMine();

    gArenaDeath =
        GenerateArenaDeath();

    gAudioLoaded = true;

    ApplyVolumes();
}


void UpdateJengAudio(
    bool mainScreen,
    bool arenaActive)
{
    if (!gAudioLoaded)
        return;

    ApplyVolumes();

    if (
        !mainScreen ||
        !gSettings.masterEnabled ||
        !gSettings.musicEnabled
    )
    {
        StopMusic();
        return;
    }

    MusicTrack desired =
        arenaActive
        ? MusicTrack::ARENA
        : MusicTrack::LOBBY;

    if (desired != gCurrentTrack)
    {
        StopMusic();
        gCurrentTrack = desired;
    }

    Sound* track =
        desired == MusicTrack::ARENA
        ? &gArenaMusic
        : &gLobbyMusic;

    if (!IsSoundPlaying(*track))
        PlaySound(*track);
}


void ShutdownJengAudio()
{
    if (!gAudioLoaded)
    {
        if (IsAudioDeviceReady())
            CloseAudioDevice();

        return;
    }

    StopMusic();

    StopSound(gArenaShot);
    StopSound(gArenaMine);
    StopSound(gArenaDeath);

    UnloadSound(gLobbyMusic);
    UnloadSound(gArenaMusic);

    UnloadSound(gArenaShot);
    UnloadSound(gArenaMine);
    UnloadSound(gArenaDeath);

    gAudioLoaded = false;

    if (IsAudioDeviceReady())
        CloseAudioDevice();
}


void PlayJengSound(
    JengSoundEffect effect)
{
    if (
        !gAudioLoaded ||
        !gSettings.masterEnabled ||
        !gSettings.sfxEnabled
    )
    {
        return;
    }

    switch (effect)
    {
        case JengSoundEffect::ARENA_SHOT:
            PlaySound(gArenaShot);
            break;

        case JengSoundEffect::ARENA_MINE:
            PlaySound(gArenaMine);
            break;

        case JengSoundEffect::ARENA_DEATH:
            PlaySound(gArenaDeath);
            break;
    }
}


void OpenAudioSettings()
{
    if (gSettingsOpen)
        return;

    gBeforeOpen = gSettings;
    gSettingsOpen = true;
}


bool IsAudioSettingsOpen()
{
    return gSettingsOpen;
}


void CancelAudioSettings()
{
    if (!gSettingsOpen)
        return;

    gSettings = gBeforeOpen;
    ApplyVolumes();

    gSettingsOpen = false;
}


void DrawAudioSettings()
{
    if (!gSettingsOpen)
        return;

    DrawRectangle(
        0,
        0,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        Color{0, 0, 0, 215}
    );

    const float panelWidth = 620.0f;
    const float panelHeight = 470.0f;

    Rectangle panel = {
        WINDOW_WIDTH / 2.0f -
            panelWidth / 2.0f,

        WINDOW_HEIGHT / 2.0f -
            panelHeight / 2.0f,

        panelWidth,
        panelHeight
    };

    DrawRectangleRounded(
        panel,
        0.04f,
        10,
        PANEL
    );

    DrawRectangleRoundedLinesEx(
        panel,
        0.04f,
        10,
        2.0f,
        JENG_YELLOW
    );

    DrawText(
        "AUDIO SETTINGS",
        (int)panel.x + 28,
        (int)panel.y + 24,
        29,
        JENG_RED
    );

    DrawText(
        "Music and game sounds are saved on this computer.",
        (int)panel.x + 28,
        (int)panel.y + 62,
        14,
        TEXT_MUTED
    );

    Rectangle masterToggle = {
        panel.x + 28,
        panel.y + 100,
        170,
        44
    };

    Rectangle musicToggle = {
        panel.x + 212,
        panel.y + 100,
        170,
        44
    };

    Rectangle sfxToggle = {
        panel.x + 396,
        panel.y + 100,
        196,
        44
    };

    if (
        DrawToggle(
            masterToggle,
            "MASTER",
            gSettings.masterEnabled
        )
    )
    {
        ApplyVolumes();
    }

    if (
        DrawToggle(
            musicToggle,
            "MUSIC",
            gSettings.musicEnabled
        )
    )
    {
        ApplyVolumes();
    }

    if (
        DrawToggle(
            sfxToggle,
            "GAME SFX",
            gSettings.sfxEnabled
        )
    )
    {
        ApplyVolumes();
    }

    Rectangle musicSlider = {
        panel.x + 38,
        panel.y + 202,
        panel.width - 76,
        12
    };

    Rectangle sfxSlider = {
        panel.x + 38,
        panel.y + 272,
        panel.width - 76,
        12
    };

    DrawVolumeSlider(
        "MUSIC VOLUME",
        musicSlider,
        gSettings.musicVolume
    );

    DrawVolumeSlider(
        "GAME SFX VOLUME",
        sfxSlider,
        gSettings.sfxVolume
    );

    DrawText(
        "LOBBY TRACK",
        (int)panel.x + 38,
        (int)panel.y + 316,
        13,
        TEXT_MUTED
    );

    DrawText(
        "RETRO CHIPTUNE",
        (int)panel.x + 166,
        (int)panel.y + 314,
        15,
        JENG_YELLOW
    );

    DrawText(
        "ARENA TRACK",
        (int)panel.x + 350,
        (int)panel.y + 316,
        13,
        TEXT_MUTED
    );

    DrawText(
        "FAST COMBAT",
        (int)panel.x + 464,
        (int)panel.y + 314,
        15,
        JENG_RED
    );

    Rectangle defaultsButton = {
        panel.x + 28,
        panel.y + 390,
        150,
        48
    };

    Rectangle cancelButton = {
        panel.x + panel.width - 332,
        panel.y + 390,
        140,
        48
    };

    Rectangle saveButton = {
        panel.x + panel.width - 178,
        panel.y + 390,
        150,
        48
    };

    if (
        DrawButton(
            defaultsButton,
            "DEFAULTS",
            PANEL_LIGHT,
            JENG_YELLOW,
            TEXT_MAIN,
            15
        )
    )
    {
        gSettings = AudioSettings{};
        ApplyVolumes();
    }

    if (
        DrawButton(
            cancelButton,
            "CANCEL",
            PANEL_LIGHT,
            JENG_RED,
            TEXT_MAIN,
            15
        )
    )
    {
        CancelAudioSettings();
        return;
    }

    if (
        DrawButton(
            saveButton,
            "SAVE",
            JENG_RED,
            Color{255, 80, 80, 255},
            WHITE,
            15
        )
    )
    {
        SaveAudioSettings();
        gSettingsOpen = false;
        return;
    }
}

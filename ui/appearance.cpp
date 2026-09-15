#include "appearance.h"

#include "../config.h"
#include "../theme.h"
#include "ui_common.h"

#include "raylib.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

namespace
{
    bool gOpen = false;
    int gSelected = 0;

    struct ThemeSnapshot
    {
        Color bg;
        Color panel;
        Color textMain;
        Color textMuted;

        Color chatBg;
        Color chatText;
        Color chatSystem;
        Color chatGame;

        Color accentRed;
        Color accentYellow;
    };

    ThemeSnapshot gBeforeOpen{};


    struct EditableColor
    {
        const char* label;
        Color* color;
    };


    vector<EditableColor> EditableColors()
    {
        return {
            {"BACKGROUND",    &BG},
            {"PANELS",        &PANEL},
            {"MAIN TEXT",     &TEXT_MAIN},
            {"MUTED TEXT",    &TEXT_MUTED},
            {"CHAT BG",       &CHAT_BG},
            {"CHAT TEXT",     &CHAT_TEXT},
            {"SYSTEM CHAT",   &CHAT_SYSTEM},
            {"GAME CHAT",     &CHAT_GAME},
            {"RED ACCENT",    &JENG_RED},
            {"YELLOW ACCENT", &JENG_YELLOW}
        };
    }


    ThemeSnapshot CaptureTheme()
    {
        return ThemeSnapshot{
            BG,
            PANEL,
            TEXT_MAIN,
            TEXT_MUTED,
            CHAT_BG,
            CHAT_TEXT,
            CHAT_SYSTEM,
            CHAT_GAME,
            JENG_RED,
            JENG_YELLOW
        };
    }


    void RestoreTheme(const ThemeSnapshot& value)
    {
        BG = value.bg;

        PANEL = value.panel;
        RefreshDerivedThemeColors();

        TEXT_MAIN = value.textMain;
        TEXT_MUTED = value.textMuted;

        CHAT_BG = value.chatBg;
        CHAT_TEXT = value.chatText;
        CHAT_SYSTEM = value.chatSystem;
        CHAT_GAME = value.chatGame;

        JENG_RED = value.accentRed;
        JENG_YELLOW = value.accentYellow;
    }


    filesystem::path SettingsPath()
    {
#ifdef _WIN32
        const char* appData = getenv("APPDATA");

        if (appData && *appData)
        {
            return filesystem::path(appData) /
                   "JengChat" /
                   "theme.cfg";
        }

        return filesystem::path("theme.cfg");

#elif defined(__APPLE__)
        const char* home = getenv("HOME");

        if (home && *home)
        {
            return filesystem::path(home) /
                   "Library" /
                   "Application Support" /
                   "JengChat" /
                   "theme.cfg";
        }

        return filesystem::path("theme.cfg");

#else
        const char* home = getenv("HOME");

        if (home && *home)
        {
            return filesystem::path(home) /
                   ".config" /
                   "JengChat" /
                   "theme.cfg";
        }

        return filesystem::path("theme.cfg");
#endif
    }


    string ColorToText(Color color)
    {
        return
            to_string((int)color.r) + "," +
            to_string((int)color.g) + "," +
            to_string((int)color.b);
    }


    bool ParseColor(const string& text, Color& output)
    {
        istringstream stream(text);
        string item;
        int values[3] = {0, 0, 0};

        for (int i = 0; i < 3; i++)
        {
            if (!getline(stream, item, ','))
                return false;

            try
            {
                values[i] = stoi(item);
            }
            catch (...)
            {
                return false;
            }

            if (values[i] < 0 || values[i] > 255)
                return false;
        }

        output = Color{
            (unsigned char)values[0],
            (unsigned char)values[1],
            (unsigned char)values[2],
            255
        };

        return true;
    }


    void SaveAppearanceSettings()
    {
        filesystem::path path = SettingsPath();

        error_code ec;
        filesystem::create_directories(path.parent_path(), ec);

        ofstream file(path);

        if (!file)
            return;

        file << "background="    << ColorToText(BG)          << "\n";
        file << "panel="         << ColorToText(PANEL)       << "\n";
        file << "text_main="     << ColorToText(TEXT_MAIN)   << "\n";
        file << "text_muted="    << ColorToText(TEXT_MUTED)  << "\n";
        file << "chat_bg="       << ColorToText(CHAT_BG)     << "\n";
        file << "chat_text="     << ColorToText(CHAT_TEXT)   << "\n";
        file << "chat_system="   << ColorToText(CHAT_SYSTEM) << "\n";
        file << "chat_game="     << ColorToText(CHAT_GAME)   << "\n";
        file << "accent_red="    << ColorToText(JENG_RED)    << "\n";
        file << "accent_yellow=" << ColorToText(JENG_YELLOW) << "\n";
    }


    void ApplySetting(const string& key, const string& value)
    {
        Color parsed;

        if (!ParseColor(value, parsed))
            return;

        if (key == "background")
            BG = parsed;
        else if (key == "panel")
        {
            PANEL = parsed;
            RefreshDerivedThemeColors();
        }
        else if (key == "text_main")
            TEXT_MAIN = parsed;
        else if (key == "text_muted")
            TEXT_MUTED = parsed;
        else if (key == "chat_bg")
            CHAT_BG = parsed;
        else if (key == "chat_text")
            CHAT_TEXT = parsed;
        else if (key == "chat_system")
            CHAT_SYSTEM = parsed;
        else if (key == "chat_game")
            CHAT_GAME = parsed;
        else if (key == "accent_red")
            JENG_RED = parsed;
        else if (key == "accent_yellow")
            JENG_YELLOW = parsed;
    }


    bool DrawSmallButton(
        Rectangle rect,
        const char* label,
        Color fill,
        Color hoverFill,
        Color textColor
    )
    {
        bool hover = IsMouseInside(rect);

        DrawRectangleRounded(
            rect,
            0.12f,
            8,
            hover ? hoverFill : fill
        );

        int width = MeasureText(label, 13);

        DrawText(
            label,
            (int)(rect.x + rect.width / 2.0f - width / 2.0f),
            (int)rect.y + 10,
            13,
            textColor
        );

        return
            hover &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    }


    void DrawChannelSlider(
        const char* label,
        Rectangle bar,
        unsigned char& value,
        Color barColor
    )
    {
        DrawText(
            label,
            (int)bar.x - 30,
            (int)bar.y + 2,
            15,
            TEXT_MUTED
        );

        DrawRectangleRounded(
            bar,
            0.5f,
            8,
            Color{48, 50, 58, 255}
        );

        float fillWidth =
            bar.width *
            ((float)value / 255.0f);

        Rectangle fill = {
            bar.x,
            bar.y,
            fillWidth,
            bar.height
        };

        DrawRectangleRounded(
            fill,
            0.5f,
            8,
            barColor
        );

        float handleX =
            bar.x +
            bar.width *
            ((float)value / 255.0f);

        DrawCircle(
            (int)handleX,
            (int)(bar.y + bar.height / 2.0f),
            8.0f,
            TEXT_MAIN
        );

        Vector2 mouse = GetUIMousePosition();

        bool overExpanded =
            mouse.x >= bar.x - 6 &&
            mouse.x <= bar.x + bar.width + 6 &&
            mouse.y >= bar.y - 8 &&
            mouse.y <= bar.y + bar.height + 8;

        if (
            overExpanded &&
            IsMouseButtonDown(MOUSE_BUTTON_LEFT)
        )
        {
            float t =
                (mouse.x - bar.x) /
                bar.width;

            t = clamp(t, 0.0f, 1.0f);

            value =
                (unsigned char)(
                    t * 255.0f
                );
        }

        string number =
            to_string((int)value);

        DrawText(
            number.c_str(),
            (int)(bar.x + bar.width + 15),
            (int)bar.y + 2,
            14,
            TEXT_MAIN
        );
    }


    void DrawPreview(Rectangle rect)
    {
        DrawRectangleRounded(
            rect,
            0.04f,
            8,
            CHAT_BG
        );

        DrawRectangleRoundedLinesEx(
            rect,
            0.04f,
            8,
            1.0f,
            PANEL_LIGHT
        );

        DrawText(
            "CHAT PREVIEW",
            (int)rect.x + 14,
            (int)rect.y + 12,
            13,
            TEXT_MUTED
        );

        DrawText(
            "[09:41] Alex: hey, JENG CHAT works!",
            (int)rect.x + 14,
            (int)rect.y + 38,
            14,
            CHAT_TEXT
        );

        DrawText(
            "*** Jordan joined JENG CHAT. ***",
            (int)rect.x + 14,
            (int)rect.y + 62,
            14,
            CHAT_SYSTEM
        );

        DrawText(
            "Poker table created.",
            (int)rect.x + 14,
            (int)rect.y + 86,
            14,
            CHAT_GAME
        );
    }
}


void LoadAppearanceSettings()
{
    ifstream file(SettingsPath());

    if (!file)
        return;

    string line;

    while (getline(file, line))
    {
        size_t equals = line.find('=');

        if (equals == string::npos)
            continue;

        string key = line.substr(0, equals);
        string value = line.substr(equals + 1);

        ApplySetting(key, value);
    }
}


void OpenAppearanceSettings()
{
    if (gOpen)
        return;

    gBeforeOpen = CaptureTheme();
    gSelected = 0;
    gOpen = true;
}


bool IsAppearanceSettingsOpen()
{
    return gOpen;
}


void CancelAppearanceSettings()
{
    if (!gOpen)
        return;

    RestoreTheme(gBeforeOpen);
    gOpen = false;
}


void DrawAppearanceSettings()
{
    if (!gOpen)
        return;

    // --------------------------------------------------------
    // BACKDROP
    // --------------------------------------------------------

    DrawRectangle(
        0,
        0,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        Color{0, 0, 0, 215}
    );


    // --------------------------------------------------------
    // WINDOW
    // --------------------------------------------------------

    Rectangle panel = {
        115.0f,
        55.0f,
        770.0f,
        540.0f
    };

    DrawRectangleRounded(
        panel,
        0.025f,
        10,
        PANEL
    );

    DrawRectangleRoundedLinesEx(
        panel,
        0.025f,
        10,
        2.0f,
        JENG_YELLOW
    );

    DrawText(
        "APPEARANCE",
        (int)panel.x + 26,
        (int)panel.y + 20,
        28,
        JENG_RED
    );

    DrawText(
        "Customize JENG CHAT colors",
        (int)panel.x + 26,
        (int)panel.y + 55,
        14,
        TEXT_MUTED
    );


    // --------------------------------------------------------
    // COLOR CATEGORY LIST
    // --------------------------------------------------------

    vector<EditableColor> colors =
        EditableColors();

    const float listX = panel.x + 26.0f;
    const float listY = panel.y + 92.0f;

    for (int i = 0; i < (int)colors.size(); i++)
    {
        Rectangle item = {
            listX,
            listY + i * 34.0f,
            218.0f,
            29.0f
        };

        bool hover = IsMouseInside(item);
        bool selected = i == gSelected;

        DrawRectangleRounded(
            item,
            0.10f,
            7,
            selected
                ? JENG_RED
                : (hover ? PANEL_LIGHT : PANEL_ALT)
        );

        Rectangle swatch = {
            item.x + 8,
            item.y + 6,
            17,
            17
        };

        DrawRectangleRounded(
            swatch,
            0.15f,
            5,
            *colors[i].color
        );

        DrawText(
            colors[i].label,
            (int)item.x + 34,
            (int)item.y + 7,
            13,
            selected ? WHITE : TEXT_MAIN
        );

        if (
            hover &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT)
        )
        {
            gSelected = i;
        }
    }


    // --------------------------------------------------------
    // RGB EDITOR
    // --------------------------------------------------------

    gSelected =
        clamp(
            gSelected,
            0,
            (int)colors.size() - 1
        );

    Color* selectedColor =
        colors[gSelected].color;

    DrawText(
        colors[gSelected].label,
        (int)panel.x + 285,
        (int)panel.y + 101,
        19,
        JENG_YELLOW
    );

    Rectangle largeSwatch = {
        panel.x + 655,
        panel.y + 91,
        78,
        58
    };

    DrawRectangleRounded(
        largeSwatch,
        0.12f,
        8,
        *selectedColor
    );

    DrawRectangleRoundedLinesEx(
        largeSwatch,
        0.12f,
        8,
        2.0f,
        TEXT_MAIN
    );

    Rectangle redBar = {
        panel.x + 330,
        panel.y + 169,
        315,
        16
    };

    Rectangle greenBar = {
        panel.x + 330,
        panel.y + 214,
        315,
        16
    };

    Rectangle blueBar = {
        panel.x + 330,
        panel.y + 259,
        315,
        16
    };

    DrawChannelSlider(
        "R",
        redBar,
        selectedColor->r,
        Color{235, 64, 64, 255}
    );

    DrawChannelSlider(
        "G",
        greenBar,
        selectedColor->g,
        Color{70, 210, 120, 255}
    );

    DrawChannelSlider(
        "B",
        blueBar,
        selectedColor->b,
        Color{90, 145, 255, 255}
    );

    selectedColor->a = 255;

    // Panel companions follow the selected panel automatically.
    if (selectedColor == &PANEL)
        RefreshDerivedThemeColors();


    // --------------------------------------------------------
    // PREVIEW
    // --------------------------------------------------------

    DrawPreview(
        Rectangle{
            panel.x + 285,
            panel.y + 320,
            448,
            120
        }
    );


    // --------------------------------------------------------
    // BUTTONS
    // --------------------------------------------------------

    Rectangle resetButton = {
        panel.x + 26,
        panel.y + panel.height - 54,
        145,
        34
    };

    Rectangle cancelButton = {
        panel.x + panel.width - 260,
        panel.y + panel.height - 54,
        105,
        34
    };

    Rectangle saveButton = {
        panel.x + panel.width - 140,
        panel.y + panel.height - 54,
        112,
        34
    };

    if (
        DrawSmallButton(
            resetButton,
            "RESET DEFAULTS",
            PANEL_LIGHT,
            JENG_YELLOW,
            TEXT_MAIN
        )
    )
    {
        ResetClientTheme();
    }

    if (
        DrawSmallButton(
            cancelButton,
            "CANCEL",
            PANEL_LIGHT,
            JENG_RED,
            TEXT_MAIN
        )
    )
    {
        CancelAppearanceSettings();
        return;
    }

    if (
        DrawSmallButton(
            saveButton,
            "SAVE",
            JENG_RED,
            Color{255, 80, 80, 255},
            WHITE
        )
    )
    {
        SaveAppearanceSettings();
        gOpen = false;
        return;
    }
}

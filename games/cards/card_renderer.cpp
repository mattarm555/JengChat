#include "card_renderer.h"

#include "../../theme.h"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

namespace
{
    unordered_map<string, Texture2D> gCardTextures;
    bool gLoaded = false;

    const string ASSET_ROOT = "assets/cards/";

    vector<string> BuildCodes()
    {
        const char* ranks[] = {
            "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K", "A"
        };

        const char suits[] = {'S', 'H', 'D', 'C'};

        vector<string> result;

        for (char suit : suits)
        {
            for (const char* rank : ranks)
                result.push_back(string(rank) + suit);
        }

        result.push_back("back");
        return result;
    }

    Color SuitColor(char suit)
    {
        if (suit == 'H' || suit == 'D')
            return JENG_RED;

        return Color{30, 32, 38, 255};
    }

    const char* SuitLetter(char suit)
    {
        switch (suit)
        {
            case 'S': return "S";
            case 'H': return "H";
            case 'D': return "D";
            case 'C': return "C";
            default: return "?";
        }
    }

    void DrawTextureFit(Texture2D texture, Rectangle destination)
    {
        if (texture.id == 0 || texture.width <= 0 || texture.height <= 0)
            return;

        float scale = min(
            destination.width / (float)texture.width,
            destination.height / (float)texture.height
        );

        float width = texture.width * scale;
        float height = texture.height * scale;

        Rectangle target = {
            destination.x + (destination.width - width) / 2.0f,
            destination.y + (destination.height - height) / 2.0f,
            width,
            height
        };

        Rectangle source = {
            0.0f,
            0.0f,
            (float)texture.width,
            (float)texture.height
        };

        DrawTexturePro(
            texture,
            source,
            target,
            Vector2{0.0f, 0.0f},
            0.0f,
            WHITE
        );
    }

    void DrawFallbackCard(const string& code, Rectangle destination)
    {
        DrawRectangleRounded(destination, 0.08f, 8, Color{246, 246, 242, 255});
        DrawRectangleRoundedLinesEx(destination, 0.08f, 8, 1.5f, Color{65, 67, 74, 255});

        if (code.size() < 2)
            return;

        char suit = code.back();
        string rank = code.substr(0, code.size() - 1);
        Color ink = SuitColor(suit);

        DrawText(
            rank.c_str(),
            (int)destination.x + 8,
            (int)destination.y + 7,
            18,
            ink
        );

        DrawText(
            SuitLetter(suit),
            (int)destination.x + 9,
            (int)destination.y + 28,
            15,
            ink
        );

        int rankWidth = MeasureText(rank.c_str(), 27);
        DrawText(
            rank.c_str(),
            (int)(destination.x + destination.width / 2.0f - rankWidth / 2.0f),
            (int)(destination.y + destination.height / 2.0f - 25),
            27,
            ink
        );

        int suitWidth = MeasureText(SuitLetter(suit), 23);
        DrawText(
            SuitLetter(suit),
            (int)(destination.x + destination.width / 2.0f - suitWidth / 2.0f),
            (int)(destination.y + destination.height / 2.0f + 9),
            23,
            ink
        );
    }

    void DrawFallbackBack(Rectangle destination)
    {
        DrawRectangleRounded(destination, 0.08f, 8, Color{85, 24, 34, 255});
        DrawRectangleRoundedLinesEx(destination, 0.08f, 8, 2.0f, Color{238, 208, 120, 255});

        Rectangle inner = {
            destination.x + 7,
            destination.y + 7,
            destination.width - 14,
            destination.height - 14
        };

        DrawRectangleRoundedLinesEx(inner, 0.08f, 8, 1.0f, Color{238, 208, 120, 190});
        DrawText(
            "J",
            (int)(destination.x + destination.width / 2.0f - 8),
            (int)(destination.y + destination.height / 2.0f - 13),
            26,
            Color{238, 208, 120, 255}
        );
    }
}

void LoadCardAssets()
{
    if (gLoaded)
        return;

    gLoaded = true;

    for (const string& code : BuildCodes())
    {
        string path = ASSET_ROOT + code + ".png";

        if (!FileExists(path.c_str()))
            continue;

        Texture2D texture = LoadTexture(path.c_str());

        if (texture.id != 0)
            gCardTextures[code] = texture;
    }
}

void UnloadCardAssets()
{
    for (auto& entry : gCardTextures)
    {
        if (entry.second.id != 0)
            UnloadTexture(entry.second);
    }

    gCardTextures.clear();
    gLoaded = false;
}

void DrawPlayingCard(const string& code, Rectangle destination)
{
    auto it = gCardTextures.find(code);

    if (it != gCardTextures.end())
    {
        DrawTextureFit(it->second, destination);
        return;
    }

    DrawFallbackCard(code, destination);
}

void DrawCardBack(Rectangle destination)
{
    auto it = gCardTextures.find("back");

    if (it != gCardTextures.end())
    {
        DrawTextureFit(it->second, destination);
        return;
    }

    DrawFallbackBack(destination);
}

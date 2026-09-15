#include "chess_pieces.h"

#include <algorithm>

using namespace std;

namespace
{
    Texture2D whitePawn{};
    Texture2D whiteKnight{};
    Texture2D whiteBishop{};
    Texture2D whiteRook{};
    Texture2D whiteQueen{};
    Texture2D whiteKing{};

    Texture2D blackPawn{};
    Texture2D blackKnight{};
    Texture2D blackBishop{};
    Texture2D blackRook{};
    Texture2D blackQueen{};
    Texture2D blackKing{};

    bool loaded = false;


    Texture2D LoadPieceTexture(
        const char* path
    )
    {
        if (!FileExists(path))
        {
            TraceLog(
                LOG_WARNING,
                "Missing chess texture: %s",
                path
            );

            return Texture2D{};
        }

        Texture2D texture =
            LoadTexture(path);

        SetTextureFilter(
            texture,
            TEXTURE_FILTER_BILINEAR
        );

        return texture;
    }


    Texture2D* TextureForPiece(
        char piece
    )
    {
        switch (piece)
        {
            case 'P': return &whitePawn;
            case 'N': return &whiteKnight;
            case 'B': return &whiteBishop;
            case 'R': return &whiteRook;
            case 'Q': return &whiteQueen;
            case 'K': return &whiteKing;

            case 'p': return &blackPawn;
            case 'n': return &blackKnight;
            case 'b': return &blackBishop;
            case 'r': return &blackRook;
            case 'q': return &blackQueen;
            case 'k': return &blackKing;

            default:
                return nullptr;
        }
    }


    void UnloadIfValid(
        Texture2D& texture
    )
    {
        if (texture.id != 0)
        {
            UnloadTexture(texture);
            texture = Texture2D{};
        }
    }
}


void LoadChessPieceAssets()
{
    if (loaded)
        return;


    whitePawn =
        LoadPieceTexture(
            "assets/chess/white_pawn.png"
        );

    whiteKnight =
        LoadPieceTexture(
            "assets/chess/white_knight.png"
        );

    whiteBishop =
        LoadPieceTexture(
            "assets/chess/white_bishop.png"
        );

    whiteRook =
        LoadPieceTexture(
            "assets/chess/white_rook.png"
        );

    whiteQueen =
        LoadPieceTexture(
            "assets/chess/white_queen.png"
        );

    whiteKing =
        LoadPieceTexture(
            "assets/chess/white_king.png"
        );


    blackPawn =
        LoadPieceTexture(
            "assets/chess/black_pawn.png"
        );

    blackKnight =
        LoadPieceTexture(
            "assets/chess/black_knight.png"
        );

    blackBishop =
        LoadPieceTexture(
            "assets/chess/black_bishop.png"
        );

    blackRook =
        LoadPieceTexture(
            "assets/chess/black_rook.png"
        );

    blackQueen =
        LoadPieceTexture(
            "assets/chess/black_queen.png"
        );

    blackKing =
        LoadPieceTexture(
            "assets/chess/black_king.png"
        );


    loaded = true;
}


void UnloadChessPieceAssets()
{
    UnloadIfValid(whitePawn);
    UnloadIfValid(whiteKnight);
    UnloadIfValid(whiteBishop);
    UnloadIfValid(whiteRook);
    UnloadIfValid(whiteQueen);
    UnloadIfValid(whiteKing);

    UnloadIfValid(blackPawn);
    UnloadIfValid(blackKnight);
    UnloadIfValid(blackBishop);
    UnloadIfValid(blackRook);
    UnloadIfValid(blackQueen);
    UnloadIfValid(blackKing);

    loaded = false;
}


bool ChessPieceAssetsLoaded()
{
    return loaded;
}


void DrawChessPiece(
    char piece,
    Rectangle square,
    float padding
)
{
    // Lazy-load so chess works immediately even before main.cpp
    // is updated to load assets explicitly.
    if (!loaded)
    {
        LoadChessPieceAssets();
    }


    Texture2D* texture =
        TextureForPiece(piece);

    if (
        texture == nullptr ||
        texture->id == 0
    )
    {
        return;
    }


    float availableWidth =
        square.width -
        padding * 2.0f;

    float availableHeight =
        square.height -
        padding * 2.0f;


    float scaleX =
        availableWidth /
        (float)texture->width;

    float scaleY =
        availableHeight /
        (float)texture->height;

    float scale =
        min(
            scaleX,
            scaleY
        );


    float drawWidth =
        texture->width *
        scale;

    float drawHeight =
        texture->height *
        scale;


    Rectangle source = {
        0.0f,
        0.0f,
        (float)texture->width,
        (float)texture->height
    };


    Rectangle destination = {
        square.x +
            square.width / 2.0f -
            drawWidth / 2.0f,

        square.y +
            square.height / 2.0f -
            drawHeight / 2.0f,

        drawWidth,
        drawHeight
    };


    DrawTexturePro(
        *texture,
        source,
        destination,
        Vector2{0.0f, 0.0f},
        0.0f,
        WHITE
    );
}

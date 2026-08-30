#include "app/CaveView.h"

#include "app/C64HudPresentation.h"
#include "engine/ObjectCodes.h"

#include <QColor>
#include <QImage>
#include <QPaintEvent>
#include <QPainter>

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

namespace boulderdash::app
{
namespace
{

constexpr int kHudHeight = 24;
// The game-mode raster split at $8613 leaves one C64 scanline between the eight-scanline HUD and the cave.
constexpr int kC64HudCaveMargin = kHudHeight / 8;
constexpr int kGameplayTopHeight = kHudHeight + kC64HudCaveMargin;
constexpr int kC64AtlasFirstTileOffset = 32;
constexpr int kC64AtlasTileSize = 32;
constexpr int kC64AtlasTileStride = 36;
constexpr int kC64AtlasColumns = 16;
constexpr std::size_t kC64CharacterCodeCount = 256;
constexpr int kC64TitleColumns = 40;
constexpr int kC64TitleLogoRows = 19;
constexpr int kC64TitlePanelFirstSourceRow = 2;
constexpr int kC64TitlePanelRows = 17;
constexpr int kC64TitleTextRows = 5;
constexpr int kC64TitleRows = kC64TitlePanelRows + kC64TitleTextRows;
constexpr int kC64TitleNativeCharacterSize = 8;
// The C64 rotates the title background by one character row every four frames. Quarter-row presentation phases
// preserve that cycle duration while making the movement smooth at the presentation cadence.
constexpr int kC64TitleScrollSubframesPerPixel = 4;
constexpr std::size_t kC64TitleScrollPhaseCount = kC64TitleNativeCharacterSize * kC64TitleScrollSubframesPerPixel;
// Preserve the port's 0.8 s cover and 2.05 s reveal at the calibrated PAL SubSecondTick cadence. The resulting
// 146 ticks also align presentation completion with the cycle-timed transition in GameSession.
constexpr std::uint64_t kC64CaveTransitionCoverFrames = 41;
constexpr std::uint64_t kC64CaveTransitionRevealFrames = 105;
constexpr std::uint64_t kC64CaveTransitionFrames = kC64CaveTransitionCoverFrames + kC64CaveTransitionRevealFrames;
constexpr std::uint64_t kC64CaveTransitionSteelScrollFrames = 16;
constexpr engine::CellCode kFirstC64AtlasCharacter = 0x20;
constexpr engine::CellCode kLastC64AtlasCharacter = 0x7f;
constexpr QRgb kC64ReferenceBlack = qRgb(0x00, 0x00, 0x00);
constexpr QRgb kC64ReferenceWhite = qRgb(0xff, 0xff, 0xff);
constexpr QRgb kC64ReferenceOrange = qRgb(0x90, 0x5f, 0x25);
constexpr QRgb kC64ReferenceDarkGray = qRgb(0x55, 0x55, 0x55);
constexpr QRgb kC64ReferencePurple = qRgb(0x8a, 0x46, 0xae);
constexpr QRgb kC64ReferenceLightRed = qRgb(0xbb, 0x77, 0x6d);
constexpr QRgb kC64ReferenceBlue = qRgb(0x3e, 0x31, 0xa2);
constexpr QRgb kC64ReferenceLightBlue = qRgb(0x7c, 0x70, 0xda);
constexpr std::array<std::uint8_t, 20> kC64LevelActiveAnimations = {
    0x00, 0x01, 0x00, 0x02, 0x01, 0x01, 0x05, 0x01, 0x00, 0x01,
    0x01, 0x01, 0x06, 0x03, 0x01, 0x01, 0x02, 0x01, 0x01, 0x01,
};
constexpr std::array<std::uint8_t, 32> kC64IdleRockfordCharset = {
    // Captured at $3260/$3268/$32e0/$32e8 with JoystickStatus = $0f.
    0x00, 0x08, 0x0a, 0x22, 0x22, 0x0a, 0x02, 0x0a, 0x00, 0x20, 0xa0, 0x88, 0x88, 0xa0, 0x80, 0xa0,
    0x23, 0x32, 0x03, 0x02, 0x07, 0x04, 0x04, 0x3c, 0xc8, 0x8c, 0xc0, 0x80, 0xd0, 0x10, 0x10, 0x3c,
};
constexpr std::array<engine::CellCode, kC64TitleColumns * kC64TitleLogoRows> kC64TitleLogoCharacters = {
#include "app/C64TitleLogoData.inc"
};
constexpr std::array<engine::CellCode, 20> kC64TitlePlayerLine = {
    0x11, 0x20, 0x30, 0x2c, 0x21, 0x39, 0x25, 0x32, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
};
constexpr std::array<engine::CellCode, 20> kC64TitleByPeterLiepaLine = {
    0x20, 0x20, 0x22, 0x39, 0x20, 0x30, 0x25, 0x34, 0x25, 0x32,
    0x20, 0x2c, 0x29, 0x25, 0x30, 0x21, 0x20, 0x20, 0x20, 0x20,
};
constexpr std::array<engine::CellCode, 20> kC64TitleWithChrisGreyLine = {
    0x20, 0x20, 0x37, 0x29, 0x34, 0x28, 0x20, 0x23, 0x28, 0x32,
    0x29, 0x33, 0x20, 0x27, 0x32, 0x25, 0x39, 0x20, 0x20, 0x20,
};
constexpr std::array<engine::CellCode, 20> kPcTitlePressEnterToPlayLine = {
    0x30, 0x32, 0x25, 0x33, 0x33, 0x20, 0x25, 0x2e, 0x34, 0x25,
    0x32, 0x20, 0x34, 0x2f, 0x20, 0x30, 0x2c, 0x21, 0x39, 0x20,
};
constexpr std::array<engine::CellCode, 20> kC64TitleCaveAndLevelLine = {
    0x20, 0x23, 0x21, 0x36, 0x25, 0x1a, 0x20, 0x21, 0x20, 0x20,
    0x2c, 0x25, 0x36, 0x25, 0x2c, 0x1a, 0x20, 0x11, 0x20, 0x20,
};
struct Rgb final
{
    int red = 0;
    int green = 0;
    int blue = 0;
};

constexpr std::array<Rgb, 16> kC64PixCenPalette = {
    Rgb{0x00, 0x00, 0x00}, Rgb{0xff, 0xff, 0xff}, Rgb{0x89, 0x40, 0x36}, Rgb{0x7a, 0xbf, 0xc7},
    Rgb{0x8a, 0x46, 0xae}, Rgb{0x68, 0xa9, 0x41}, Rgb{0x3e, 0x31, 0xa2}, Rgb{0xd0, 0xdc, 0x71},
    Rgb{0x90, 0x5f, 0x25}, Rgb{0x5c, 0x47, 0x00}, Rgb{0xbb, 0x77, 0x6d}, Rgb{0x55, 0x55, 0x55},
    Rgb{0x80, 0x80, 0x80}, Rgb{0xac, 0xea, 0x88}, Rgb{0x7c, 0x70, 0xda}, Rgb{0xab, 0xab, 0xab},
};

struct CaveColours final
{
    QColor background0;
    QColor background1;
    QColor background2;
    QColor foreground;
    engine::CellCode colourRamValue = 0;
};

[[nodiscard]] bool operator==(const CaveColours &left, const CaveColours &right)
{
    return left.background0 == right.background0 && left.background1 == right.background1 &&
           left.background2 == right.background2 && left.foreground == right.foreground &&
           left.colourRamValue == right.colourRamValue;
}

[[nodiscard]] CaveColours caveColoursForSnapshot(const engine::GameSnapshot &snapshot)
{
    if (!snapshot.cave.has_value())
    {
        return {CaveView::c64PaletteColour(0), CaveView::c64PaletteColour(0), CaveView::c64PaletteColour(0),
                CaveView::c64PaletteColour(0x09), 0x09};
    }

    const engine::CaveConfiguration &configuration = snapshot.cave->configuration;
    return {CaveView::c64PaletteColour(0), CaveView::c64PaletteColour(configuration.backgroundColour1),
            CaveView::c64PaletteColour(configuration.backgroundColour2),
            CaveView::c64PaletteColour(configuration.foregroundColour), configuration.foregroundColour};
}

[[nodiscard]] CaveColours hudColoursForSnapshot(const engine::GameSnapshot &snapshot)
{
    CaveColours colours = caveColoursForSnapshot(snapshot);
    colours.foreground = CaveView::c64PaletteColour(1);
    colours.colourRamValue = 1;
    return colours;
}

[[nodiscard]] CaveColours c64TitleColours(const QColor &foreground)
{
    // TitleModeVICColourRegs at $8606 sets black, blue, light blue, and white.
    return {CaveView::c64PaletteColour(0x00), CaveView::c64PaletteColour(0x06), CaveView::c64PaletteColour(0x0e),
            foreground, 0x01};
}

const QImage &c64TileAtlas()
{
    static const QImage atlas(QStringLiteral(":/c64/game-char-data.png"));
    Q_ASSERT(!atlas.isNull());
    Q_ASSERT(atlas.size() == QSize(608, 248));
    return atlas;
}

const QImage &titleCharacterAtlas()
{
    static const QImage atlas(QStringLiteral(":/c64/title-char-data.png"));
    Q_ASSERT(!atlas.isNull());
    Q_ASSERT(atlas.size() == QSize(608, 320));
    return atlas;
}

const QImage &rockfordAnimationAtlas()
{
    static const QImage atlas(QStringLiteral(":/c64/rockford-animation-chars.png"));
    Q_ASSERT(!atlas.isNull());
    Q_ASSERT(atlas.size() == QSize(608, 248));
    return atlas;
}

const QImage &animatedCharacterAtlas()
{
    static const QImage atlas(QStringLiteral(":/c64/animated-char-data.png"));
    Q_ASSERT(!atlas.isNull());
    Q_ASSERT(atlas.size() == QSize(608, 320));
    return atlas;
}

class RecolouredGameplayAtlas final
{
  public:
    explicit RecolouredGameplayAtlas(std::array<QRgb, 4> sourcePixelCodes) : sourcePixelCodes_(sourcePixelCodes)
    {
    }

    [[nodiscard]] const QImage &forCave(const QImage &source, const CaveColours &colours)
    {
        if (!image_.isNull() && colours_ == colours)
        {
            return image_;
        }

        image_ = QImage(source.size(), QImage::Format_ARGB32);
        image_.fill(0);

        const bool highResolution = (colours.colourRamValue & 0x08U) == 0;
        std::array<QRgb, 4> multicolourPalette = {};
        if (!highResolution)
        {
            multicolourPalette = {
                colours.background0.rgba(),
                colours.background1.rgba(),
                colours.background2.rgba(),
                CaveView::c64MulticolourCellColour(colours.colourRamValue).rgba(),
            };
        }
        const int rowCount = (source.height() - kC64AtlasFirstTileOffset) / kC64AtlasTileStride;
        const int columnCount = (source.width() - kC64AtlasFirstTileOffset) / kC64AtlasTileStride;
        for (int characterRow = 0; characterRow < rowCount; ++characterRow)
        {
            for (int characterColumn = 0; characterColumn < columnCount; ++characterColumn)
            {
                const int left = kC64AtlasFirstTileOffset + characterColumn * kC64AtlasTileStride;
                const int top = kC64AtlasFirstTileOffset + characterRow * kC64AtlasTileStride;
                for (int line = 0; line < 8; ++line)
                {
                    for (int pair = 0; pair < 4; ++pair)
                    {
                        const QRgb sourcePixel = source.pixel(left + 4 + pair * 8, top + 2 + line * 4);
                        const std::uint8_t pixelCode = sourcePixelCode(sourcePixel);
                        if (highResolution)
                        {
                            fillBlock(left + pair * 8, top + line * 4, 4, 4,
                                      (pixelCode & 0x02U) != 0 ? colours.foreground.rgba()
                                                               : colours.background0.rgba());
                            fillBlock(left + pair * 8 + 4, top + line * 4, 4, 4,
                                      (pixelCode & 0x01U) != 0 ? colours.foreground.rgba()
                                                               : colours.background0.rgba());
                        }
                        else
                        {
                            fillBlock(left + pair * 8, top + line * 4, 8, 4, multicolourPalette[pixelCode]);
                        }
                    }
                }
            }
        }

        colours_ = colours;
        return image_;
    }

  private:
    [[nodiscard]] std::uint8_t sourcePixelCode(const QRgb pixel) const
    {
        for (std::size_t index = 0; index < sourcePixelCodes_.size(); ++index)
        {
            if (pixel == sourcePixelCodes_[index])
            {
                return static_cast<std::uint8_t>(index);
            }
        }

        Q_UNREACHABLE();
    }

    void fillBlock(const int left, const int top, const int width, const int height, const QRgb colour)
    {
        for (int y = top; y < top + height; ++y)
        {
            auto *pixels = reinterpret_cast<QRgb *>(image_.scanLine(y));
            std::fill_n(pixels + left, width, colour);
        }
    }

    std::array<QRgb, 4> sourcePixelCodes_;
    QImage image_;
    CaveColours colours_{};
};

[[nodiscard]] const QImage &recolouredStaticGameplayAtlas(const CaveColours &colours)
{
    // The disassembly image represents C64 bit pairs as black, orange, dark gray, and white.
    static RecolouredGameplayAtlas atlas(
        {kC64ReferenceBlack, kC64ReferenceOrange, kC64ReferenceDarkGray, kC64ReferenceWhite});
    return atlas.forCave(c64TileAtlas(), colours);
}

[[nodiscard]] const QImage &recolouredAnimatedGameplayAtlas(const CaveColours &colours)
{
    // AnimatedCharData uses purple and light red for the 01 and 10 bit-pair values.
    static RecolouredGameplayAtlas atlas(
        {kC64ReferenceBlack, kC64ReferencePurple, kC64ReferenceLightRed, kC64ReferenceWhite});
    return atlas.forCave(animatedCharacterAtlas(), colours);
}

[[nodiscard]] const QImage &recolouredRockfordGameplayAtlas(const CaveColours &colours)
{
    static RecolouredGameplayAtlas atlas(
        {kC64ReferenceBlack, kC64ReferenceOrange, kC64ReferenceDarkGray, kC64ReferenceWhite});
    return atlas.forCave(rockfordAnimationAtlas(), colours);
}

class ComposedGameplayTileCache final
{
  public:
    explicit ComposedGameplayTileCache(const engine::CellCode firstAtlasCharacter)
        : firstAtlasCharacter_(firstAtlasCharacter)
    {
    }

    [[nodiscard]] const QImage &forCave(const QImage &atlas, const engine::CellCode baseCharacter,
                                        const CaveColours &colours, const QSize &tileSize)
    {
        Q_ASSERT(baseCharacter >= firstAtlasCharacter_);
        Q_ASSERT(baseCharacter <= std::numeric_limits<engine::CellCode>::max() - 0x11U);
        Q_ASSERT(tileSize.width() > 0 && tileSize.height() > 0);
        if (!hasColours_ || !(colours_ == colours) || tileSize_ != tileSize)
        {
            tiles_.fill(QImage{});
            colours_ = colours;
            tileSize_ = tileSize;
            hasColours_ = true;
        }

        QImage &tile = tiles_[baseCharacter];
        if (!tile.isNull())
        {
            return tile;
        }

        // Compose at the final display size to preserve the existing odd-size quadrant rounding and avoid scaling
        // the cached tile in the per-cell paint path.
        tile = QImage(tileSize, QImage::Format_ARGB32);
        QPainter tilePainter(&tile);
        const int leftWidth = tileSize.width() / 2;
        const int topHeight = tileSize.height() / 2;
        const std::array<engine::CellCode, 4> characters = {
            baseCharacter,
            static_cast<engine::CellCode>(baseCharacter + 1),
            static_cast<engine::CellCode>(baseCharacter + 0x10),
            static_cast<engine::CellCode>(baseCharacter + 0x11),
        };
        const std::array<QRect, 4> destinations = {
            QRect(0, 0, leftWidth, topHeight),
            QRect(leftWidth, 0, tileSize.width() - leftWidth, topHeight),
            QRect(0, topHeight, leftWidth, tileSize.height() - topHeight),
            QRect(leftWidth, topHeight, tileSize.width() - leftWidth, tileSize.height() - topHeight),
        };
        for (std::size_t index = 0; index < characters.size(); ++index)
        {
            const int characterIndex = static_cast<int>(characters[index] - firstAtlasCharacter_);
            const int column = characterIndex % kC64AtlasColumns;
            const int row = characterIndex / kC64AtlasColumns;
            const QRect source(kC64AtlasFirstTileOffset + column * kC64AtlasTileStride,
                               kC64AtlasFirstTileOffset + row * kC64AtlasTileStride, kC64AtlasTileSize,
                               kC64AtlasTileSize);
            Q_ASSERT(atlas.rect().contains(source));
            tilePainter.drawImage(destinations[index], atlas, source);
        }
        return tile;
    }

  private:
    std::array<QImage, kC64CharacterCodeCount> tiles_;
    CaveColours colours_{};
    QSize tileSize_;
    engine::CellCode firstAtlasCharacter_ = 0;
    bool hasColours_ = false;
};

[[nodiscard]] const QImage &composedStaticGameplayTile(const engine::CellCode baseCharacter, const CaveColours &colours,
                                                       const QSize &tileSize)
{
    static ComposedGameplayTileCache cache(kFirstC64AtlasCharacter);
    return cache.forCave(recolouredStaticGameplayAtlas(colours), baseCharacter, colours, tileSize);
}

[[nodiscard]] const QImage &composedAnimatedGameplayTile(const engine::CellCode baseCharacter,
                                                         const CaveColours &colours, const QSize &tileSize)
{
    static ComposedGameplayTileCache cache(0x00);
    return cache.forCave(recolouredAnimatedGameplayAtlas(colours), baseCharacter, colours, tileSize);
}

[[nodiscard]] const QImage &composedRockfordGameplayTile(const engine::CellCode baseCharacter,
                                                         const CaveColours &colours, const QSize &tileSize)
{
    static ComposedGameplayTileCache cache(0x00);
    return cache.forCave(recolouredRockfordGameplayAtlas(colours), baseCharacter, colours, tileSize);
}

QRect titleCharacterSourceRect(const engine::CellCode character)
{
    Q_ASSERT(character <= 0x7f);
    const int column = character & 0x0f;
    const int row = character >> 4;
    const QRect source(kC64AtlasFirstTileOffset + column * kC64AtlasTileStride,
                       kC64AtlasFirstTileOffset + row * kC64AtlasTileStride, kC64AtlasTileSize, kC64AtlasTileSize);
    Q_ASSERT(titleCharacterAtlas().rect().contains(source));
    return source;
}

[[nodiscard]] std::uint8_t c64TitlePixelCode(const engine::CellCode character, const int column, const int row)
{
    Q_ASSERT(character <= 0x7f);
    Q_ASSERT(column >= 0 && column < 4);
    Q_ASSERT(row >= 0 && row < 8);

    const QRect source = titleCharacterSourceRect(character);
    const QRgb pixel = titleCharacterAtlas().pixel(source.left() + 4 + column * 8, source.top() + 2 + row * 4);
    if (pixel == kC64ReferenceBlack)
    {
        return 0;
    }
    if (pixel == kC64ReferenceBlue)
    {
        return 1;
    }
    if (pixel == kC64ReferenceLightBlue)
    {
        return 2;
    }
    Q_ASSERT(pixel == kC64ReferenceWhite);
    return 3;
}

void drawC64TopLineCharacter(QPainter &painter, const QRect &destination, engine::CellCode character,
                             const std::uint8_t colourRamValue, const CaveColours &colours)
{
    if (character == 0x01)
    {
        character = 0x20;
    }
    Q_ASSERT(character <= 0x7f);

    const bool multicolour = (colourRamValue & 0x08U) != 0;
    const QColor foreground = CaveView::c64PaletteColour(colourRamValue & 0x07U);
    for (int row = 0; row < 8; ++row)
    {
        const int top = destination.top() + row * destination.height() / 8;
        const int bottom = destination.top() + (row + 1) * destination.height() / 8;
        for (int pair = 0; pair < 4; ++pair)
        {
            const std::uint8_t pixel = c64TitlePixelCode(character, pair, row);
            const int left = destination.left() + pair * destination.width() / 4;
            const int right = destination.left() + (pair + 1) * destination.width() / 4;
            if (multicolour)
            {
                const QColor *colour = nullptr;
                switch (pixel)
                {
                case 0:
                    colour = &colours.background0;
                    break;
                case 1:
                    colour = &colours.background1;
                    break;
                case 2:
                    colour = &colours.background2;
                    break;
                case 3:
                    colour = &foreground;
                    break;
                default:
                    Q_UNREACHABLE();
                }
                painter.fillRect(QRect(left, top, right - left, bottom - top), *colour);
                continue;
            }

            const int middle = destination.left() + (pair * 2 + 1) * destination.width() / 8;
            const QColor &leftColour = (pixel & 0x02U) != 0 ? foreground : colours.background0;
            const QColor &rightColour = (pixel & 0x01U) != 0 ? foreground : colours.background0;
            painter.fillRect(QRect(left, top, middle - left, bottom - top), leftColour);
            painter.fillRect(QRect(middle, top, right - middle, bottom - top), rightColour);
        }
    }
}

[[nodiscard]] bool c64TitleCharacterHasScrollingBackground(const engine::CellCode character) noexcept
{
    return character == 0x00 || character == 0x09 || character == 0x0a;
}

[[nodiscard]] std::uint8_t c64TitlePixelCodeWithScrollingBackground(const engine::CellCode character, const int column,
                                                                    const int characterRow, const int scrollingRow)
{
    // RunTitleScreen at $859b-$85a5 copies character $0b to $00 before scrolling it.
    const std::uint8_t scrollingBackgroundPixel = c64TitlePixelCode(0x0b, column, scrollingRow);
    if (character == 0x00)
    {
        return scrollingBackgroundPixel;
    }
    if (character == 0x09)
    {
        return static_cast<std::uint8_t>(c64TitlePixelCode(0x06, column, characterRow) | scrollingBackgroundPixel);
    }
    if (character == 0x0a)
    {
        return static_cast<std::uint8_t>(c64TitlePixelCode(0x07, column, characterRow) | scrollingBackgroundPixel);
    }
    return c64TitlePixelCode(character, column, characterRow);
}

[[nodiscard]] const QColor &c64TitlePixelColour(const std::uint8_t pixel, const CaveColours &colours)
{
    switch (pixel)
    {
    case 0:
        return colours.background0;
    case 1:
        return colours.background1;
    case 2:
        return colours.background2;
    case 3:
        return colours.foreground;
    default:
        Q_UNREACHABLE();
    }
}

void drawC64TitleCharacter(QPainter &painter, const QRect &destination, const engine::CellCode character,
                           const CaveColours &colours)
{
    Q_ASSERT(character <= 0x7f);
    for (int row = 0; row < kC64TitleNativeCharacterSize; ++row)
    {
        for (int column = 0; column < 4; ++column)
        {
            const int left = destination.left() + column * destination.width() / 4;
            const int right = destination.left() + (column + 1) * destination.width() / 4;
            const int top = destination.top() + row * destination.height() / kC64TitleNativeCharacterSize;
            const int bottom = destination.top() + (row + 1) * destination.height() / kC64TitleNativeCharacterSize;
            const std::uint8_t pixel = c64TitlePixelCodeWithScrollingBackground(character, column, row, row);
            painter.fillRect(QRect(left, top, right - left, bottom - top), c64TitlePixelColour(pixel, colours));
        }
    }
}

void drawC64TitleTextLine(QPainter &painter, const QRect &titleRect, const int row,
                          const std::array<engine::CellCode, 20> &characters,
                          const std::array<bool, 20> &lightBlueCharacters)
{
    Q_ASSERT(row >= 0 && row < kC64TitleTextRows);

    const int characterSize = titleRect.width() / kC64TitleColumns;
    const int top = titleRect.top() + (kC64TitlePanelRows + row) * characterSize;
    const CaveColours white = c64TitleColours(CaveView::c64PaletteColour(0x01));
    const CaveColours lightBlue = c64TitleColours(CaveView::c64PaletteColour(0x0e));
    for (std::size_t index = 0; index < characters.size(); ++index)
    {
        const engine::CellCode character = characters[index];
        Q_ASSERT(character <= 0x4b);
        const CaveColours &colours = lightBlueCharacters[index] ? lightBlue : white;
        const int left = titleRect.left() + static_cast<int>(index) * characterSize * 2;
        drawC64TitleCharacter(painter, QRect(left, top, characterSize, characterSize), character, colours);
        drawC64TitleCharacter(painter, QRect(left + characterSize, top, characterSize, characterSize),
                              static_cast<engine::CellCode>(character + 0x34), colours);
    }
}

void drawC64TitleFrame(QPainter &painter, const std::uint8_t playerCount)
{
    Q_ASSERT(playerCount == 1 || playerCount == 2);

    const QRect titleRect(0, 0, kC64TitleColumns * kC64TitleNativeCharacterSize,
                          kC64TitleRows * kC64TitleNativeCharacterSize);
    const int characterSize = kC64TitleNativeCharacterSize;
    const CaveColours white = c64TitleColours(CaveView::c64PaletteColour(0x01));

    for (int row = 0; row < kC64TitlePanelRows; ++row)
    {
        for (int column = 0; column < kC64TitleColumns; ++column)
        {
            const QRect destination(titleRect.left() + column * characterSize, titleRect.top() + row * characterSize,
                                    characterSize, characterSize);
            const engine::CellCode character =
                kC64TitleLogoCharacters[(row + kC64TitlePanelFirstSourceRow) * kC64TitleColumns + column];
            drawC64TitleCharacter(painter, destination, character, white);
        }
    }

    constexpr std::array<bool, 20> kNoLightBlueCharacters = {};
    constexpr std::array<bool, 20> kPlayerNumberLightBlue = {
        true,  false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false,
    };
    constexpr std::array<bool, 20> kCaveAndLevelValuesLightBlue = {
        false, false, false, false, false, false, false, true, false, false,
        false, false, false, false, false, false, false, true, false, false,
    };
    std::array<engine::CellCode, 20> playerLine = kC64TitlePlayerLine;
    playerLine[0] = static_cast<engine::CellCode>(0x10 + playerCount);

    drawC64TitleTextLine(painter, titleRect, 0, kC64TitleByPeterLiepaLine, kNoLightBlueCharacters);
    drawC64TitleTextLine(painter, titleRect, 1, kC64TitleWithChrisGreyLine, kNoLightBlueCharacters);
    drawC64TitleTextLine(painter, titleRect, 2, kPcTitlePressEnterToPlayLine, kNoLightBlueCharacters);
    drawC64TitleTextLine(painter, titleRect, 3, playerLine, kPlayerNumberLightBlue);
    drawC64TitleTextLine(painter, titleRect, 4, kC64TitleCaveAndLevelLine, kCaveAndLevelValuesLightBlue);
}

[[nodiscard]] const QImage &c64StaticTitleFrame(const std::uint8_t playerCount)
{
    Q_ASSERT(playerCount == 1 || playerCount == 2);

    static std::array<QImage, 2> frames;
    const std::size_t playerIndex = playerCount - 1;
    QImage &frame = frames[playerIndex];
    if (!frame.isNull())
    {
        return frame;
    }

    frame = QImage(kC64TitleColumns * kC64TitleNativeCharacterSize, kC64TitleRows * kC64TitleNativeCharacterSize,
                   QImage::Format_ARGB32);
    frame.fill(CaveView::c64PaletteColour(0x00));
    QPainter framePainter(&frame);
    drawC64TitleFrame(framePainter, playerCount);
    return frame;
}

[[nodiscard]] std::size_t c64AnimatedTitleCharacterIndex(const engine::CellCode character)
{
    Q_ASSERT(c64TitleCharacterHasScrollingBackground(character));
    if (character == 0x00)
    {
        return 0;
    }
    if (character == 0x09)
    {
        return 1;
    }
    return 2;
}

[[nodiscard]] const QImage &c64AnimatedTitleCharacterFrame(const engine::CellCode character,
                                                           const std::uint64_t presentationFrame)
{
    Q_ASSERT(c64TitleCharacterHasScrollingBackground(character));

    static std::array<std::array<QImage, kC64TitleScrollPhaseCount>, 3> frames;
    const std::size_t characterIndex = c64AnimatedTitleCharacterIndex(character);
    const std::size_t phase = static_cast<std::size_t>(presentationFrame % kC64TitleScrollPhaseCount);
    QImage &frame = frames[characterIndex][phase];
    if (!frame.isNull())
    {
        return frame;
    }

    frame = QImage(kC64TitleNativeCharacterSize, static_cast<int>(kC64TitleScrollPhaseCount), QImage::Format_ARGB32);
    const CaveColours colours = c64TitleColours(CaveView::c64PaletteColour(0x01));
    QPainter framePainter(&frame);
    for (int subframeRow = 0; subframeRow < static_cast<int>(kC64TitleScrollPhaseCount); ++subframeRow)
    {
        const int characterRow = subframeRow / kC64TitleScrollSubframesPerPixel;
        const int scrollingRow =
            ((subframeRow + static_cast<int>(phase)) / kC64TitleScrollSubframesPerPixel) % kC64TitleNativeCharacterSize;
        for (int column = 0; column < 4; ++column)
        {
            const std::uint8_t pixel =
                c64TitlePixelCodeWithScrollingBackground(character, column, characterRow, scrollingRow);
            framePainter.fillRect(column * 2, subframeRow, 2, 1, c64TitlePixelColour(pixel, colours));
        }
    }
    return frame;
}

[[nodiscard]] const QImage &c64AnimatedTitleOverlayFrame(const std::uint64_t presentationFrame)
{
    // Cache complete vertically supersampled overlays so each title frame needs only one scaled animated draw.
    static std::array<QImage, kC64TitleScrollPhaseCount> frames;
    const std::size_t phase = static_cast<std::size_t>(presentationFrame % kC64TitleScrollPhaseCount);
    QImage &frame = frames[phase];
    if (!frame.isNull())
    {
        return frame;
    }

    frame =
        QImage(kC64TitleColumns * kC64TitleNativeCharacterSize,
               kC64TitlePanelRows * static_cast<int>(kC64TitleScrollPhaseCount), QImage::Format_ARGB32_Premultiplied);
    frame.fill(Qt::transparent);
    QPainter framePainter(&frame);
    for (int row = 0; row < kC64TitlePanelRows; ++row)
    {
        for (int column = 0; column < kC64TitleColumns; ++column)
        {
            const engine::CellCode character =
                kC64TitleLogoCharacters[(row + kC64TitlePanelFirstSourceRow) * kC64TitleColumns + column];
            if (!c64TitleCharacterHasScrollingBackground(character))
            {
                continue;
            }

            const QRect destination(column * kC64TitleNativeCharacterSize,
                                    row * static_cast<int>(kC64TitleScrollPhaseCount), kC64TitleNativeCharacterSize,
                                    static_cast<int>(kC64TitleScrollPhaseCount));
            framePainter.drawImage(destination, c64AnimatedTitleCharacterFrame(character, phase));
        }
    }
    return frame;
}

void drawC64Title(QPainter &painter, const QRect &viewRect, const std::uint8_t playerCount,
                  const std::uint64_t presentationFrame)
{
    const int characterSize =
        std::max(1, std::min(viewRect.width() / kC64TitleColumns, viewRect.height() / kC64TitleRows));
    const QSize titleSize(kC64TitleColumns * characterSize, kC64TitleRows * characterSize);
    const QRect titleRect(viewRect.center() - QPoint(titleSize.width() / 2, titleSize.height() / 2), titleSize);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.drawImage(titleRect, c64StaticTitleFrame(playerCount));

    const QRect animatedTitleRect(titleRect.left(), titleRect.top(), titleRect.width(),
                                  kC64TitlePanelRows * characterSize);
    painter.drawImage(animatedTitleRect, c64AnimatedTitleOverlayFrame(presentationFrame));
}

[[nodiscard]] bool hudUsesPlayerLine(const engine::GameSnapshot &snapshot)
{
    if (snapshot.phase == engine::SessionPhase::RockfordAppearing)
    {
        Q_ASSERT(snapshot.runtime.has_value());
        // AnimateGameCharsAndHandleTopLineSwitch ($78c9-$78d1) switches to
        // the score as soon as the entry countdown ends, before Rockford's
        // four appearance cells have finished advancing.
        return snapshot.runtime->appearanceCountdown != 0;
    }

    return snapshot.phase == engine::SessionPhase::CavePrepared ||
           snapshot.phase == engine::SessionPhase::CaveTransitioning ||
           snapshot.phase == engine::SessionPhase::Transitioning;
}

class RenderedC64HudCache final
{
  public:
    [[nodiscard]] const QImage &forLine(const int viewWidth,
                                        const std::array<engine::CellCode, kC64HudCharacterColumns> &characters,
                                        const C64HudColourRamLine &colourRam, const CaveColours &colours)
    {
        Q_ASSERT(viewWidth > 0);
        if (!image_.isNull() && image_.width() == viewWidth && characters_ == characters && colourRam_ == colourRam &&
            colours_ == colours)
        {
            return image_;
        }

        image_ = QImage(viewWidth, kHudHeight, QImage::Format_ARGB32);
        QPainter hudPainter(&image_);
        hudPainter.fillRect(image_.rect(), CaveView::c64PaletteColour(0));
        const int characterHeight =
            std::max(1, std::min(kHudHeight, viewWidth / static_cast<int>(kC64HudCharacterColumns * 2U)));
        const int characterWidth = characterHeight * 2;
        const int hudWidth = static_cast<int>(kC64HudCharacterColumns) * characterWidth;
        const int left = (viewWidth - hudWidth) / 2;
        const int top = (kHudHeight - characterHeight) / 2;
        for (std::size_t column = 0; column < characters.size(); ++column)
        {
            const int characterLeft = left + static_cast<int>(column) * characterWidth;
            const int halfWidth = characterWidth / 2;
            const QRect leftCharacter(characterLeft, top, halfWidth, characterHeight);
            const QRect rightCharacter(characterLeft + halfWidth, top, characterWidth - halfWidth, characterHeight);
            drawC64TopLineCharacter(hudPainter, leftCharacter, characters[column], colourRam[column * 2], colours);
            drawC64TopLineCharacter(hudPainter, rightCharacter,
                                    static_cast<engine::CellCode>(characters[column] + 0x34), colourRam[column * 2 + 1],
                                    colours);
        }

        characters_ = characters;
        colourRam_ = colourRam;
        colours_ = colours;
        return image_;
    }

  private:
    QImage image_;
    std::array<engine::CellCode, kC64HudCharacterColumns> characters_{};
    C64HudColourRamLine colourRam_{};
    CaveColours colours_{};
};

void drawC64Hud(QPainter &painter, const engine::GameSnapshot &snapshot, const int viewWidth,
                const CaveColours &colours)
{
    const C64HudPresentation presentation = c64HudPresentationForSnapshot(snapshot);
    const bool usesPlayerLine = hudUsesPlayerLine(snapshot);
    const std::array<engine::CellCode, kC64HudCharacterColumns> &characters =
        usesPlayerLine ? presentation.playerLine : presentation.scoreLine;
    const C64HudColourRamLine &colourRam =
        usesPlayerLine ? presentation.playerLineColourRam : presentation.scoreLineColourRam;
    static RenderedC64HudCache cache;
    painter.drawImage(QPoint(0, 0), cache.forLine(viewWidth, characters, colourRam, colours));
}

void drawC64Tile(QPainter &painter, const QRect &cell, const engine::CellCode baseCharacter, const CaveColours &colours)
{
    Q_ASSERT(baseCharacter >= kFirstC64AtlasCharacter && baseCharacter <= 0x6e);
    painter.drawImage(cell.topLeft(), composedStaticGameplayTile(baseCharacter, colours, cell.size()));
}

[[nodiscard]] const QImage &composedScrollingSteelCoverFrame(const CaveColours &colours, const QSize &tileSize,
                                                             const std::uint64_t presentationFrame)
{
    static std::array<QImage, static_cast<std::size_t>(kC64CaveTransitionSteelScrollFrames)> frames;
    static CaveColours cachedColours;
    static QSize cachedSize;
    static bool hasConfiguration = false;
    Q_ASSERT(tileSize.width() > 0 && tileSize.height() > 0);
    if (!hasConfiguration || !(cachedColours == colours) || cachedSize != tileSize)
    {
        frames.fill(QImage{});
        cachedColours = colours;
        cachedSize = tileSize;
        hasConfiguration = true;
    }

    const std::size_t phase = static_cast<std::size_t>(presentationFrame % kC64CaveTransitionSteelScrollFrames);
    QImage &frame = frames[phase];
    if (!frame.isNull())
    {
        return frame;
    }

    // The image paint device clips both shifted tiles to the cell bounds during this one-time composition.
    frame = QImage(tileSize, QImage::Format_ARGB32);
    QPainter framePainter(&frame);
    const QImage &steelTile = composedStaticGameplayTile(0x2e, colours, tileSize);
    const int offset =
        static_cast<int>(phase) * tileSize.height() / static_cast<int>(kC64CaveTransitionSteelScrollFrames);
    framePainter.drawImage(QPoint(0, -offset), steelTile);
    framePainter.drawImage(QPoint(0, tileSize.height() - offset), steelTile);
    return frame;
}

void drawScrollingSteelCover(QPainter &painter, const QRect &cell, const CaveColours &colours,
                             const std::uint64_t presentationFrame)
{
    painter.drawImage(cell.topLeft(), composedScrollingSteelCoverFrame(colours, cell.size(), presentationFrame));
}

void drawRockford(QPainter &painter, const QRect &cell, const bool facesLeft, const std::uint8_t animationFrame,
                  const CaveColours &colours)
{
    const engine::CellCode familyBase = facesLeft ? 0x20 : 0x40;
    // AnimateGameCharsAndHandleTopLineSwitch ($78d4-$7955) selects one of eight phases, each stored as two
    // adjacent characters per row in RockfordAnimationChars at $63e8.
    const engine::CellCode baseCharacter = static_cast<engine::CellCode>(familyBase + (animationFrame % 8U) * 2U);
    painter.drawImage(cell.topLeft(), composedRockfordGameplayTile(baseCharacter, colours, cell.size()));
}

[[nodiscard]] const QColor &multicolourPixelColour(const std::uint8_t pixel, const CaveColours &colours)
{
    switch (pixel)
    {
    case 0:
        return colours.background0;
    case 1:
        return colours.background1;
    case 2:
        return colours.background2;
    case 3:
        return colours.foreground;
    }

    Q_UNREACHABLE();
}

void drawC64CharacterBytes(QPainter &painter, const QRect &destination, const std::array<std::uint8_t, 8> &bytes,
                           const CaveColours &colours)
{
    const bool highResolution = (colours.colourRamValue & 0x08U) == 0;
    for (int row = 0; row < 8; ++row)
    {
        const int top = destination.top() + row * destination.height() / 8;
        const int bottom = destination.top() + (row + 1) * destination.height() / 8;
        const std::uint8_t line = bytes[static_cast<std::size_t>(row)];
        const int pixelCount = highResolution ? 8 : 4;
        for (int pixel = 0; pixel < pixelCount; ++pixel)
        {
            const int left = destination.left() + pixel * destination.width() / pixelCount;
            const int right = destination.left() + (pixel + 1) * destination.width() / pixelCount;
            const QColor *colour = nullptr;
            if (highResolution)
            {
                colour = (line & (0x80U >> pixel)) != 0 ? &colours.foreground : &colours.background0;
            }
            else
            {
                const std::uint8_t pair = static_cast<std::uint8_t>((line >> ((3 - pixel) * 2)) & 0x03U);
                colour = &multicolourPixelColour(pair, colours);
            }
            painter.fillRect(QRect(left, top, right - left, bottom - top), *colour);
        }
    }
}

[[nodiscard]] const QImage &composedIdleRockfordGameplayTile(const CaveColours &colours, const QSize &tileSize)
{
    static QImage tile;
    static CaveColours cachedColours;
    static QSize cachedSize;
    if (!tile.isNull() && cachedColours == colours && cachedSize == tileSize)
    {
        return tile;
    }

    Q_ASSERT(tileSize.width() > 0 && tileSize.height() > 0);
    tile = QImage(tileSize, QImage::Format_ARGB32);
    cachedColours = colours;
    cachedSize = tileSize;
    QPainter tilePainter(&tile);
    const int leftWidth = tileSize.width() / 2;
    const int topHeight = tileSize.height() / 2;
    const std::array<QRect, 4> quadrants = {
        QRect(0, 0, leftWidth, topHeight),
        QRect(leftWidth, 0, tileSize.width() - leftWidth, topHeight),
        QRect(0, topHeight, leftWidth, tileSize.height() - topHeight),
        QRect(leftWidth, topHeight, tileSize.width() - leftWidth, tileSize.height() - topHeight),
    };

    for (std::size_t characterIndex = 0; characterIndex < quadrants.size(); ++characterIndex)
    {
        std::array<std::uint8_t, 8> bytes = {};
        std::copy_n(kC64IdleRockfordCharset.begin() + characterIndex * bytes.size(), bytes.size(), bytes.begin());
        drawC64CharacterBytes(tilePainter, quadrants[characterIndex], bytes, colours);
    }
    return tile;
}

void drawIdleRockford(QPainter &painter, const QRect &cell, const CaveColours &colours)
{
    painter.drawImage(cell.topLeft(), composedIdleRockfordGameplayTile(colours, cell.size()));
}

void drawAnimatedTile(QPainter &painter, const QRect &cell, const engine::CellCode familyBase,
                      const std::uint8_t animationFrame, const CaveColours &colours)
{
    const engine::CellCode base = static_cast<engine::CellCode>(familyBase + (animationFrame % 8) * 2);
    painter.drawImage(cell.topLeft(), composedAnimatedGameplayTile(base, colours, cell.size()));
}

void drawCell(QPainter &painter, const QRect &cell, const engine::CellCode code, const engine::GameCommand command,
              const bool rockfordFacesLeft, const std::uint8_t animationFrame, const std::uint8_t animationFlags,
              const std::uint8_t flashingEntryBoxState, const CaveColours &colours)
{
    using namespace engine::objectcodes;

    painter.fillRect(cell, colours.background0);
    if ((code == kOpenOutbox || code == kInbox) && CaveView::c64FlashingDoorShowsSteel(flashingEntryBoxState))
    {
        drawC64Tile(painter, cell, 0x2e, colours);
        return;
    }
    if (code == kRockford || code == kScannedRockford)
    {
        if (command.direction == engine::Direction::Neutral)
        {
            drawIdleRockford(painter, cell, colours);
        }
        else
        {
            drawRockford(painter, cell, rockfordFacesLeft, animationFrame, colours);
        }
        return;
    }
    if ((code == kAmoeba || code == kScannedAmoeba) && animationFlags >= 0x04)
    {
        drawAnimatedTile(painter, cell, 0x00, animationFrame, colours);
        return;
    }
    if (code >= kFireflyLeft && code <= 0x0f && (animationFlags & 0x01U) != 0)
    {
        drawAnimatedTile(painter, cell, 0x20, animationFrame, colours);
        return;
    }
    if (code >= 0x30 && code <= 0x37 && (animationFlags & 0x02U) != 0)
    {
        drawAnimatedTile(painter, cell, 0x60, animationFrame, colours);
        return;
    }
    if (code == kStationaryDiamond || code == kScannedStationaryDiamond || code == kFallingDiamond ||
        code == kScannedFallingDiamond)
    {
        drawAnimatedTile(painter, cell, 0x40, animationFrame, colours);
        return;
    }
    const engine::CellCode character = baseCharacterForObject(code);
    if (character >= kFirstC64AtlasCharacter && character <= kLastC64AtlasCharacter)
    {
        drawC64Tile(painter, cell, character, colours);
        return;
    }
}

} // namespace

CaveView::CaveView(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(640, 440);
    setAutoFillBackground(false);
}

std::uint8_t CaveView::c64AnimationFlagsForCave(const engine::CellCode caveNumber)
{
    Q_ASSERT(caveNumber >= 1 && caveNumber <= 20);
    return caveNumber >= 17 ? 0x07 : kC64LevelActiveAnimations[caveNumber - 1];
}

QColor CaveView::c64PaletteColour(const engine::CellCode colourCode)
{
    const Rgb &colour = kC64PixCenPalette[colourCode & 0x0fU];
    return {colour.red, colour.green, colour.blue};
}

QColor CaveView::c64MulticolourCellColour(const engine::CellCode colourRamValue)
{
    // In VIC-II multicolour character mode, Color RAM bit 3 is set and
    // the displayed per-cell colour is encoded by its low three bits.
    Q_ASSERT((colourRamValue & 0x08U) != 0);
    return c64PaletteColour(colourRamValue & 0x07U);
}

bool CaveView::c64FlashingDoorShowsSteel(const std::uint8_t flashingEntryBoxState)
{
    // ProcessInAndOutBoxes at $7346-$735a renders steel for odd values.
    return (flashingEntryBoxState & 0x01U) != 0;
}

std::uint8_t CaveView::c64AnimationFrameForPresentationSubSecondFrame(const std::uint64_t frame)
{
    // Animate runs on every odd AnotherFrameCounter value and increments the
    // C64 phase before copying the character data.
    return static_cast<std::uint8_t>((frame / 2U + frame % 2U) % 8U);
}

engine::CellCode CaveView::c64TitleLogoCharacter(const std::uint8_t column, const std::uint8_t row)
{
    Q_ASSERT(column < kC64TitleColumns);
    Q_ASSERT(row < kC64TitleLogoRows);
    return kC64TitleLogoCharacters[static_cast<std::size_t>(row) * kC64TitleColumns + column];
}

void CaveView::setSnapshot(engine::GameSnapshot snapshot)
{
    const bool startsCaveTransition =
        snapshot.phase == engine::SessionPhase::CaveTransitioning &&
        (!snapshot_.has_value() || snapshot_->phase != engine::SessionPhase::CaveTransitioning);
    if (startsCaveTransition)
    {
        initialCaveRevealActive_ = false;
        beginCaveTransitionReveal(snapshot);
    }
    else if (snapshot.phase != engine::SessionPhase::CaveTransitioning && !initialCaveRevealActive_)
    {
        caveTransitionRevealActive_ = false;
    }

    switch (snapshot.lastCommand.direction)
    {
    case engine::Direction::East:
    case engine::Direction::NorthEast:
    case engine::Direction::SouthEast:
        rockfordFacesLeft_ = false;
        break;
    case engine::Direction::West:
    case engine::Direction::NorthWest:
    case engine::Direction::SouthWest:
        rockfordFacesLeft_ = true;
        break;
    default:
        break;
    }
    snapshot_ = std::move(snapshot);
    update();
}

void CaveView::setPresentationSubSecondFrame(const std::uint64_t frame)
{
    const std::uint8_t animationFrame = c64AnimationFrameForPresentationSubSecondFrame(frame);
    const bool transitionFrameChanged = caveTransitionRevealIsActive() && presentationSubSecondFrame_ != frame;
    presentationSubSecondFrame_ = frame;
    if (initialCaveRevealActive_ && presentationSubSecondFrame_ - caveTransitionStartFrame_ >= kC64CaveTransitionFrames)
    {
        initialCaveRevealActive_ = false;
        caveTransitionRevealActive_ = false;
        caveTransitionRevealEventByCell_.clear();
    }
    if (animationFrame_ == animationFrame && !transitionFrameChanged)
    {
        return;
    }

    animationFrame_ = animationFrame;
    update();
}

void CaveView::setTitlePresentationFrame(const std::uint64_t frame)
{
    if (titlePresentationFrame_ == frame)
    {
        return;
    }

    titlePresentationFrame_ = frame;
    update();
}

void CaveView::setTitleVisible(const bool visible)
{
    titleVisible_ = visible;
    update();
}

void CaveView::startInitialCaveReveal()
{
    if (!snapshot_.has_value())
    {
        return;
    }

    initialCaveRevealActive_ = true;
    beginCaveTransitionReveal(*snapshot_);
    update();
}

void CaveView::beginCaveTransitionReveal(const engine::GameSnapshot &snapshot)
{
    const engine::CaveSize size = snapshot.grid.size();
    Q_ASSERT(size.width > 0 && size.height > 0);
    Q_ASSERT(size.width <= static_cast<std::size_t>(std::numeric_limits<int>::max()));
    Q_ASSERT(size.height <= static_cast<std::size_t>(std::numeric_limits<int>::max()));

    caveTransitionRevealActive_ = true;
    caveTransitionStartFrame_ = presentationSubSecondFrame_;
    caveTransitionRevealEventByCell_.assign(size.width * size.height, std::numeric_limits<std::size_t>::max());

    std::uint32_t random = 0x9e3779b9U;
    if (snapshot.cave.has_value())
    {
        random ^= static_cast<std::uint32_t>(snapshot.cave->caveNumber) << 24U;
        random ^= static_cast<std::uint32_t>(snapshot.cave->sublevelIndex) << 16U;
    }

    std::size_t revealEvent = 0;
    for (std::size_t pass = 0; pass < 69; ++pass)
    {
        for (std::size_t y = 0; y < size.height; ++y)
        {
            random = random * 1664525U + 1013904223U;
            const std::size_t x = static_cast<std::size_t>(random) % size.width;
            const std::size_t index = y * size.width + x;
            caveTransitionRevealEventByCell_[index] = std::min(caveTransitionRevealEventByCell_[index], revealEvent);
            ++revealEvent;
        }
    }
}

bool CaveView::caveTransitionRevealIsActive() const noexcept
{
    if (!caveTransitionRevealActive_)
    {
        return false;
    }
    if (!initialCaveRevealActive_)
    {
        return snapshot_.has_value() && snapshot_->phase == engine::SessionPhase::CaveTransitioning;
    }
    return presentationSubSecondFrame_ - caveTransitionStartFrame_ < kC64CaveTransitionFrames;
}

bool CaveView::caveTransitionCellIsRevealed(const std::size_t x, const std::size_t y,
                                            const engine::CaveSize &size) const noexcept
{
    const std::uint64_t elapsedFrames = presentationSubSecondFrame_ - caveTransitionStartFrame_;
    if (elapsedFrames >= kC64CaveTransitionFrames)
    {
        return true;
    }
    if (elapsedFrames < kC64CaveTransitionCoverFrames)
    {
        return false;
    }

    const std::uint64_t revealFrames = elapsedFrames - kC64CaveTransitionCoverFrames;
    const std::size_t eventCount = static_cast<std::size_t>(
        revealFrames * 69U * static_cast<std::uint64_t>(size.height) / kC64CaveTransitionRevealFrames);
    const std::size_t index = y * size.width + x;
    Q_ASSERT(index < caveTransitionRevealEventByCell_.size());
    return caveTransitionRevealEventByCell_[index] < eventCount;
}

void CaveView::setTitlePlayerCount(const std::uint8_t playerCount)
{
    titlePlayerCount_ = playerCount;
    update();
}

void CaveView::setPaused(const bool paused)
{
    paused_ = paused;
    update();
}

void CaveView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.fillRect(rect(), c64PaletteColour(0));
    if (titleVisible_)
    {
        drawC64Title(painter, rect(), titlePlayerCount_, titlePresentationFrame_);
        return;
    }
    if (!snapshot_.has_value())
    {
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter, tr("Preparing cave…"));
        return;
    }

    const engine::GameSnapshot &snapshot = *snapshot_;
    const engine::CaveSize size = snapshot.grid.size();
    Q_ASSERT(size.width > 0 && size.height > 0);
    Q_ASSERT(size.width <= static_cast<std::size_t>(std::numeric_limits<int>::max()));
    Q_ASSERT(size.height <= static_cast<std::size_t>(std::numeric_limits<int>::max()));
    const int caveColumns = static_cast<int>(size.width);
    const int caveRows = static_cast<int>(size.height);
    const int tileSize = std::max(1, std::min(width() / caveColumns, (height() - kGameplayTopHeight) / caveRows));
    const int caveWidth = caveColumns * tileSize;
    const int caveLeft = (width() - caveWidth) / 2;
    const int caveTop = kGameplayTopHeight;

    Q_ASSERT(snapshot.campaign.currentPlayer < snapshot.campaign.players.size());
    const std::uint8_t animationFlags =
        snapshot.cave.has_value() ? c64AnimationFlagsForCave(snapshot.cave->caveNumber) : 0;
    const CaveColours colours = caveColoursForSnapshot(snapshot);
    drawC64Hud(painter, snapshot, width(), hudColoursForSnapshot(snapshot));

    const bool transitionActive = caveTransitionRevealIsActive();
    for (std::size_t y = 0; y < size.height; ++y)
    {
        for (std::size_t x = 0; x < size.width; ++x)
        {
            const QRect cell(caveLeft + static_cast<int>(x) * tileSize, caveTop + static_cast<int>(y) * tileSize,
                             tileSize, tileSize);
            if (transitionActive && !caveTransitionCellIsRevealed(x, y, size))
            {
                drawScrollingSteelCover(painter, cell, colours, presentationSubSecondFrame_);
                continue;
            }
            drawCell(painter, cell, snapshot.grid.at({x, y}), snapshot.lastCommand, rockfordFacesLeft_, animationFrame_,
                     animationFlags, snapshot.campaign.flashingEntryBoxState, colours);
        }
    }
    if (paused_)
    {
        painter.fillRect(rect(), QColor(0, 0, 0, 150));
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter, tr("PAUSED — Press Space to Resume"));
    }
}

} // namespace boulderdash::app

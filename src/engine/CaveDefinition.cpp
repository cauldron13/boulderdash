#include "engine/CaveDefinition.h"

#include <stdexcept>

namespace boulderdash::engine
{
namespace
{

constexpr std::size_t kCaveHeaderSize = 32;
constexpr CellCode kCommandTerminator = 0xff;

#include "engine/CaveDataGenerated.inc"

[[nodiscard]] CellCode readByte(const std::vector<CellCode> &encoded, const std::size_t offset)
{
    if (offset >= encoded.size())
    {
        throw std::invalid_argument("The cave definition ends inside a command.");
    }

    return encoded[offset];
}

[[nodiscard]] std::size_t commandSize(const CellCode command)
{
    switch (command & 0xc0U)
    {
    case 0x00:
        return 3;
    case 0x40:
        return 5;
    case 0x80:
        return 6;
    case 0xc0:
        return 5;
    }

    throw std::logic_error("The cave command mask is invalid.");
}

[[nodiscard]] CaveDrawCommandKind commandKind(const CellCode command)
{
    switch (command & 0xc0U)
    {
    case 0x00:
        return CaveDrawCommandKind::SingleObject;
    case 0x40:
        return CaveDrawCommandKind::Line;
    case 0x80:
        return CaveDrawCommandKind::FilledRectangle;
    case 0xc0:
        return CaveDrawCommandKind::Rectangle;
    }

    throw std::logic_error("The cave command mask is invalid.");
}

} // namespace

CaveDefinition decodeCaveDefinition(const std::vector<CellCode> &encoded)
{
    if (encoded.size() <= kCaveHeaderSize)
    {
        throw std::invalid_argument("A cave definition must contain a header and a command terminator.");
    }

    CaveDefinition definition;
    definition.header.caveNumber = readByte(encoded, 0);
    definition.header.magicWallMillingTimeOrAmoeba3PercentMax = readByte(encoded, 1);
    definition.header.initialDiamondValue = readByte(encoded, 2);
    definition.header.extraDiamondValue = readByte(encoded, 3);

    for (std::size_t index = 0; index < kCaveSublevelCount; ++index)
    {
        definition.header.initialRandomSeeds[index] = readByte(encoded, 4 + index);
        definition.header.diamondsRequired[index] = readByte(encoded, 9 + index);
        definition.header.caveTimes[index] = readByte(encoded, 14 + index);
    }

    definition.header.backgroundColour1 = readByte(encoded, 19);
    definition.header.backgroundColour2 = readByte(encoded, 20);
    definition.header.foregroundColour = readByte(encoded, 21);
    definition.header.reservedBytes[0] = readByte(encoded, 22);
    definition.header.reservedBytes[1] = readByte(encoded, 23);

    for (std::size_t index = 0; index < kCaveRandomObjectCount; ++index)
    {
        definition.header.randomObjects[index] = readByte(encoded, 24 + index);
        definition.header.randomObjectProbabilities[index] = readByte(encoded, 28 + index);
    }

    std::size_t offset = kCaveHeaderSize;
    while (true)
    {
        const CellCode command = readByte(encoded, offset);
        if (command == kCommandTerminator)
        {
            if (offset + 1 != encoded.size())
            {
                throw std::invalid_argument("The cave definition contains bytes after its command terminator.");
            }
            break;
        }

        const std::size_t size = commandSize(command);
        if (encoded.size() - offset < size)
        {
            throw std::invalid_argument("The cave definition ends inside a command.");
        }

        CaveDrawCommand parsedCommand;
        parsedCommand.kind = commandKind(command);
        parsedCommand.object = static_cast<CellCode>(command & 0x3fU);
        parsedCommand.x = readByte(encoded, offset + 1);
        parsedCommand.y = readByte(encoded, offset + 2);
        if (size > 3)
        {
            parsedCommand.parameter3 = readByte(encoded, offset + 3);
            parsedCommand.parameter4 = readByte(encoded, offset + 4);
        }
        if (size > 5)
        {
            parsedCommand.parameter5 = readByte(encoded, offset + 5);
        }

        definition.drawCommands.push_back(parsedCommand);
        offset += size;
    }

    return definition;
}

const CaveDefinition &caveA()
{
    return caveDefinition(1);
}

const CaveDefinition &caveDefinition(const CellCode caveNumber)
{
    static const std::array<CaveDefinition, 20> definitions = {
        decodeCaveDefinition({kCave1Encoded.begin(), kCave1Encoded.end()}),
        decodeCaveDefinition({kCave2Encoded.begin(), kCave2Encoded.end()}),
        decodeCaveDefinition({kCave3Encoded.begin(), kCave3Encoded.end()}),
        decodeCaveDefinition({kCave4Encoded.begin(), kCave4Encoded.end()}),
        decodeCaveDefinition({kCave5Encoded.begin(), kCave5Encoded.end()}),
        decodeCaveDefinition({kCave6Encoded.begin(), kCave6Encoded.end()}),
        decodeCaveDefinition({kCave7Encoded.begin(), kCave7Encoded.end()}),
        decodeCaveDefinition({kCave8Encoded.begin(), kCave8Encoded.end()}),
        decodeCaveDefinition({kCave9Encoded.begin(), kCave9Encoded.end()}),
        decodeCaveDefinition({kCave10Encoded.begin(), kCave10Encoded.end()}),
        decodeCaveDefinition({kCave11Encoded.begin(), kCave11Encoded.end()}),
        decodeCaveDefinition({kCave12Encoded.begin(), kCave12Encoded.end()}),
        decodeCaveDefinition({kCave13Encoded.begin(), kCave13Encoded.end()}),
        decodeCaveDefinition({kCave14Encoded.begin(), kCave14Encoded.end()}),
        decodeCaveDefinition({kCave15Encoded.begin(), kCave15Encoded.end()}),
        decodeCaveDefinition({kCave16Encoded.begin(), kCave16Encoded.end()}),
        decodeCaveDefinition({kCave17Encoded.begin(), kCave17Encoded.end()}),
        decodeCaveDefinition({kCave18Encoded.begin(), kCave18Encoded.end()}),
        decodeCaveDefinition({kCave19Encoded.begin(), kCave19Encoded.end()}),
        decodeCaveDefinition({kCave20Encoded.begin(), kCave20Encoded.end()})};
    if (caveNumber == 0 || caveNumber > definitions.size())
    {
        throw std::invalid_argument("The C64 cave number is outside the campaign range.");
    }
    return definitions[caveNumber - 1];
}

} // namespace boulderdash::engine

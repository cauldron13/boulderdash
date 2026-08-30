#include "engine/GameSession.h"

#include "engine/AmoebaRules.h"
#include "engine/CavePreparation.h"
#include "engine/CaveScanner.h"
#include "engine/CaveUpdateContext.h"
#include "engine/EngineTiming.h"
#include "engine/MagicWallRules.h"
#include "engine/ProgressionRules.h"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <utility>

namespace boulderdash::engine
{
namespace
{

constexpr std::array<std::uint8_t, 5> kC64PalCaveTicksPer600SubSecondTicks = {79, 90, 99, 104, 106};
constexpr CellCode kC64CaveBNumber = 2;
constexpr std::uint8_t kC64SinglePlayerEntryAppearanceSeconds = 4;
constexpr std::uint8_t kC64TwoPlayerSingleInputEntryAppearanceSeconds = 6;
constexpr std::uint8_t kC64HudDiamondDisplayMaximum = 99;

struct PresentationFingerprint final
{
    std::uint64_t gridRevision = 0;
    SessionPhase phase = SessionPhase::Uninitialized;
    Direction lastDirection = Direction::Neutral;
    bool hasCave = false;
    CellCode caveNumber = 0;
    CellCode diamondValue = 0;
    CellCode diamondsRequired = 0;
    CellCode backgroundColour1 = 0;
    CellCode backgroundColour2 = 0;
    CellCode foregroundColour = 0;
    bool hasRuntime = false;
    bool entryAppearanceComplete = false;
    bool exitOpen = false;
    bool amoebaGrowing = false;
    bool magicWallActive = false;
    std::uint16_t collectedDiamonds = 0;
    std::uint16_t timeRemainingSeconds = 0;
    std::uint8_t currentPlayer = 0;
    std::uint32_t currentPlayerScore = 0;
    std::uint8_t currentPlayerLives = 0;
    std::uint8_t sublevelIndex = 0;
    CellCode pendingCaveNumber = 0;
    bool flashingEntryBoxShowsSteel = false;
};

bool operator==(const PresentationFingerprint &left, const PresentationFingerprint &right)
{
    return left.gridRevision == right.gridRevision && left.phase == right.phase &&
           left.lastDirection == right.lastDirection && left.hasCave == right.hasCave &&
           left.caveNumber == right.caveNumber && left.diamondValue == right.diamondValue &&
           left.diamondsRequired == right.diamondsRequired && left.backgroundColour1 == right.backgroundColour1 &&
           left.backgroundColour2 == right.backgroundColour2 && left.foregroundColour == right.foregroundColour &&
           left.hasRuntime == right.hasRuntime && left.entryAppearanceComplete == right.entryAppearanceComplete &&
           left.exitOpen == right.exitOpen && left.amoebaGrowing == right.amoebaGrowing &&
           left.magicWallActive == right.magicWallActive && left.collectedDiamonds == right.collectedDiamonds &&
           left.timeRemainingSeconds == right.timeRemainingSeconds && left.currentPlayer == right.currentPlayer &&
           left.currentPlayerScore == right.currentPlayerScore && left.currentPlayerLives == right.currentPlayerLives &&
           left.sublevelIndex == right.sublevelIndex && left.pendingCaveNumber == right.pendingCaveNumber &&
           left.flashingEntryBoxShowsSteel == right.flashingEntryBoxShowsSteel;
}

PresentationFingerprint presentationFingerprint(const GameState &state)
{
    PresentationFingerprint fingerprint;
    fingerprint.gridRevision = state.grid.revision();
    fingerprint.phase = state.phase;
    fingerprint.lastDirection = state.lastCommand.direction;
    fingerprint.currentPlayer = state.campaign.currentPlayer;
    fingerprint.sublevelIndex = state.campaign.sublevelIndex;
    fingerprint.pendingCaveNumber = state.campaign.pendingCaveNumber;
    fingerprint.flashingEntryBoxShowsSteel = (state.campaign.flashingEntryBoxState & 0x01U) != 0;

    if (state.campaign.currentPlayer < state.campaign.players.size())
    {
        const PlayerProgress &currentPlayer = state.campaign.players[state.campaign.currentPlayer];
        fingerprint.currentPlayerScore = currentPlayer.score;
        fingerprint.currentPlayerLives = currentPlayer.lives;
    }

    if (state.cave.has_value())
    {
        const CaveMetadata &cave = *state.cave;
        fingerprint.hasCave = true;
        fingerprint.caveNumber = cave.caveNumber;
        fingerprint.diamondValue = std::min(cave.configuration.initialDiamondValue, kC64HudDiamondDisplayMaximum);
        fingerprint.diamondsRequired = std::min(cave.configuration.diamondsRequired, kC64HudDiamondDisplayMaximum);
        fingerprint.backgroundColour1 = cave.configuration.backgroundColour1;
        fingerprint.backgroundColour2 = cave.configuration.backgroundColour2;
        fingerprint.foregroundColour = cave.configuration.foregroundColour;
    }

    if (state.runtime.has_value())
    {
        fingerprint.hasRuntime = true;
        fingerprint.entryAppearanceComplete = state.runtime->appearanceCountdown == 0;
        fingerprint.exitOpen = state.runtime->exitOpen;
        fingerprint.amoebaGrowing = state.runtime->amoeba.isGrowing;
        fingerprint.magicWallActive = state.runtime->magicWall.state == MagicWallState::Active;
        if (fingerprint.exitOpen && state.cave.has_value())
        {
            fingerprint.diamondValue =
                std::min(state.cave->configuration.extraDiamondValue, kC64HudDiamondDisplayMaximum);
        }
        fingerprint.collectedDiamonds =
            std::min<std::uint16_t>(state.runtime->collectedDiamonds, kC64HudDiamondDisplayMaximum);
        fingerprint.timeRemainingSeconds = state.runtime->timeRemainingSeconds;
    }

    return fingerprint;
}

std::uint64_t advanceC64PalCycleRemainder(std::uint64_t &remainder)
{
    remainder += kC64PalCyclesPerCalibrationWindow;
    const std::uint64_t elapsedCycles = remainder / kC64PalSubSecondTicksPerCalibrationWindow;
    remainder %= kC64PalSubSecondTicksPerCalibrationWindow;
    return elapsedCycles;
}

bool isBonusCave(const CellCode caveNumber)
{
    return caveNumber >= 0x11 && caveNumber <= 0x14;
}

std::uint8_t entryAppearanceCountdown(const CellCode caveNumber, const std::uint8_t playerCount)
{
    // InitGameVariablesFromLevelData ($7f67-$7f87) adds two seconds for two
    // players sharing one input and one more second for Cave B.
    std::uint8_t countdown =
        playerCount == 2 ? kC64TwoPlayerSingleInputEntryAppearanceSeconds : kC64SinglePlayerEntryAppearanceSeconds;
    if (caveNumber == kC64CaveBNumber)
    {
        ++countdown;
    }
    return countdown;
}

void configureNewCampaign(GameState &state, const std::uint8_t playerCount)
{
    if (playerCount == 0 || playerCount > state.campaign.players.size())
    {
        throw std::invalid_argument("The campaign supports one or two players.");
    }

    CampaignRuntimeState &campaign = state.campaign;
    campaign = {};
    campaign.playerCount = playerCount;
    for (PlayerProgress &player : campaign.players)
    {
        player.lives = 3;
        if (state.cave.has_value())
        {
            player.caveNumber = state.cave->caveNumber;
            player.sublevelIndex = state.cave->sublevelIndex;
        }
    }
    if (state.cave.has_value())
    {
        campaign.sublevelIndex = state.cave->sublevelIndex;
        campaign.pendingCaveNumber = state.cave->caveNumber;
        campaign.pendingCaveIsBonus = isBonusCave(campaign.pendingCaveNumber);
    }
    if (state.runtime.has_value())
    {
        state.runtime->score = 0;
        state.runtime->appearanceCountdown = entryAppearanceCountdown(state.cave->caveNumber, playerCount);
    }
}

void replacePreparedCave(GameState &state, const PreparedCave &preparedCave)
{
    CampaignRuntimeState campaign = state.campaign;
    if (campaign.gameOver)
    {
        throw std::logic_error("A game-over campaign cannot load another cave.");
    }
    if (campaign.pendingCaveNumber != 0 && preparedCave.metadata.caveNumber != campaign.pendingCaveNumber)
    {
        throw std::invalid_argument("The prepared cave does not match the pending C64 campaign cave.");
    }

    state = makeInitialGameState(preparedCave);
    state.campaign = campaign;
    state.campaign.pendingCaveNumber = preparedCave.metadata.caveNumber;
    state.campaign.pendingCaveIsBonus = isBonusCave(preparedCave.metadata.caveNumber);
    state.runtime->appearanceCountdown =
        entryAppearanceCountdown(preparedCave.metadata.caveNumber, campaign.playerCount);
    initializeCaveProgress(state);
}

} // namespace

GameSession::GameSession(GameState initialState) : state_(std::move(initialState))
{
    if (state_.cave.has_value() && state_.runtime.has_value() &&
        (state_.phase == SessionPhase::RockfordAppearing || state_.phase == SessionPhase::Playing ||
         state_.phase == SessionPhase::RockfordDead))
    {
        initializeCaveProgress(state_);
    }
}

GameSession makeNewCampaignSession(const std::uint8_t playerCount)
{
    GameState initialState = makeInitialGameState(prepareCave(caveA(), 0));
    configureNewCampaign(initialState, playerCount);
    initializeCaveProgress(initialState);
    // SetLevel ($8722-$8745) finishes revealing the prepared cave before the
    // first ProcessCave call. IRQ-driven appearance seconds keep advancing.
    initialState.phase = SessionPhase::CaveTransitioning;
    return GameSession(std::move(initialState));
}

GameSnapshot GameSession::snapshot() const
{
    return GameSnapshot{state_.grid, state_.tick,    state_.phase,   state_.lastCommand,
                        state_.cave, state_.runtime, state_.campaign};
}

SessionPhase GameSession::phase() const noexcept
{
    return state_.phase;
}

std::vector<GameEvent> GameSession::tick(const GameCommand command)
{
    if (state_.phase == SessionPhase::CaveCompleted || state_.phase == SessionPhase::CaveTransitioning)
    {
        return {};
    }

    state_.lastCommand = command;
    ++state_.tick;
    std::vector<GameEvent> events;

    if (state_.phase == SessionPhase::RockfordDead && state_.runtime.has_value())
    {
        prepareAmoebaTick(state_);
        CaveUpdateContext context{state_, {Direction::Neutral}, &events};
        const CaveScanKind scanKind =
            state_.cave.has_value() && state_.cave->caveNumber >= 0x11 ? CaveScanKind::Bonus : CaveScanKind::Normal;
        static_cast<void>(CaveScanner{}.scan(context, scanKind));
        advanceRockfordDeath(state_);
    }
    else if ((state_.phase == SessionPhase::RockfordAppearing || state_.phase == SessionPhase::Playing) &&
             state_.runtime.has_value())
    {
        prepareAmoebaTick(state_);
        CaveUpdateContext context{state_, command, &events};
        const CaveScanKind scanKind =
            state_.cave.has_value() && state_.cave->caveNumber >= 0x11 ? CaveScanKind::Bonus : CaveScanKind::Normal;
        static_cast<void>(CaveScanner{}.scan(context, scanKind));
    }

    return events;
}

FrameAdvanceResult GameSession::advanceFrame(const GameCommand command)
{
    if (state_.phase == SessionPhase::GameOver)
    {
        return {};
    }

    const PresentationFingerprint previousPresentation = presentationFingerprint(state_);
    if (state_.phase == SessionPhase::Transitioning)
    {
        loadPendingCave();
        return {{}, true, !(presentationFingerprint(state_) == previousPresentation)};
    }

    std::vector<GameEvent> events;
    if (state_.phase == SessionPhase::CaveTransitioning)
    {
        ++framesSinceCaveSecond_;
        if (framesSinceCaveSecond_ == kC64SubSecondTicksPerGameSecond)
        {
            framesSinceCaveSecond_ = 0;
            advanceCaveSecond(events);
        }

        const std::uint64_t elapsedCycles = advanceC64PalCycleRemainder(caveTransitionCycleRemainder_);
        caveTransitionCycleCredits_ += elapsedCycles;
        if (caveTransitionCycleCredits_ >= kC64PalCaveTransitionCycles)
        {
            caveTransitionCycleCredits_ -= kC64PalCaveTransitionCycles;
            state_.phase = SessionPhase::RockfordAppearing;
        }
        caveTickCredits_ = 0;
        return {std::move(events), false, !(presentationFingerprint(state_) == previousPresentation)};
    }

    if (state_.phase == SessionPhase::CaveCompleted)
    {
        const std::uint64_t elapsedCycles = advanceC64PalCycleRemainder(caveCompletionCycleRemainder_);
        advanceCaveCompletionCycles(elapsedCycles);
        framesSinceCaveSecond_ = 0;
        caveTickCredits_ = 0;
        return {std::move(events), false, !(presentationFingerprint(state_) == previousPresentation)};
    }

    advanceLogicalFrame();
    ++framesSinceCaveSecond_;
    if (framesSinceCaveSecond_ == kC64SubSecondTicksPerGameSecond)
    {
        framesSinceCaveSecond_ = 0;
        advanceCaveSecond(events);
    }

    const std::uint8_t sublevelIndex = state_.cave.has_value() ? state_.cave->sublevelIndex : 0;
    if (sublevelIndex >= kC64PalCaveTicksPer600SubSecondTicks.size())
    {
        throw std::logic_error("The cave sublevel index is outside the C64 range.");
    }

    caveTickCredits_ += kC64PalCaveTicksPer600SubSecondTicks[sublevelIndex];
    if (caveTickCredits_ >= kC64PalSubSecondTicksPerCalibrationWindow)
    {
        caveTickCredits_ -= kC64PalSubSecondTicksPerCalibrationWindow;
        events = tick(command);
        if (state_.phase == SessionPhase::CaveCompleted)
        {
            framesSinceCaveSecond_ = 0;
            caveTickCredits_ = 0;
            caveCompletionCycleRemainder_ = 0;
        }
    }

    return {std::move(events), false, !(presentationFingerprint(state_) == previousPresentation)};
}

void GameSession::loadPendingCave()
{
    if (state_.phase != SessionPhase::Transitioning)
    {
        throw std::logic_error("Only a transitioning campaign can load its pending cave.");
    }

    const CellCode caveNumber = state_.campaign.pendingCaveNumber;
    if (caveNumber == 0 || caveNumber > 20)
    {
        throw std::logic_error("The pending C64 campaign cave number is outside the supported range.");
    }

    const PreparedCave preparedCave = prepareCave(caveDefinition(caveNumber), state_.campaign.sublevelIndex);
    replacePreparedCave(state_, preparedCave);
    // The C64 has the complete next cave in memory before its screen reveal.
    // Keep gameplay suspended while the presentation exposes that grid.
    state_.phase = SessionPhase::CaveTransitioning;
    framesSinceCaveSecond_ = 0;
    caveTickCredits_ = 0;
    caveCompletionCycleRemainder_ = 0;
    caveTransitionCycleRemainder_ = 0;
    caveTransitionCycleCredits_ = 0;
}

void GameSession::advanceCaveSecond(std::vector<GameEvent> &events)
{
    if (state_.runtime.has_value() && state_.runtime->appearanceCountdown != 0)
    {
        --state_.runtime->appearanceCountdown;
        if (state_.runtime->appearanceCountdown == 0)
        {
            events.push_back({GameEventType::RockfordAppearanceStarted, {}});
        }
    }
    // Once FlashingEntryBoxFlag is clear, SubSecondTick ($7111-$712c)
    // keeps advancing game seconds while Rockford is dead.
    if (state_.phase == SessionPhase::Playing || state_.phase == SessionPhase::RockfordDead)
    {
        const std::uint16_t previousTimeRemaining = state_.runtime->timeRemainingSeconds;
        advanceActiveCaveSecond(state_);
        const std::uint16_t timeRemaining = state_.runtime->timeRemainingSeconds;
        if (timeRemaining != previousTimeRemaining && timeRemaining >= 1 && timeRemaining <= 9)
        {
            events.push_back({GameEventType::TimeRunningOut, {}, static_cast<std::uint8_t>(timeRemaining)});
        }
    }
}

void GameSession::advanceLogicalFrame()
{
    if (state_.phase != SessionPhase::CaveCompleted && state_.phase != SessionPhase::CaveTransitioning)
    {
        advanceMagicWallFrame(state_);
    }
}

void GameSession::advanceCaveCompletionCycles(const std::uint64_t cycles)
{
    advanceCompletedCaveCycles(state_, cycles);
}

} // namespace boulderdash::engine

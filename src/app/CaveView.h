#pragma once

#include "engine/GameState.h"

#include <QColor>
#include <QWidget>

#include <cstdint>
#include <optional>
#include <vector>

namespace boulderdash::app
{

class CaveView final : public QWidget
{
  public:
    explicit CaveView(QWidget *parent = nullptr);

    void setSnapshot(engine::GameSnapshot snapshot);
    void setPresentationSubSecondFrame(std::uint64_t frame);
    void setTitlePresentationFrame(std::uint64_t frame);
    void setTitleVisible(bool visible);
    void startInitialCaveReveal();
    void setTitlePlayerCount(std::uint8_t playerCount);
    void setPaused(bool paused);
    [[nodiscard]] static std::uint8_t c64AnimationFlagsForCave(engine::CellCode caveNumber);
    [[nodiscard]] static QColor c64PaletteColour(engine::CellCode colourCode);
    [[nodiscard]] static QColor c64MulticolourCellColour(engine::CellCode colourRamValue);
    [[nodiscard]] static bool c64FlashingDoorShowsSteel(std::uint8_t flashingEntryBoxState);
    [[nodiscard]] static std::uint8_t c64AnimationFrameForPresentationSubSecondFrame(std::uint64_t frame);
    [[nodiscard]] static engine::CellCode c64TitleLogoCharacter(std::uint8_t column, std::uint8_t row);

  protected:
    void paintEvent(QPaintEvent *event) override;

  private:
    void beginCaveTransitionReveal(const engine::GameSnapshot &snapshot);
    [[nodiscard]] bool caveTransitionRevealIsActive() const noexcept;
    [[nodiscard]] bool caveTransitionCellIsRevealed(std::size_t x, std::size_t y,
                                                    const engine::CaveSize &size) const noexcept;

    std::optional<engine::GameSnapshot> snapshot_;
    std::uint8_t animationFrame_ = 0;
    std::uint64_t titlePresentationFrame_ = 0;
    std::uint64_t presentationSubSecondFrame_ = 0;
    std::uint64_t caveTransitionStartFrame_ = 0;
    std::vector<std::size_t> caveTransitionRevealEventByCell_;
    bool rockfordFacesLeft_ = false;
    bool titleVisible_ = false;
    bool paused_ = false;
    bool caveTransitionRevealActive_ = false;
    bool initialCaveRevealActive_ = false;
    std::uint8_t titlePlayerCount_ = 1;
};

} // namespace boulderdash::app

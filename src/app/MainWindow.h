#pragma once

#include "engine/GameState.h"

#include <QMainWindow>

class QFocusEvent;
class QKeyEvent;

namespace boulderdash::app
{

class CaveView;
class KeyboardInput;

class MainWindow final : public QMainWindow
{
  public:
    explicit MainWindow(KeyboardInput &keyboardInput, QWidget *parent = nullptr);

    void setSnapshot(engine::GameSnapshot snapshot);
    void setPresentationSubSecondFrame(std::uint64_t frame);
    void setTitlePresentationFrame(std::uint64_t frame);
    void setTitleVisible(bool visible);
    void startInitialCaveReveal();
    [[nodiscard]] bool consumeStartRequested() noexcept;
    [[nodiscard]] std::uint8_t selectedPlayerCount() const noexcept;
    [[nodiscard]] bool consumeRestartRequested() noexcept;
    [[nodiscard]] bool consumePauseRequested() noexcept;
    void setPaused(bool paused);

  protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

  private:
    KeyboardInput &keyboardInput_;
    CaveView *caveView_ = nullptr;
    bool restartRequested_ = false;
    bool pauseRequested_ = false;
    bool pauseAllowed_ = false;
    bool startRequested_ = false;
    bool titleVisible_ = false;
    std::uint8_t selectedPlayerCount_ = 1;
};

} // namespace boulderdash::app

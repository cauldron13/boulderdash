#include "app/MainWindow.h"

#include "app/CaveView.h"
#include "app/KeyboardInput.h"

#include <QFocusEvent>
#include <QKeyEvent>

#include <utility>

namespace boulderdash::app
{

MainWindow::MainWindow(KeyboardInput &keyboardInput, QWidget *parent)
    : QMainWindow(parent), keyboardInput_(keyboardInput), caveView_(new CaveView(this))
{
    setWindowTitle(tr("Boulder Dash"));
    // Preserve 24-pixel cave tiles while including the three-pixel C64 HUD-to-cave raster gap.
    resize(960, 555);
    setFocusPolicy(Qt::StrongFocus);
    setCentralWidget(caveView_);
}

void MainWindow::setSnapshot(engine::GameSnapshot snapshot)
{
    pauseAllowed_ = snapshot.phase == engine::SessionPhase::RockfordAppearing ||
                    snapshot.phase == engine::SessionPhase::Playing ||
                    snapshot.phase == engine::SessionPhase::RockfordDead;
    caveView_->setSnapshot(std::move(snapshot));
}

void MainWindow::setPresentationSubSecondFrame(const std::uint64_t frame)
{
    caveView_->setPresentationSubSecondFrame(frame);
}

void MainWindow::setTitlePresentationFrame(const std::uint64_t frame)
{
    caveView_->setTitlePresentationFrame(frame);
}

void MainWindow::setTitleVisible(const bool visible)
{
    titleVisible_ = visible;
    caveView_->setTitleVisible(visible);
    caveView_->setTitlePlayerCount(selectedPlayerCount_);
    setWindowTitle(visible ? tr("Boulder Dash - Press Enter to Play") : tr("Boulder Dash"));
}

void MainWindow::startInitialCaveReveal()
{
    caveView_->startInitialCaveReveal();
}

bool MainWindow::consumeStartRequested() noexcept
{
    const bool requested = startRequested_;
    startRequested_ = false;
    return requested;
}

std::uint8_t MainWindow::selectedPlayerCount() const noexcept
{
    return selectedPlayerCount_;
}

bool MainWindow::consumeRestartRequested() noexcept
{
    const bool requested = restartRequested_;
    restartRequested_ = false;
    return requested;
}

bool MainWindow::consumePauseRequested() noexcept
{
    const bool requested = pauseRequested_;
    pauseRequested_ = false;
    return requested;
}

void MainWindow::setPaused(const bool paused)
{
    caveView_->setPaused(paused);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (titleVisible_ && !event->isAutoRepeat() && (event->key() == Qt::Key_1 || event->key() == Qt::Key_2))
    {
        selectedPlayerCount_ = event->key() == Qt::Key_1 ? 1 : 2;
        caveView_->setTitlePlayerCount(selectedPlayerCount_);
        event->accept();
        return;
    }
    if (!event->isAutoRepeat() && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter))
    {
        if (titleVisible_)
        {
            startRequested_ = true;
        }
        else
        {
            restartRequested_ = true;
        }
        event->accept();
        return;
    }
    if (!titleVisible_ && pauseAllowed_ && !event->isAutoRepeat() && event->key() == Qt::Key_Space)
    {
        pauseRequested_ = true;
        event->accept();
        return;
    }
    if (keyboardInput_.pressKey(event->key()))
    {
        event->accept();
        return;
    }

    QMainWindow::keyPressEvent(event);
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    if (!event->isAutoRepeat() && keyboardInput_.releaseKey(event->key()))
    {
        event->accept();
        return;
    }

    QMainWindow::keyReleaseEvent(event);
}

void MainWindow::focusOutEvent(QFocusEvent *event)
{
    keyboardInput_.clear();
    QMainWindow::focusOutEvent(event);
}

} // namespace boulderdash::app

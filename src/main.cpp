#include "app/FixedStepRunner.h"
#include "app/KeyboardInput.h"
#include "app/MainWindow.h"
#include "audio/AudioController.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QTimer>

#include <chrono>
#include <cstdint>
#include <utility>

namespace
{

// Title IRQ cadence has not yet been included in the PAL gameplay calibration.
constexpr std::uint64_t kCurrentTitlePresentationFramesPerSecond = 60;

} // namespace

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Boulder Dash"));
    QApplication::setOrganizationName(QStringLiteral("Boulder Dash"));

    boulderdash::engine::GameSession session = boulderdash::engine::makeNewCampaignSession(1);
    boulderdash::app::FixedStepRunner fixedStepRunner(std::move(session));
    boulderdash::audio::AudioController audioController;
    boulderdash::app::KeyboardInput keyboardInput;
    boulderdash::app::MainWindow mainWindow(keyboardInput);
    bool titleActive = true;
    bool pausedByUser = false;
    std::uint64_t titlePresentationFrame = 0;
    std::uint64_t titlePresentationFrameRemainder = 0;
    bool initialCaveRevealRequested = false;
    mainWindow.setTitleVisible(true);
    fixedStepRunner.setPaused(true);
    audioController.startTheme();
    mainWindow.setSnapshot(fixedStepRunner.snapshot());
    mainWindow.setPresentationSubSecondFrame(fixedStepRunner.presentationSubSecondFrame());
    QElapsedTimer elapsedTimer;
    elapsedTimer.start();
    QTimer simulationTimer;
    simulationTimer.setTimerType(Qt::PreciseTimer);
    simulationTimer.setInterval(16);
    QObject::connect(
        &simulationTimer, &QTimer::timeout,
        [&fixedStepRunner, &audioController, &keyboardInput, &mainWindow, &elapsedTimer, &titleActive, &pausedByUser,
         &titlePresentationFrame, &titlePresentationFrameRemainder, &initialCaveRevealRequested]() {
            if (mainWindow.consumePauseRequested())
            {
                pausedByUser = !pausedByUser;
                keyboardInput.clear();
                fixedStepRunner.setPaused(pausedByUser);
                mainWindow.setPaused(pausedByUser);
                if (pausedByUser)
                {
                    audioController.stopAll();
                }
                else
                {
                    audioController.synchronizeState(fixedStepRunner.snapshot());
                }
                elapsedTimer.restart();
                return;
            }
            const auto elapsed = std::chrono::nanoseconds(elapsedTimer.nsecsElapsed());
            elapsedTimer.restart();
            if (titleActive)
            {
                titlePresentationFrameRemainder +=
                    static_cast<std::uint64_t>(elapsed.count()) * kCurrentTitlePresentationFramesPerSecond;
                titlePresentationFrame += titlePresentationFrameRemainder / 1000000000ULL;
                titlePresentationFrameRemainder %= 1000000000ULL;
                mainWindow.setTitlePresentationFrame(titlePresentationFrame);
            }
            const boulderdash::app::FixedStepAdvanceResult result =
                fixedStepRunner.advance(elapsed, keyboardInput.command());
            if (result.simulatedFrameCount != 0 && !titleActive)
            {
                audioController.playEvents(result.events);
            }
            if (result.caveLoaded && !titleActive)
            {
                audioController.playCaveTransition();
            }
            bool publishSnapshot = result.presentationChanged;
            if (mainWindow.consumeStartRequested())
            {
                keyboardInput.clear();
                audioController.stopAll();
                fixedStepRunner.reset(boulderdash::engine::makeNewCampaignSession(mainWindow.selectedPlayerCount()));
                fixedStepRunner.setPaused(false);
                mainWindow.setTitleVisible(false);
                mainWindow.setPaused(false);
                titleActive = false;
                pausedByUser = false;
                audioController.playCaveTransition();
                initialCaveRevealRequested = true;
                publishSnapshot = true;
            }
            const bool restartRequested = mainWindow.consumeRestartRequested();
            if (restartRequested && fixedStepRunner.phase() == boulderdash::engine::SessionPhase::GameOver)
            {
                keyboardInput.clear();
                audioController.stopAll();
                fixedStepRunner.reset(boulderdash::engine::makeNewCampaignSession(mainWindow.selectedPlayerCount()));
                audioController.playCaveTransition();
                initialCaveRevealRequested = true;
                publishSnapshot = true;
            }
            mainWindow.setPresentationSubSecondFrame(fixedStepRunner.presentationSubSecondFrame());
            if (!publishSnapshot)
            {
                return;
            }

            boulderdash::engine::GameSnapshot snapshot = fixedStepRunner.snapshot();
            if (!titleActive)
            {
                audioController.synchronizeState(snapshot);
            }
            mainWindow.setSnapshot(std::move(snapshot));
            if (initialCaveRevealRequested)
            {
                mainWindow.startInitialCaveReveal();
                initialCaveRevealRequested = false;
            }
        });
    QObject::connect(&application, &QApplication::applicationStateChanged,
                     [&fixedStepRunner, &audioController, &keyboardInput, &elapsedTimer, &titleActive,
                      &pausedByUser](const Qt::ApplicationState state) {
                         const bool applicationActive = state == Qt::ApplicationActive;
                         const bool paused = titleActive || pausedByUser || !applicationActive;
                         fixedStepRunner.setPaused(paused);
                         if (!applicationActive)
                         {
                             audioController.stopAll();
                         }
                         else if (titleActive)
                         {
                             audioController.startTheme();
                         }
                         else if (!pausedByUser)
                         {
                             audioController.synchronizeState(fixedStepRunner.snapshot());
                         }
                         if (!applicationActive)
                         {
                             keyboardInput.clear();
                         }
                         elapsedTimer.restart();
                     });
    QObject::connect(&application, &QApplication::aboutToQuit, [&audioController]() { audioController.stopAll(); });
    QObject::connect(&application, &QApplication::focusChanged,
                     [&application, &fixedStepRunner, &audioController, &keyboardInput, &mainWindow, &elapsedTimer,
                      &titleActive, &pausedByUser](QWidget *, QWidget *current) {
                         if (application.applicationState() != Qt::ApplicationActive)
                         {
                             return;
                         }
                         const bool hasFocus =
                             current != nullptr && (current == &mainWindow || mainWindow.isAncestorOf(current));
                         const bool paused = titleActive || pausedByUser || !hasFocus;
                         fixedStepRunner.setPaused(paused);
                         if (!hasFocus)
                         {
                             audioController.stopAll();
                         }
                         else if (titleActive)
                         {
                             audioController.startTheme();
                         }
                         else if (!pausedByUser)
                         {
                             audioController.synchronizeState(fixedStepRunner.snapshot());
                         }
                         if (!hasFocus)
                         {
                             keyboardInput.clear();
                         }
                         elapsedTimer.restart();
                     });
    simulationTimer.start();

    mainWindow.show();
    mainWindow.setFocus();

    return application.exec();
}

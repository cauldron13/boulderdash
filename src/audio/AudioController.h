#pragma once

#include "engine/EngineTypes.h"
#include "engine/GameState.h"

#include <QAudioOutput>
#include <QMediaPlayer>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace boulderdash::audio
{

class AudioController final
{
  public:
    AudioController();
    ~AudioController();

    void playEvents(const std::vector<engine::GameEvent> &events);
    void synchronizeState(const engine::GameSnapshot &snapshot);
    void playCaveTransition();
    void startTheme();
    void stopAll();

  private:
    struct EffectMixer;

    enum class LogicalVoice : std::uint8_t
    {
        Voice1,
        Voice2,
        Voice3,
    };

    [[nodiscard]] static std::optional<std::size_t> effectIndex(const engine::GameEvent &event) noexcept;
    [[nodiscard]] static LogicalVoice logicalVoice(engine::GameEventType type) noexcept;
    [[nodiscard]] static std::size_t logicalVoiceIndex(LogicalVoice voice) noexcept;
    void playEffect(std::size_t effectIndex, LogicalVoice voice, bool loop = false);
    void stopVoice(LogicalVoice voice);
    void synchronizeVoice3Background();

    std::unique_ptr<EffectMixer> effectMixer_;
    bool amoebaRequested_ = false;
    bool magicWallRequested_ = false;
    QAudioOutput themeOutput_;
    QMediaPlayer theme_;
};

} // namespace boulderdash::audio

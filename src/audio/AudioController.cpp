#include "audio/AudioController.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QByteArray>
#include <QDebug>
#include <QFile>
#include <QIODevice>
#include <QMediaDevices>
#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include <utility>
#include <vector>

namespace boulderdash::audio
{
namespace
{

constexpr float kThemeVolume = 0.28F;
constexpr float kEffectVolume = 0.20F;
constexpr std::size_t kLogicalVoiceCount = 3;
constexpr std::size_t kEffectCount = 17;
constexpr std::size_t kDiamondFallingVariantCount = 8;
constexpr std::size_t kDirtMovementEffectIndex = 0;
constexpr std::size_t kEmptyMovementEffectIndex = 1;
constexpr std::size_t kDiamondCollectedEffectIndex = 2;
constexpr std::size_t kCrackEffectIndex = 3;
constexpr std::size_t kBoulderPushedEffectIndex = 4;
constexpr std::size_t kBoulderImpactEffectIndex = 5;
constexpr std::size_t kExplosionEffectIndex = 6;
constexpr std::size_t kFinishedEffectIndex = 7;
constexpr std::size_t kFirstTimeoutEffectIndex = 8;
constexpr std::size_t kFirstDiamondFallingEffectIndex = kEffectCount;
constexpr std::size_t kCoverEffectIndex = kFirstDiamondFallingEffectIndex + kDiamondFallingVariantCount;
constexpr std::size_t kAmoebaEffectIndex = kCoverEffectIndex + 1;
constexpr std::size_t kMagicWallEffectIndex = kAmoebaEffectIndex + 1;
constexpr std::size_t kMixerEffectCount = kMagicWallEffectIndex + 1;
constexpr qint64 kEffectOutputBufferMicroseconds = 12000;
constexpr int kEffectPumpIntervalMilliseconds = 2;

constexpr std::array<const char *, kMixerEffectCount> kEffectResources = {
    ":/c64/audio/walk_d.wav",     ":/c64/audio/walk_e.wav",    ":/c64/audio/diamond_collect.wav",
    ":/c64/audio/crack.wav",      ":/c64/audio/stone.wav",     ":/c64/audio/stone_2.wav",
    ":/c64/audio/exploded.wav",   ":/c64/audio/finished.wav",  ":/c64/audio/timeout_1.wav",
    ":/c64/audio/timeout_2.wav",  ":/c64/audio/timeout_3.wav", ":/c64/audio/timeout_4.wav",
    ":/c64/audio/timeout_5.wav",  ":/c64/audio/timeout_6.wav", ":/c64/audio/timeout_7.wav",
    ":/c64/audio/timeout_8.wav",  ":/c64/audio/timeout_9.wav", ":/c64/audio/diamond_1.wav",
    ":/c64/audio/diamond_2.wav",  ":/c64/audio/diamond_3.wav", ":/c64/audio/diamond_4.wav",
    ":/c64/audio/diamond_5.wav",  ":/c64/audio/diamond_6.wav", ":/c64/audio/diamond_7.wav",
    ":/c64/audio/diamond_8.wav",  ":/c64/audio/cover.wav",     ":/c64/audio/amoeba.wav",
    ":/c64/audio/magic_wall.wav",
};

[[nodiscard]] std::uint16_t readLittleEndian16(const char *data) noexcept
{
    const auto *bytes = reinterpret_cast<const unsigned char *>(data);
    return static_cast<std::uint16_t>(bytes[0]) | (static_cast<std::uint16_t>(bytes[1]) << 8U);
}

[[nodiscard]] std::uint32_t readLittleEndian32(const char *data) noexcept
{
    const auto *bytes = reinterpret_cast<const unsigned char *>(data);
    return static_cast<std::uint32_t>(bytes[0]) | (static_cast<std::uint32_t>(bytes[1]) << 8U) |
           (static_cast<std::uint32_t>(bytes[2]) << 16U) | (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

[[nodiscard]] float normalizedPcm16Sample(const char *data) noexcept
{
    const std::uint16_t raw = readLittleEndian16(data);
    const int sample = raw >= 0x8000U ? static_cast<int>(raw) - 0x10000 : static_cast<int>(raw);
    return static_cast<float>(sample) / 32768.0F;
}

void writeOutputSample(char *destination, const QAudioFormat::SampleFormat format, const float value) noexcept
{
    const float clipped = std::clamp(value, -1.0F, 1.0F);
    switch (format)
    {
    case QAudioFormat::UInt8: {
        const auto sample = static_cast<std::uint8_t>(std::lround((clipped + 1.0F) * 127.5F));
        std::memcpy(destination, &sample, sizeof(sample));
        return;
    }
    case QAudioFormat::Int16: {
        const auto sample = static_cast<std::int16_t>(std::lround(clipped * 32767.0F));
        std::memcpy(destination, &sample, sizeof(sample));
        return;
    }
    case QAudioFormat::Int32: {
        const auto sample = static_cast<std::int32_t>(std::llround(static_cast<double>(clipped) * 2147483647.0));
        std::memcpy(destination, &sample, sizeof(sample));
        return;
    }
    case QAudioFormat::Float:
        std::memcpy(destination, &clipped, sizeof(clipped));
        return;
    case QAudioFormat::Unknown:
    case QAudioFormat::NSampleFormats:
        return;
    }
}

} // namespace

struct AudioController::EffectMixer final
{
    struct DecodedEffect final
    {
        std::vector<float> stereoSamples;
    };

    struct Voice final
    {
        std::optional<std::size_t> effectIndex;
        std::size_t samplePosition = 0;
        bool loop = false;
    };

    EffectMixer() : outputDevice_(QMediaDevices::defaultAudioOutput())
    {
        if (outputDevice_.isNull())
        {
            qWarning() << "No audio output is available; sound effects are disabled.";
            return;
        }

        outputFormat_.setSampleRate(44100);
        outputFormat_.setChannelCount(2);
        outputFormat_.setSampleFormat(QAudioFormat::Int16);
        if (!outputDevice_.isFormatSupported(outputFormat_))
        {
            outputFormat_ = outputDevice_.preferredFormat();
        }
        if (!outputFormat_.isValid() || outputFormat_.bytesPerFrame() <= 0)
        {
            qWarning() << "The default audio output has no usable PCM format; sound effects are disabled.";
            return;
        }

        for (std::size_t index = 0; index < effects_.size(); ++index)
        {
            effects_[index] = decodeWaveResource(QString::fromLatin1(kEffectResources[index]));
        }

        audioSink_ = std::make_unique<QAudioSink>(outputDevice_, outputFormat_);
        const qsizetype requestedBufferSize = std::max<qsizetype>(
            outputFormat_.bytesForDuration(kEffectOutputBufferMicroseconds), outputFormat_.bytesPerFrame());
        targetQueuedByteCount_ = requestedBufferSize;
        audioSink_->setBufferSize(requestedBufferSize);
        audioPumpTimer_.setTimerType(Qt::PreciseTimer);
        audioPumpTimer_.setInterval(kEffectPumpIntervalMilliseconds);
        QObject::connect(&audioPumpTimer_, &QTimer::timeout, [this]() { pumpAudio(); });
        QObject::connect(audioSink_.get(), &QAudioSink::stateChanged, [this](const QAudio::State state) {
            if (state == QAudio::StoppedState && audioSink_->error() != QAudio::NoError)
            {
                qWarning() << "Sound-effect output stopped with audio error" << audioSink_->error();
            }
        });
        sinkDevice_ = audioSink_->start();
        if (!sinkDevice_)
        {
            qWarning() << "Unable to open the sound-effect output; sound effects are disabled.";
            audioSink_.reset();
            return;
        }
        pumpAudio();
        audioPumpTimer_.start();
    }

    ~EffectMixer()
    {
        audioPumpTimer_.stop();
        sinkDevice_ = nullptr;
        if (audioSink_)
        {
            audioSink_->stop();
        }
    }

    void play(const std::size_t effectIndex, const std::size_t voiceIndex, const bool loop)
    {
        Q_ASSERT(effectIndex < effects_.size());
        Q_ASSERT(voiceIndex < voices_.size());
        if (!audioSink_ || effects_[effectIndex].stereoSamples.empty())
        {
            return;
        }

        {
            const std::lock_guard<std::mutex> lock(mutex_);
            voices_[voiceIndex] = {effectIndex, 0, loop};
        }
        pumpAudio();
    }

    void stop(const std::size_t voiceIndex)
    {
        Q_ASSERT(voiceIndex < voices_.size());
        const std::lock_guard<std::mutex> lock(mutex_);
        voices_[voiceIndex] = {};
    }

    void stopAll()
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        voices_.fill({});
    }

    [[nodiscard]] std::optional<std::size_t> activeEffect(const std::size_t voiceIndex) const
    {
        Q_ASSERT(voiceIndex < voices_.size());
        const std::lock_guard<std::mutex> lock(mutex_);
        return voices_[voiceIndex].effectIndex;
    }

  private:
    void pumpAudio()
    {
        if (!audioSink_ || !sinkDevice_)
        {
            return;
        }
        {
            const std::lock_guard<std::mutex> lock(mutex_);
            const bool hasActiveVoice = std::any_of(voices_.cbegin(), voices_.cend(),
                                                    [](const Voice &voice) { return voice.effectIndex.has_value(); });
            if (!hasActiveVoice)
            {
                return;
            }
        }

        const qint64 bytesPerFrame = outputFormat_.bytesPerFrame();
        const qint64 availableBytes = audioSink_->bytesFree();
        const qint64 bufferedByteCount =
            std::max<qint64>(0, static_cast<qint64>(audioSink_->bufferSize()) - availableBytes);
        const qint64 queueBudget = std::max<qint64>(0, targetQueuedByteCount_ - bufferedByteCount);
        const qint64 writableByteCount = std::min(availableBytes, queueBudget);
        const qint64 renderByteCount = writableByteCount - writableByteCount % bytesPerFrame;
        if (renderByteCount <= 0)
        {
            return;
        }

        outputBuffer_.resize(renderByteCount);
        const qint64 renderedByteCount = render(outputBuffer_.data(), outputBuffer_.size());
        const qint64 writtenByteCount = sinkDevice_->write(outputBuffer_.constData(), renderedByteCount);
        if (writtenByteCount < 0)
        {
            qWarning() << "Unable to write sound effects to the audio output:" << sinkDevice_->errorString();
        }
        else if (writtenByteCount != renderedByteCount)
        {
            qWarning() << "The sound-effect output accepted only" << writtenByteCount << "of" << renderedByteCount
                       << "bytes.";
        }
    }

    [[nodiscard]] DecodedEffect decodeWaveResource(const QString &resourcePath) const
    {
        QFile file(resourcePath);
        if (!file.open(QIODevice::ReadOnly))
        {
            qWarning().noquote() << "Unable to open audio effect" << resourcePath;
            return {};
        }
        const QByteArray bytes = file.readAll();
        const qsizetype byteCount = bytes.size();
        if (byteCount < 12 || std::memcmp(bytes.constData(), "RIFF", 4) != 0 ||
            std::memcmp(bytes.constData() + 8, "WAVE", 4) != 0)
        {
            qWarning().noquote() << "Audio effect is not a RIFF/WAVE file" << resourcePath;
            return {};
        }

        std::uint16_t encoding = 0;
        std::uint16_t channelCount = 0;
        std::uint16_t bitsPerSample = 0;
        std::uint32_t sampleRate = 0;
        qsizetype pcmOffset = -1;
        std::uint32_t pcmByteCount = 0;
        qsizetype chunkOffset = 12;
        while (chunkOffset <= byteCount - 8)
        {
            const char *const chunk = bytes.constData() + chunkOffset;
            const std::uint32_t chunkSize = readLittleEndian32(chunk + 4);
            const qsizetype payloadOffset = chunkOffset + 8;
            if (static_cast<std::uint64_t>(chunkSize) > static_cast<std::uint64_t>(byteCount - payloadOffset))
            {
                qWarning().noquote() << "Audio effect contains a truncated WAVE chunk" << resourcePath;
                return {};
            }

            if (std::memcmp(chunk, "fmt ", 4) == 0 && chunkSize >= 16)
            {
                const char *const format = bytes.constData() + payloadOffset;
                encoding = readLittleEndian16(format);
                channelCount = readLittleEndian16(format + 2);
                sampleRate = readLittleEndian32(format + 4);
                bitsPerSample = readLittleEndian16(format + 14);
            }
            else if (std::memcmp(chunk, "data", 4) == 0)
            {
                pcmOffset = payloadOffset;
                pcmByteCount = chunkSize;
            }

            chunkOffset = payloadOffset + static_cast<qsizetype>(chunkSize) + (chunkSize & 1U);
        }

        if (encoding != 1 || (channelCount != 1 && channelCount != 2) || bitsPerSample != 16 || sampleRate == 0 ||
            pcmOffset < 0)
        {
            qWarning().noquote() << "Audio effect uses an unsupported WAVE format" << resourcePath;
            return {};
        }

        const std::size_t inputFrameSize = static_cast<std::size_t>(channelCount) * sizeof(std::int16_t);
        const std::size_t inputFrameCount = pcmByteCount / inputFrameSize;
        if (inputFrameCount == 0)
        {
            qWarning().noquote() << "Audio effect contains no PCM frames" << resourcePath;
            return {};
        }

        const std::uint64_t outputFrameCount64 =
            (static_cast<std::uint64_t>(inputFrameCount) * static_cast<std::uint64_t>(outputFormat_.sampleRate()) +
             sampleRate - 1U) /
            sampleRate;
        if (outputFrameCount64 > std::numeric_limits<std::size_t>::max() / 2U)
        {
            qWarning().noquote() << "Audio effect is too large to decode" << resourcePath;
            return {};
        }

        DecodedEffect effect;
        const std::size_t outputFrameCount = static_cast<std::size_t>(outputFrameCount64);
        effect.stereoSamples.resize(outputFrameCount * 2U);
        const char *const pcm = bytes.constData() + pcmOffset;
        const auto inputSample = [pcm, channelCount](const std::size_t frame, const std::size_t channel) {
            const std::size_t sourceChannel = channelCount == 1 ? 0 : channel;
            const std::size_t byteOffset = (frame * channelCount + sourceChannel) * sizeof(std::int16_t);
            return normalizedPcm16Sample(pcm + byteOffset);
        };
        for (std::size_t outputFrame = 0; outputFrame < outputFrameCount; ++outputFrame)
        {
            const double inputPosition = static_cast<double>(outputFrame) * sampleRate / outputFormat_.sampleRate();
            const std::size_t firstInputFrame = std::min(static_cast<std::size_t>(inputPosition), inputFrameCount - 1U);
            const std::size_t secondInputFrame = std::min(firstInputFrame + 1U, inputFrameCount - 1U);
            const float fraction = static_cast<float>(inputPosition - static_cast<double>(firstInputFrame));
            for (std::size_t channel = 0; channel < 2; ++channel)
            {
                const float first = inputSample(firstInputFrame, channel);
                const float second = inputSample(secondInputFrame, channel);
                effect.stereoSamples[outputFrame * 2U + channel] = first + (second - first) * fraction;
            }
        }
        return effect;
    }

    qint64 render(char *data, const qint64 maximumSize)
    {
        if (maximumSize <= 0 || outputFormat_.bytesPerFrame() <= 0)
        {
            return 0;
        }

        const qint64 bytesPerFrame = outputFormat_.bytesPerFrame();
        const qint64 frameCount = maximumSize / bytesPerFrame;
        const qint64 renderedByteCount = frameCount * bytesPerFrame;
        const int bytesPerSample = outputFormat_.bytesPerSample();
        const int outputChannelCount = outputFormat_.channelCount();
        const std::lock_guard<std::mutex> lock(mutex_);
        for (qint64 frame = 0; frame < frameCount; ++frame)
        {
            float left = 0.0F;
            float right = 0.0F;
            for (Voice &voice : voices_)
            {
                if (!voice.effectIndex)
                {
                    continue;
                }
                const DecodedEffect &effect = effects_[*voice.effectIndex];
                if (voice.samplePosition >= effect.stereoSamples.size())
                {
                    if (voice.loop && !effect.stereoSamples.empty())
                    {
                        voice.samplePosition = 0;
                    }
                    else
                    {
                        voice = {};
                        continue;
                    }
                }
                left += effect.stereoSamples[voice.samplePosition];
                right += effect.stereoSamples[voice.samplePosition + 1U];
                voice.samplePosition += 2U;
            }

            left *= kEffectVolume;
            right *= kEffectVolume;
            char *const outputFrame = data + frame * bytesPerFrame;
            for (int channel = 0; channel < outputChannelCount; ++channel)
            {
                const float sample = outputChannelCount == 1 ? (left + right) * 0.5F
                                                             : (channel == 0 ? left : (channel == 1 ? right : 0.0F));
                writeOutputSample(outputFrame + channel * bytesPerSample, outputFormat_.sampleFormat(), sample);
            }
        }
        return renderedByteCount;
    }

    QAudioDevice outputDevice_;
    QAudioFormat outputFormat_;
    std::array<DecodedEffect, kMixerEffectCount> effects_;
    std::array<Voice, kLogicalVoiceCount> voices_;
    mutable std::mutex mutex_;
    QTimer audioPumpTimer_;
    QIODevice *sinkDevice_ = nullptr;
    QByteArray outputBuffer_;
    qint64 targetQueuedByteCount_ = 0;
    std::unique_ptr<QAudioSink> audioSink_;
};

AudioController::AudioController() : effectMixer_(std::make_unique<EffectMixer>())
{
    theme_.setAudioOutput(&themeOutput_);
    themeOutput_.setVolume(kThemeVolume);
    theme_.setSource(QUrl(QStringLiteral("qrc:/c64/audio/theme.mp3")));
    theme_.setLoops(QMediaPlayer::Infinite);
}

AudioController::~AudioController() = default;

void AudioController::playEvents(const std::vector<engine::GameEvent> &events)
{
    for (const engine::GameEvent &event : events)
    {
        if (event.type == engine::GameEventType::DiamondFalling)
        {
            Q_ASSERT(event.value >= 1 && event.value <= kDiamondFallingVariantCount);
            playEffect(kFirstDiamondFallingEffectIndex + event.value - 1U, LogicalVoice::Voice1);
            continue;
        }
        const std::optional<std::size_t> index = effectIndex(event);
        if (!index)
        {
            continue;
        }
        playEffect(*index, logicalVoice(event.type));
    }
}

void AudioController::synchronizeState(const engine::GameSnapshot &snapshot)
{
    const std::size_t voice3Index = logicalVoiceIndex(LogicalVoice::Voice3);
    const std::optional<std::size_t> activeVoice3Effect = effectMixer_->activeEffect(voice3Index);
    if (snapshot.phase != engine::SessionPhase::CaveCompleted && activeVoice3Effect == kFinishedEffectIndex)
    {
        stopVoice(LogicalVoice::Voice3);
    }

    const bool hasActiveCave =
        snapshot.runtime.has_value() &&
        (snapshot.phase == engine::SessionPhase::RockfordAppearing || snapshot.phase == engine::SessionPhase::Playing ||
         snapshot.phase == engine::SessionPhase::RockfordDead);
    amoebaRequested_ =
        hasActiveCave && snapshot.runtime->appearanceCountdown == 0 && snapshot.runtime->amoeba.isGrowing;
    magicWallRequested_ = hasActiveCave && snapshot.runtime->magicWall.state == engine::MagicWallState::Active;
    synchronizeVoice3Background();
}

void AudioController::playEffect(const std::size_t effectIndex, const LogicalVoice voice, const bool loop)
{
    effectMixer_->play(effectIndex, logicalVoiceIndex(voice), loop);
}

void AudioController::stopVoice(const LogicalVoice voice)
{
    effectMixer_->stop(logicalVoiceIndex(voice));
}

void AudioController::synchronizeVoice3Background()
{
    const std::size_t voiceIndex = logicalVoiceIndex(LogicalVoice::Voice3);
    const std::optional<std::size_t> currentEffect = effectMixer_->activeEffect(voiceIndex);
    const bool oneShotIsPlaying =
        currentEffect && *currentEffect != kAmoebaEffectIndex && *currentEffect != kMagicWallEffectIndex;
    if (oneShotIsPlaying)
    {
        return;
    }

    const std::optional<std::size_t> requestedEffect =
        amoebaRequested_ ? std::optional<std::size_t>(kAmoebaEffectIndex)
                         : (magicWallRequested_ ? std::optional<std::size_t>(kMagicWallEffectIndex) : std::nullopt);
    if (currentEffect == requestedEffect)
    {
        return;
    }
    if (!requestedEffect)
    {
        stopVoice(LogicalVoice::Voice3);
        return;
    }

    playEffect(*requestedEffect, LogicalVoice::Voice3, true);
}

void AudioController::startTheme()
{
    theme_.play();
}

void AudioController::playCaveTransition()
{
    playEffect(kCoverEffectIndex, LogicalVoice::Voice2);
}

void AudioController::stopAll()
{
    amoebaRequested_ = false;
    magicWallRequested_ = false;
    theme_.stop();
    effectMixer_->stopAll();
}

std::optional<std::size_t> AudioController::effectIndex(const engine::GameEvent &event) noexcept
{
    switch (event.type)
    {
    case engine::GameEventType::DugDirt:
        return kDirtMovementEffectIndex;
    case engine::GameEventType::RockfordMovedThroughEmptySpace:
        return kEmptyMovementEffectIndex;
    case engine::GameEventType::DiamondCollected:
        return kDiamondCollectedEffectIndex;
    case engine::GameEventType::DiamondQuotaReached:
    case engine::GameEventType::RockfordAppearanceStarted:
        return kCrackEffectIndex;
    case engine::GameEventType::BoulderPushed:
        return kBoulderPushedEffectIndex;
    case engine::GameEventType::BoulderImpact:
        return kBoulderImpactEffectIndex;
    case engine::GameEventType::DiamondFalling:
        return std::nullopt;
    case engine::GameEventType::Explosion:
        return kExplosionEffectIndex;
    case engine::GameEventType::ExitEntered:
        return kFinishedEffectIndex;
    case engine::GameEventType::TimeRunningOut:
        Q_ASSERT(event.value >= 1 && event.value <= 9);
        // The JS asset sequence is chronological: timeout_1 is 9 seconds left and timeout_9 is 1 second left.
        return kFirstTimeoutEffectIndex + 9 - event.value;
    case engine::GameEventType::RockfordDied:
    case engine::GameEventType::MagicWallActivated:
        return std::nullopt;
    }
    return std::nullopt;
}

AudioController::LogicalVoice AudioController::logicalVoice(const engine::GameEventType type) noexcept
{
    switch (type)
    {
    case engine::GameEventType::DiamondCollected:
    case engine::GameEventType::BoulderPushed:
    case engine::GameEventType::BoulderImpact:
    case engine::GameEventType::DiamondFalling:
        return LogicalVoice::Voice1;
    case engine::GameEventType::DugDirt:
    case engine::GameEventType::RockfordMovedThroughEmptySpace:
    case engine::GameEventType::Explosion:
    case engine::GameEventType::TimeRunningOut:
        return LogicalVoice::Voice2;
    case engine::GameEventType::DiamondQuotaReached:
    case engine::GameEventType::RockfordAppearanceStarted:
    case engine::GameEventType::ExitEntered:
        return LogicalVoice::Voice3;
    case engine::GameEventType::RockfordDied:
    case engine::GameEventType::MagicWallActivated:
        break;
    }

    Q_ASSERT(false);
    return LogicalVoice::Voice1;
}

std::size_t AudioController::logicalVoiceIndex(const LogicalVoice voice) noexcept
{
    const std::size_t index = static_cast<std::size_t>(voice);
    Q_ASSERT(index < kLogicalVoiceCount);
    return index;
}

} // namespace boulderdash::audio

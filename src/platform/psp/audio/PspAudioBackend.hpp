#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <pspkernel.h>

#include "AudioAsset.hpp"
#include "AudioPlaybackRequest.hpp"
#include "IAudioBackend.hpp"

namespace helengine::psp {
    /// <summary>
    /// Plays shared Helengine PCM audio through the PSP audio output channel.
    /// </summary>
    class PspAudioBackend final : public ::IAudioBackend {
    public:
        /// <summary>
        /// Creates one PSP audio backend and starts the dedicated output thread.
        /// </summary>
        PspAudioBackend();

        /// <summary>
        /// Stops playback and releases the reserved PSP audio channel.
        /// </summary>
        ~PspAudioBackend();

        int32_t Play(::AudioAsset* asset, ::AudioPlaybackRequest* request) override;

        void Stop(int32_t voiceId) override;

        void SetBusGain(std::string busId, float gain) override;

        void SetBusPaused(std::string busId, bool paused) override;

        bool IsPlaying(int32_t voiceId) override;

        void Update() override;

    private:
        static constexpr int OutputSampleRate = 44100;
        static constexpr int OutputChannelCount = 2;
        static constexpr int BufferFrameCount = 1024;
        static constexpr int ThreadPriority = 0x18;
        static constexpr int ThreadStackSize = 0x4000;

        struct ActiveVoiceState {
            int32_t VoiceId;
            const int16_t* SourceSamples;
            int32_t SourceSampleCount;
            int32_t SourceSampleRate;
            std::string BusId;
            float BaseGain;
            bool Loop;
            bool Playing;
            std::uint64_t SourceCursorQ16;
        };

        static int AudioThreadEntry(SceSize argumentsSize, void* arguments);

        void RunAudioThread();

        int FillOutputBufferLocked();

        float ResolveCombinedGainLocked(const std::string& busId, float baseGain) const;

        bool IsBusPausedLocked(const std::string& busId) const;

        static std::string NormalizeBusId(std::string busId);

        static float ClampGain(float gain);

        static int ConvertGainToVolume(float gain);

        std::atomic<bool> Running;
        int ChannelId;
        SceUID ThreadId;
        int32_t NextVoiceId;
        bool HasActiveVoice;
        bool HasLoggedActiveMix;
        ActiveVoiceState ActiveVoice;
        std::unordered_map<std::string, float> BusGainsById;
        std::unordered_set<std::string> PausedBusIds;
        std::array<std::int16_t, BufferFrameCount * OutputChannelCount> OutputBuffer;
        mutable std::mutex StateMutex;
    };
}

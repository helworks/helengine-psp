#include "platform/psp/audio/PspAudioBackend.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <stdexcept>

#include <pspaudio.h>
#include <pspthreadman.h>

#include "platform/psp/PspBootTrace.hpp"

namespace helengine::psp {
    PspAudioBackend::PspAudioBackend()
        : Running(true),
          ChannelId(-1),
          ThreadId(-1),
          NextVoiceId(0),
          HasActiveVoice(false),
          HasLoggedActiveMix(false),
          ActiveVoice(),
          BusGainsById(),
          PausedBusIds(),
          OutputBuffer(),
          StateMutex() {
        BusGainsById.emplace("master", 1.0f);
        BusGainsById.emplace("music", 1.0f);
        BusGainsById.emplace("sfx", 1.0f);

        ChannelId = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, BufferFrameCount, PSP_AUDIO_FORMAT_STEREO);
        if (ChannelId < 0) {
            throw std::runtime_error("Failed to reserve the PSP audio output channel.");
        }

        ThreadId = sceKernelCreateThread(
            "helengine_psp_audio",
            &PspAudioBackend::AudioThreadEntry,
            ThreadPriority,
            ThreadStackSize,
            PSP_THREAD_ATTR_USER,
            nullptr);
        if (ThreadId < 0) {
            sceAudioChRelease(ChannelId);
            ChannelId = -1;
            throw std::runtime_error("Failed to create the PSP audio output thread.");
        }

        PspAudioBackend* backend = this;
        int startResult = sceKernelStartThread(ThreadId, sizeof(PspAudioBackend*), &backend);
        if (startResult < 0) {
            sceKernelDeleteThread(ThreadId);
            ThreadId = -1;
            sceAudioChRelease(ChannelId);
            ChannelId = -1;
            throw std::runtime_error("Failed to start the PSP audio output thread.");
        }
    }

    PspAudioBackend::~PspAudioBackend() {
        Running.store(false);

        if (ThreadId >= 0) {
            sceKernelWaitThreadEnd(ThreadId, nullptr);
            sceKernelDeleteThread(ThreadId);
            ThreadId = -1;
        }

        if (ChannelId >= 0) {
            sceAudioChRelease(ChannelId);
            ChannelId = -1;
        }
    }

    int32_t PspAudioBackend::Play(::AudioAsset* asset, ::AudioPlaybackRequest* request) {
        if (asset == nullptr) {
            throw std::invalid_argument("asset");
        }
        if (asset->SampleRate <= 0) {
            throw std::runtime_error("PSP audio playback requires one audio asset with a positive sample rate.");
        }
        if (asset->Channels != 1) {
            throw std::runtime_error("PSP audio playback currently requires mono cooked assets.");
        }
        if (asset->EncodedBytes == nullptr || asset->EncodedBytes->Length <= 0 || asset->EncodedBytes->Data == nullptr) {
            throw std::runtime_error("PSP audio playback requires one non-empty encoded payload.");
        }
        if ((asset->EncodedBytes->Length % static_cast<int32_t>(sizeof(std::int16_t))) != 0) {
            throw std::runtime_error("PSP audio playback requires 16-bit PCM sample alignment.");
        }

        std::lock_guard<std::mutex> lock(StateMutex);

        ActiveVoiceState voice = {};
        voice.VoiceId = NextVoiceId++;
        voice.SourceSamples = reinterpret_cast<const std::int16_t*>(asset->EncodedBytes->Data);
        voice.SourceSampleCount = asset->EncodedBytes->Length / static_cast<int32_t>(sizeof(std::int16_t));
        voice.SourceSampleRate = asset->SampleRate;
        voice.BusId = NormalizeBusId(request != nullptr && !request->BusId.empty() ? request->BusId : asset->DefaultBusId);
        voice.BaseGain = ClampGain(request != nullptr ? request->Gain : 1.0f);
        voice.Loop = request != nullptr ? request->Loop : asset->DefaultLoop;
        voice.Playing = true;
        voice.SourceCursorQ16 = 0;

        ActiveVoice = voice;
        HasActiveVoice = true;
        HasLoggedActiveMix = false;
        PspBootTrace::WriteLine(
            std::string("PspAudioPlay voiceId=") + std::to_string(voice.VoiceId)
            + " channels=" + std::to_string(asset->Channels)
            + " sampleRate=" + std::to_string(asset->SampleRate)
            + " sampleCount=" + std::to_string(voice.SourceSampleCount)
            + " loop=" + (voice.Loop ? "true" : "false")
            + " bus=" + voice.BusId
            + " gain=" + std::to_string(voice.BaseGain));
        return voice.VoiceId;
    }

    void PspAudioBackend::Stop(int32_t voiceId) {
        std::lock_guard<std::mutex> lock(StateMutex);
        if (!HasActiveVoice || ActiveVoice.VoiceId != voiceId) {
            return;
        }

        PspBootTrace::WriteLine(std::string("PspAudioStop voiceId=") + std::to_string(voiceId));
        ActiveVoice.Playing = false;
        HasActiveVoice = false;
        HasLoggedActiveMix = false;
    }

    void PspAudioBackend::SetBusGain(std::string busId, float gain) {
        std::lock_guard<std::mutex> lock(StateMutex);
        BusGainsById[NormalizeBusId(std::move(busId))] = ClampGain(gain);
    }

    void PspAudioBackend::SetBusPaused(std::string busId, bool paused) {
        std::lock_guard<std::mutex> lock(StateMutex);
        std::string normalizedBusId = NormalizeBusId(std::move(busId));
        if (paused) {
            PausedBusIds.insert(normalizedBusId);
        } else {
            PausedBusIds.erase(normalizedBusId);
        }
    }

    bool PspAudioBackend::IsPlaying(int32_t voiceId) {
        std::lock_guard<std::mutex> lock(StateMutex);
        return HasActiveVoice && ActiveVoice.VoiceId == voiceId && ActiveVoice.Playing;
    }

    void PspAudioBackend::Update() {
    }

    int PspAudioBackend::AudioThreadEntry(SceSize argumentsSize, void* arguments) {
        if (arguments == nullptr || argumentsSize != sizeof(PspAudioBackend*)) {
            return 0;
        }

        PspAudioBackend* backend = *reinterpret_cast<PspAudioBackend**>(arguments);
        if (backend == nullptr) {
            return 0;
        }

        backend->RunAudioThread();
        return 0;
    }

    void PspAudioBackend::RunAudioThread() {
        while (Running.load()) {
            int volume = 0;
            {
                std::lock_guard<std::mutex> lock(StateMutex);
                volume = FillOutputBufferLocked();
            }

            int clampedVolume = std::clamp(volume, 0, PSP_AUDIO_VOLUME_MAX);
            sceAudioOutputPannedBlocking(ChannelId, clampedVolume, clampedVolume, OutputBuffer.data());
        }
    }

    int PspAudioBackend::FillOutputBufferLocked() {
        std::fill(OutputBuffer.begin(), OutputBuffer.end(), static_cast<std::int16_t>(0));

        if (!HasActiveVoice || !ActiveVoice.Playing) {
            return 0;
        }
        if (IsBusPausedLocked(ActiveVoice.BusId)) {
            return 0;
        }

        float combinedGain = ResolveCombinedGainLocked(ActiveVoice.BusId, ActiveVoice.BaseGain);
        if (combinedGain <= 0.0f) {
            return 0;
        }

        if (!HasLoggedActiveMix) {
            PspBootTrace::WriteLine(
                std::string("PspAudioMix voiceId=") + std::to_string(ActiveVoice.VoiceId)
                + " combinedGain=" + std::to_string(combinedGain)
                + " cursorQ16=" + std::to_string(ActiveVoice.SourceCursorQ16)
                + " sourceSamples=" + std::to_string(ActiveVoice.SourceSampleCount));
            HasLoggedActiveMix = true;
        }

        std::uint64_t stepQ16 = (static_cast<std::uint64_t>(ActiveVoice.SourceSampleRate) << 16) / OutputSampleRate;
        if (stepQ16 == 0) {
            stepQ16 = 1;
        }

        std::uint64_t loopLengthQ16 = static_cast<std::uint64_t>(ActiveVoice.SourceSampleCount) << 16;
        for (int frameIndex = 0; frameIndex < BufferFrameCount; frameIndex++) {
            int32_t sourceIndex = static_cast<int32_t>(ActiveVoice.SourceCursorQ16 >> 16);
            if (sourceIndex >= ActiveVoice.SourceSampleCount) {
                if (!ActiveVoice.Loop || ActiveVoice.SourceSampleCount <= 0) {
                    ActiveVoice.Playing = false;
                    HasActiveVoice = false;
                    HasLoggedActiveMix = false;
                    break;
                }

                ActiveVoice.SourceCursorQ16 %= loopLengthQ16;
                sourceIndex = static_cast<int32_t>(ActiveVoice.SourceCursorQ16 >> 16);
            }

            std::int16_t sample = ActiveVoice.SourceSamples[sourceIndex];
            OutputBuffer[frameIndex * OutputChannelCount] = sample;
            OutputBuffer[(frameIndex * OutputChannelCount) + 1] = sample;
            ActiveVoice.SourceCursorQ16 += stepQ16;
        }

        return ConvertGainToVolume(combinedGain);
    }

    float PspAudioBackend::ResolveCombinedGainLocked(const std::string& busId, float baseGain) const {
        float masterGain = 1.0f;
        auto masterGainIterator = BusGainsById.find("master");
        if (masterGainIterator != BusGainsById.end()) {
            masterGain = masterGainIterator->second;
        }

        float busGain = 1.0f;
        auto busGainIterator = BusGainsById.find(busId);
        if (busGainIterator != BusGainsById.end()) {
            busGain = busGainIterator->second;
        }

        return ClampGain(masterGain * busGain * baseGain);
    }

    bool PspAudioBackend::IsBusPausedLocked(const std::string& busId) const {
        return PausedBusIds.contains("master") || PausedBusIds.contains(busId);
    }

    std::string PspAudioBackend::NormalizeBusId(std::string busId) {
        if (busId.empty()) {
            return "master";
        }

        std::transform(
            busId.begin(),
            busId.end(),
            busId.begin(),
            [](unsigned char value) {
                return static_cast<char>(std::tolower(value));
            });
        return busId;
    }

    float PspAudioBackend::ClampGain(float gain) {
        if (!(gain >= 0.0f) || gain != gain) {
            return 0.0f;
        }

        return std::clamp(gain, 0.0f, 1.0f);
    }

    int PspAudioBackend::ConvertGainToVolume(float gain) {
        return static_cast<int>(std::clamp(gain, 0.0f, 1.0f) * PSP_AUDIO_VOLUME_MAX);
    }
}

// the mixer pulled every 10 ms by the clock (on a fixed schedule, as a sound device pulls), its output dropped
// XBE_AUDIO_TICKS=1: pulled by the frames the game presents instead, kRate/60 samples a frame - two machines
// then consume the same stream at the same tick and their audio rings end the same, which a session's memory
// comparison needs (by the clock they drift a few samples a minute; the net tests set it)
// WAV=file: the output also written there (relative to the run folder), 48 kHz 16 bit stereo, the header kept current
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

#include "app/app.h"
#include "audio/audio.h"
#include "core/log.h"
#include "harness.h"

namespace audio {

namespace {

// a RIFF WAVE header for bytes of 16 bit stereo PCM at kRate
void WavHeader(FILE* f, uint32_t bytes) {
    auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
    std::fseek(f, 0, SEEK_SET);
    std::fwrite("RIFF", 1, 4, f);
    u32(36 + bytes);
    std::fwrite("WAVEfmt ", 1, 8, f);
    u32(16);
    u16(1);  // PCM
    u16(2);
    u32(kRate);
    u32(kRate * 4);
    u16(4);
    u16(16);
    std::fwrite("data", 1, 4, f);
    u32(bytes);
    std::fseek(f, 0, SEEK_END);
}

}  // namespace

void StartOutput(std::function<void(float*, uint32_t)> render) {
    FILE* wav = nullptr;
    if (const char* name = std::getenv("WAV"); name && *name) {
        wav = std::fopen((app::ExeDir() / name).string().c_str(), "wb");
        if (wav) WavHeader(wav, 0);
        LOGI("audio: output started, written to %s", name);  // the log's time of the file's first sample
    }
    std::thread([render, wav] {
        std::vector<float> b(kRate / 100 * 2);
        std::vector<int16_t> pcm(b.size());
        bool ticks = [] {
            const char* e = std::getenv("XBE_AUDIO_TICKS");
            return e && *e == '1';
        }();
        uint64_t mixed = 0;  // samples pulled by frames so far (XBE_AUDIO_TICKS)
        uint32_t bytes = 0;
        auto next = std::chrono::steady_clock::now();
        for (int n = 0;; n++) {
            if (ticks) {
                // the game's own clock: one frame is kRate/60 samples. Holds while the game holds (loading, a
                // session's barrier waiting for a slow machine), and two machines present the same frames
                for (;;) {
                    uint64_t want = harness::g_frames.load() * (kRate / 60);
                    if (mixed < want) break;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                uint64_t want = harness::g_frames.load() * (kRate / 60);
                int pulls = 0;
                while (mixed < want) {
                    uint32_t c = uint32_t(std::min<uint64_t>(kRate / 100, want - mixed));
                    render(b.data(), c);
                    if (wav) {
                        for (size_t i = 0; i < size_t(c) * 2; i++) pcm[i] = int16_t(std::lround(b[i] * 32767.0f));
                        std::fwrite(pcm.data(), 2, size_t(c) * 2, wav);
                        bytes += uint32_t(c) * 4;
                        if (++pulls % 100 == 0) {
                            WavHeader(wav, bytes);
                            std::fflush(wav);
                        }
                    }
                    mixed += c;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            render(b.data(), kRate / 100);
            if (wav) {
                for (size_t i = 0; i < b.size(); i++) pcm[i] = int16_t(std::lround(b[i] * 32767.0f));
                std::fwrite(pcm.data(), 2, pcm.size(), wav);
                bytes += uint32_t(pcm.size() * 2);
                if (n % 100 == 0) {  // a run ends by being killed: the header says what is there each second
                    WavHeader(wav, bytes);
                    std::fflush(wav);
                }
            }
            next += std::chrono::milliseconds(10);
            std::this_thread::sleep_until(next);
        }
    }).detach();
}

}  // namespace audio

#include "Audio/AudioDevice.h"
#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>
#include <vector>

int main()
{
    using namespace Janus;
    // A real output device is required: no silent fallback can make this smoke pass.
    for (int run = 0; run < 3; ++run)
    {
        auto opened = CreateDefaultAudioDevice();
        if (!opened)
        {
            std::cerr << opened.GetError().message << '\n';
            return 1;
        }
        auto device = std::move(opened).Value();
        std::vector<f32> samples(AudioDevice::MaxBlockFrames * AudioDevice::Channels);
        const auto start = std::chrono::steady_clock::now();
        for (int block = 0; block < 10; ++block)
        {
            for (usize i = 0; i < AudioDevice::MaxBlockFrames; ++i)
            {
                const auto frame = static_cast<usize>(block) * AudioDevice::MaxBlockFrames + i;
                const f64 t = static_cast<f64>(frame) / AudioDevice::SampleRate;
                const f32 value =
                    static_cast<f32>(0.06 * std::sin(6.283185307179586 * (330 + run * 110) * t));
                samples[2 * i] = samples[2 * i + 1] = value;
            }
            auto submitted = device->Submit(samples);
            if (!submitted)
            {
                std::cerr << submitted.GetError().message << '\n';
                return 2;
            }
            std::this_thread::sleep_until(start + std::chrono::milliseconds(100 * (block + 1)));
        }
        device->Clear();
        std::cout << "Device cycle " << run + 1
                  << ": open, 48000 frames submitted, clear, close.\n";
    }
    std::cout << "Audio device smoke passed; audible quality requires human listening.\n";
}

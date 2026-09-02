#include "Application/Application.h"
#include "Application/ApplicationClient.h"

#include "Core/Log/Log.h"
#include "Core/Math/Vector2.h"
#include "Core/Time/TimeStep.h"
#include "Renderer/OrthographicCamera.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/Sprite.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>

class BenchmarkClient final : public Janus::ApplicationClient
{
public:
    explicit BenchmarkClient(Janus::u32 spriteCount)
        : m_SpriteCount(spriteCount)
    {
    }

    Janus::Result<void> OnInitialize(Janus::Application& application) override
    {
        auto& renderer = application.GetRenderer2D();

        std::vector<Janus::u8> pixels = {
            255, 255, 255, 255,
            255, 0, 0, 255,
            0, 255, 0, 255,
            0, 0, 255, 255};

        Janus::TextureDesc desc;
        desc.width = 2;
        desc.height = 2;
        desc.data = pixels.data();
        desc.dataSize = pixels.size();

        const auto texture = renderer.CreateTexture(desc);

        if (!texture)
        {
            return Janus::Result<void>::Failure(texture.GetError());
        }

        m_Texture = texture.Value();

        return Janus::Result<void>::Success();
    }

    void OnUpdate(Janus::TimeStep, Janus::Application& application) override
    {
        auto& renderer = application.GetRenderer2D();

        const auto start = std::chrono::steady_clock::now();

        Janus::OrthographicCamera camera;
        renderer.BeginFrame(camera);

        Janus::Sprite sprite;
        sprite.texture = m_Texture;
        sprite.size = Janus::Vector2{16.0f, 16.0f};

        for (Janus::u32 index = 0; index < m_SpriteCount; ++index)
        {
            sprite.position = Janus::Vector2{
                static_cast<Janus::f32>((index % 100) * 20),
                static_cast<Janus::f32>((index / 100) * 20)};

            renderer.SubmitSprite(sprite);
        }

        const auto endResult = renderer.EndFrame();

        if (!endResult)
        {
            JANUS_ERROR("Renderer EndFrame failed: {}", endResult.GetError().message);
            application.RequestExit();
            return;
        }

        const auto end = std::chrono::steady_clock::now();
        m_TotalSeconds += std::chrono::duration<Janus::f64>(
            end - start).count();

        ++m_Frames;

        if (m_Frames == 120)
        {
            const auto& stats = renderer.GetStatistics();

            std::printf(
                "Sprites: %u\n",
                m_SpriteCount);
            std::printf(
                "Frame Time: %.6f ms\n",
                (m_TotalSeconds / m_Frames) * 1000.0);
            std::printf(
                "Draw Calls: %u\n",
                stats.drawCallCount);
            std::printf(
                "Batch Count: %u\n",
                stats.batchCount);

            application.RequestExit();
        }
    }

private:
    Janus::u32 m_SpriteCount = 0;
    Janus::u32 m_Frames = 0;
    Janus::f64 m_TotalSeconds = 0.0;
    Janus::TextureHandle m_Texture;
};

int main(int argc, char** argv)
{
    Janus::u32 spriteCount = 1000;

    if (argc > 1)
    {
        spriteCount = static_cast<Janus::u32>(
            std::strtoul(argv[1], nullptr, 10));
    }

    Janus::ApplicationConfig config;
    config.window.title = "Janus Renderer2D Benchmark";
    config.window.width = 1280;
    config.window.height = 720;
    config.window.resizable = false;

    Janus::Application application(config);
    BenchmarkClient client(spriteCount);

    const auto result = application.Run(client);

    if (!result)
    {
        std::fprintf(
            stderr,
            "Janus benchmark failed: %s\n",
            result.GetError().message.c_str());

        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

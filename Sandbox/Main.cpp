#include "Application/Application.h"
#include "Application/ApplicationClient.h"

#include "Core/Event/Event.h"
#include "Core/Log/Log.h"
#include "Core/Math/Vector2.h"
#include "Core/Time/TimeStep.h"
#include "Renderer/OrthographicCamera.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/Sprite.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <variant>

class SandboxClient final : public Janus::ApplicationClient
{
public:
    Janus::Result<void> OnInitialize(Janus::Application& application) override
    {
        auto& renderer = application.GetRenderer2D();

        const auto checker = CreateCheckerTexture(renderer, false);

        if (!checker)
        {
            return Janus::Result<void>::Failure(checker.GetError());
        }

        const auto transparent = CreateCheckerTexture(renderer, true);

        if (!transparent)
        {
            renderer.DestroyTexture(checker.Value());
            return Janus::Result<void>::Failure(transparent.GetError());
        }

        m_CheckerTexture = checker.Value();
        m_TransparentTexture = transparent.Value();

        return Janus::Result<void>::Success();
    }

    void OnEvent(const Janus::Event& event, Janus::Application&) override
    {
        if (const auto* resize = std::get_if<Janus::WindowResizeEvent>(&event))
        {
            JANUS_INFO("Window resized to {}x{}.", resize->width, resize->height);
        }
    }

    void OnUpdate(Janus::TimeStep timeStep, Janus::Application& application) override
    {
        if (application.GetInput().WasKeyPressed(Janus::KeyCode::Escape))
        {
            application.RequestExit();
            return;
        }

        m_ElapsedSeconds += timeStep.GetSeconds();

        Janus::OrthographicCamera camera;
        camera.position = Janus::Vector2{
            std::sin(m_ElapsedSeconds) * 200.0f,
            std::cos(m_ElapsedSeconds) * 80.0f};

        auto& renderer = application.GetRenderer2D();
        renderer.BeginFrame(camera);

        Janus::Sprite atlasSprite;
        atlasSprite.texture = m_CheckerTexture;
        atlasSprite.position = Janus::Vector2{0.0f, 0.0f};
        atlasSprite.size = Janus::Vector2{256.0f, 256.0f};
        atlasSprite.uv = Janus::TextureRegion{
            Janus::Vector2{0.0f, 0.0f},
            Janus::Vector2{1.0f, 1.0f}};
        atlasSprite.color.a = 1.0f;

        renderer.SubmitSprite(atlasSprite);

        Janus::Sprite transparentSprite;
        transparentSprite.texture = m_TransparentTexture;
        transparentSprite.position = Janus::Vector2{120.0f, 60.0f};
        transparentSprite.size = Janus::Vector2{128.0f, 128.0f};
        transparentSprite.rotationRadians = m_ElapsedSeconds;
        transparentSprite.color.a = 0.55f;

        renderer.SubmitSprite(transparentSprite);

        const auto result = renderer.EndFrame();

        if (!result)
        {
            JANUS_ERROR("Renderer EndFrame failed: {}", result.GetError().message);
        }
    }

private:
    static Janus::Result<Janus::TextureHandle> CreateCheckerTexture(
        Janus::Renderer2D& renderer,
        bool transparent)
    {
        constexpr Janus::u32 size = 4;
        std::vector<Janus::u8> pixels(
            size * size * 4);

        for (Janus::u32 y = 0; y < size; ++y)
        {
            for (Janus::u32 x = 0; x < size; ++x)
            {
                const bool bright = ((x + y) % 2) == 0;
                const Janus::u8 value = bright ? 220 : 40;
                const Janus::u8 alpha = transparent ? 128 : 255;
                const Janus::usize offset =
                    (static_cast<Janus::usize>(y) * size + x) * 4;

                pixels[offset] = value;
                pixels[offset + 1] = bright ? 80 : 30;
                pixels[offset + 2] = bright ? 220 : 100;
                pixels[offset + 3] = alpha;
            }
        }

        Janus::TextureDesc desc;
        desc.width = size;
        desc.height = size;
        desc.data = pixels.data();
        desc.dataSize = pixels.size();

        return renderer.CreateTexture(desc);
    }

    Janus::TextureHandle m_CheckerTexture;
    Janus::TextureHandle m_TransparentTexture;
    float m_ElapsedSeconds = 0.0f;
};

int main()
{
    Janus::ApplicationConfig config;
    config.window.title = "Janus Sandbox";
    config.window.width = 1280;
    config.window.height = 720;
    config.window.resizable = true;

    Janus::Application application(config);
    SandboxClient client;
    const auto result = application.Run(client);

    if (!result)
    {
        std::fprintf(stderr, "Janus failed: %s\n", result.GetError().message.c_str());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

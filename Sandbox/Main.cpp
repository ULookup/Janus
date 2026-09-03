#include "Application/Application.h"
#include "Application/ApplicationClient.h"

#include "Core/Event/Event.h"
#include "Core/Log/Log.h"
#include "Core/Math/Vector2.h"
#include "Core/Time/TimeStep.h"
#include "Scene/Scene.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <variant>

class SandboxClient final : public Janus::ApplicationClient
{
public:
    Janus::Result<void> OnInitialize(Janus::Application& application) override
    {
        auto& scene = application.GetScene();
        scene.SetName("Sandbox Transitional Scene");

        const auto camera = scene.CreateEntity("Camera");
        scene.AddComponent<Janus::CameraComponent>(
            camera,
            Janus::CameraComponent{1.0f, true});

        // v0.4 #11 intentionally removes the old runtime TextureHandle-backed
        // sprite setup. #12 replaces this transitional scene with the
        // disk-backed SandboxProject/AssetRegistry/Scene vertical slice.
        for (int index = 0; index < 8; ++index)
        {
            const auto entity = scene.CreateEntity(
                "Entity " + std::to_string(index));
            auto* transform =
                scene.GetComponent<Janus::TransformComponent>(entity);
            transform->position = Janus::Vector2{
                static_cast<Janus::f32>((index % 4) * 80 - 120),
                static_cast<Janus::f32>((index / 4) * 70 - 40)};
        }

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

        auto& scene = application.GetScene();
        scene.View<Janus::TransformComponent,
                   Janus::CameraComponent>()
            .ForEach(
                [&](Janus::ECS::Entity,
                    Janus::TransformComponent& transform,
                    Janus::CameraComponent&)
                {
                    transform.position = Janus::Vector2{
                        static_cast<Janus::f32>(
                            std::sin(m_ElapsedSeconds)) * 200.0f,
                        static_cast<Janus::f32>(
                            std::cos(m_ElapsedSeconds)) * 80.0f};
                });
    }

private:
    Janus::f64 m_ElapsedSeconds = 0.0;
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

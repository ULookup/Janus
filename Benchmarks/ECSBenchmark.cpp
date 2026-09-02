#include "Scene/Scene.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>

int main(int argc, char** argv)
{
    Janus::usize entityCount = 10000;

    if (argc > 1)
    {
        entityCount = static_cast<Janus::usize>(
            std::strtoull(argv[1], nullptr, 10));
    }

    Janus::Scene scene;

    std::vector<Janus::ECS::Entity> entities;
    entities.reserve(entityCount);

    const auto startCreate = std::chrono::steady_clock::now();
    for (Janus::usize index = 0; index < entityCount; ++index)
    {
        entities.push_back(scene.CreateEntity());
    }
    const auto endCreate = std::chrono::steady_clock::now();

    const auto startIterate = std::chrono::steady_clock::now();
    scene.View<Janus::TransformComponent>()
        .ForEach(
            [](Janus::ECS::Entity,
               Janus::TransformComponent& transform)
            {
                transform.position.x += 1.0f;
            });
    const auto endIterate = std::chrono::steady_clock::now();

    const auto startDestroy = std::chrono::steady_clock::now();
    for (Janus::ECS::Entity entity : entities)
    {
        scene.DestroyEntity(entity);
    }
    const auto endDestroy = std::chrono::steady_clock::now();

    const auto ms = [](auto start, auto end)
    {
        return std::chrono::duration<Janus::f64>(
            end - start).count() * 1000.0;
    };

    std::printf("Entities: %zu\n", entityCount);
    std::printf("Create: %.6f ms\n", ms(startCreate, endCreate));
    std::printf("Transform Iterate: %.6f ms\n", ms(startIterate, endIterate));
    std::printf("Destroy: %.6f ms\n", ms(startDestroy, endDestroy));

    return EXIT_SUCCESS;
}

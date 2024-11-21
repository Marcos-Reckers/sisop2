#pragma once

#include <thread>
#include <queue>
#include <mutex>
#include <optional>

namespace Threads
{
    template <typename T>
    class AtomicQueue
    {
    private:
        std::mutex lock = std::mutex();
        std::queue<T> resources;

    public:
        AtomicQueue() : resources(std::queue<T>()) {}
        AtomicQueue(std::queue<T> resources) : resources(resources) {}

        void produce(T resource)
        {
            const std::lock_guard<std::mutex> lock_guard(lock);
            resources.push(resource);
        }

        std::optional<T> consume()
        {
            const std::lock_guard<std::mutex> lock_guard(lock);
            if (resources.empty())
            {
                // std::cout << "No resources to consume" << std::endl;
                return std::nullopt;
            }

            auto resource = resources.front();
            resources.pop();
            return resource;
        }

        T consume_blocking()
        {
            auto resource = this->consume();
            while (!resource.has_value())
            {
                std::this_thread::yield();
                resource = this->consume();
            }
            return resource.value();
        }
    };
} // namespace Threads (usando código do IAN KERSZ AMARAL)
#pragma once
#include "ysfx_gmem.hpp"
#include "WDL/eel2/ns-eel.h"
#include <unordered_map>
#include <mutex>


namespace
{
    struct gmem_context
    {
        ~gmem_context()
        {
            NSEEL_VM_FreeGRAM(&ctx);
        }

        std::size_t refcount = 1;
        void* ctx = nullptr;
    };

    std::unordered_map<std::string, std::unique_ptr<gmem_context>> gmem_registry;

    std::mutex gmem_registry_mutex;
}


static void** acquire_gmem(std::string_view name)
{
    std::lock_guard lock(gmem_registry_mutex);

    std::string key(name);
    auto it = gmem_registry.find(key);

    if (it == gmem_registry.end())
    {
        auto [new_it, _] = gmem_registry.emplace(key, std::make_unique<gmem_context>());

        return &new_it->second->ctx;
    }

    ++it->second->refcount;

    return &it->second->ctx;
}


static void release_gmem(std::string_view name)
{
    std::lock_guard lock(gmem_registry_mutex);

    auto it = gmem_registry.find(std::string(name));

    if (it == gmem_registry.end())
    {
        // Double detach somehow? Should probably log this.
        return;
    }

    if (--it->second->refcount == 0)
        gmem_registry.erase(it);
}


void ysfx_gmem_detach(ysfx_t* fx)
{
    if (fx->gmem_name.empty())
        return;

    NSEEL_VM_SetGRAM(fx->vm.get(), nullptr);
    release_gmem(fx->gmem_name);

    fx->gmem_name.clear();
}


void ysfx_gmem_attach(ysfx_t* fx, std::string_view name)
{
    if (fx->gmem_name == name)
        return;

    ysfx_gmem_detach(fx);

    if (name.empty())
    {
        NSEEL_VM_SetGRAM(fx->vm.get(), nullptr);
        return;
    }

    void** ctx = acquire_gmem(name);
    NSEEL_VM_SetGRAM(fx->vm.get(), ctx);

    fx->gmem_name = name;
}

std::string get_gmem_identifier(ysfx_t* fx)
{
    return fx->gmem_name;
}

void** get_gmem_address(ysfx_t* fx)
{
    std::lock_guard lock(gmem_registry_mutex);
    
    std::string name = fx->gmem_name;

    if (name.empty())
        return nullptr;

    auto it = gmem_registry.find(std::string(name));

    if (it == gmem_registry.end())
        return nullptr;
    else
        return &it->second->ctx;
}

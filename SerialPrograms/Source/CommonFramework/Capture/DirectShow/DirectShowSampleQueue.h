/*  DirectShow Capture
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_DirectShowSampleQueue_H
#define PokemonAutomation_DirectShowSampleQueue_H

#include <atomic>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <cstdint>
#include <vector>
#include "Common/Cpp/Concurrency/Mutex.h"
#include "Common/Cpp/Time.h"

namespace PokemonAutomation::DirectShow{

struct SampleBuffer{
    std::vector<unsigned char> bytes;
    size_t size = 0;
    int64_t start = 0;
    int64_t end = 0;
    bool discontinuity = false;
    WallClock arrival = (WallClock::min)();
};

// Storage is allocated before streaming. Shared consumers cannot cause the
// allocator to grow: retained slot_count are unavailable until all readers release.
// Callbacks never wait for a consumer or invoke downstream processing.
class SampleQueue{
public:
    SampleQueue(size_t slot_count, size_t bytes, bool latest_only)
        : m_latest_only(latest_only)
    {
        if (slot_count == 0 || slot_count > 64 || bytes == 0 || bytes > 64 * 1024 * 1024 ||
            slot_count * bytes > 256 * 1024 * 1024){
            throw std::invalid_argument("Invalid DirectShow sample buffer capacity.");
        }
        for (size_t i = 0; i < slot_count; i++){
            auto slot = std::make_shared<SampleBuffer>();
            slot->bytes.resize(bytes);
            m_storage.push_back(std::move(slot));
        }
        m_pending.resize(slot_count);
    }

    bool push(const unsigned char* bytes, size_t size, int64_t start, int64_t end, bool discontinuity) noexcept{
        std::unique_lock<Mutex> lock(m_lock, std::try_to_lock);
        if (!lock || m_closed.load(std::memory_order_relaxed)){
            m_dropped.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        if (!bytes || size == 0 || size > m_storage.front()->bytes.size()){
            m_dropped.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        if (m_latest_only){
            m_dropped.fetch_add(m_count, std::memory_order_relaxed);
            clear_pending();
        }
        for (const auto& slot : m_storage){
            if (slot.use_count() != 1){
                continue;
            }
            slot->arrival = current_time();
            memcpy(slot->bytes.data(), bytes, size);
            slot->size = size;
            slot->start = start;
            slot->end = end;
            slot->discontinuity = discontinuity;
            m_pending[(m_head + m_count) % m_pending.size()] = slot;
            m_count++;
            return true;
        }
        m_dropped.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    std::shared_ptr<const SampleBuffer> pop(){
        std::lock_guard<Mutex> lock(m_lock);
        if (m_count == 0){
            return {};
        }
        auto result = std::move(m_pending[m_head]);
        m_head = (m_head + 1) % m_pending.size();
        m_count--;
        return result;
    }
    void flush(){
        std::lock_guard<Mutex> lock(m_lock);
        clear_pending();
    }
    void close(){
        std::lock_guard<Mutex> lock(m_lock);
        m_closed.store(true, std::memory_order_relaxed);
        clear_pending();
    }
    uint64_t dropped() const{ return m_dropped.load(std::memory_order_relaxed); }

private:
    void clear_pending(){
        for (auto& slot : m_pending){
            slot.reset();
        }
        m_count = 0;
        m_head = 0;
    }
    const bool m_latest_only;
    Mutex m_lock;
    std::vector<std::shared_ptr<SampleBuffer>> m_storage;
    std::vector<std::shared_ptr<SampleBuffer>> m_pending;
    size_t m_head = 0;
    size_t m_count = 0;
    std::atomic<bool> m_closed{false};
    std::atomic<uint64_t> m_dropped{0};
};

}
#endif

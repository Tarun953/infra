#ifndef CONTAINERS_MEMORY_POOL_HPP
#define CONTAINERS_MEMORY_POOL_HPP

#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>

namespace infra
{
    namespace containers
    {

        static constexpr std::size_t CACHE_LINE_SIZE = std::hardware_constructive_interference_size;

        template <typename T, std::size_t PoolSize>
        class MemoryPool
        {
            static_assert(PoolSize > 0, "PoolSize must be greater than 0");
            static_assert(std::is_nothrow_destructible<T>::value, "T must be nothrow destructible");

            union Block
            {
                alignas(T) std::byte data[sizeof(T)];
                Block *next;
            };

        public:
            MemoryPool() noexcept
            {
                for (std::size_t i = 0; i < PoolSize - 1; i++)
                {
                    pool_[i].next = &pool_[i + 1];
                }
                pool_[PoolSize - 1].next = nullptr;
                freeList_ = &pool_[0];
            }

            // Delete the copy and move constructors and assignment operators
            MemoryPool(const MemoryPool &) = delete;
            MemoryPool &operator=(const MemoryPool &) = delete;
            MemoryPool(MemoryPool &&) = delete;
            MemoryPool &operator=(MemoryPool &&) = delete;

            ~MemoryPool() noexcept = default;

            template <typename... Args>
            [[nodiscard]] T *allocate(Args &&...args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
            {
                if (!freeList_) [[unlikely]]
                {
                    return nullptr; // Pool exhausted
                }

                Block *block = freeList_;
                freeList_ = block->next;
                ::new (&(block->data)) T(std::forward<Args>(args)...);
                ++allocated_;
                return reinterpret_cast<T *>(block);
            }

            void deallocate(T *obj)
            {
                if (!obj) [[unlikely]]
                {
                    return; // Ignore null pointers
                }

                obj->~T(); // Call the destructor
                Block *block = reinterpret_cast<Block *>(obj);
                block->next = freeList_;
                freeList_ = block;
                --allocated_;
            }

        private:
            alignas(CACHE_LINE_SIZE) Block pool_[PoolSize];
            Block *freeList_ = nullptr;
            std::size_t allocated_ = 0;
        };
    }
}

#endif // CONTAINERS_MEMORY_POOL_HPP
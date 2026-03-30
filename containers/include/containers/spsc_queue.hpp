#ifndef CONTAINERS_SPSC_QUEUE_HPP
#define CONTAINERS_SPSC_QUEUE_HPP
#include <atomic>

namespace infra
{
    namespace containers
    {
        static constexpr std::size_t CACHE_LINE_SIZE = 64;

        template <typename T, std::size_t Capacity>
        class SPSCQueue
        {
            static_assert(std::is_nothrow_destructible_v<T>, "SPSCQueue requires a non-throwing destructor");
            static_assert(Capacity > 0, "SPSCQueue capacity must be greater than zero");
            static_assert((Capacity & (Capacity - 1)) == 0, "SPSCQueue capacity must be a power of two");

            static constexpr std::size_t MASK = Capacity - 1;

        private:
            alignas(CACHE_LINE_SIZE) std::atomic<std::size_t> head_{0};
            char pad1_[CACHE_LINE_SIZE - sizeof(std::atomic<std::size_t>)];

            alignas(CACHE_LINE_SIZE) std::atomic<std::size_t> tail_{0};
            char pad2_[CACHE_LINE_SIZE - sizeof(std::atomic<std::size_t>)];

            // This allows us to construct T in-place. It will not default-construct T, so we can manage the lifecycle manually.
            alignas(T) std::byte buffer_[Capacity][sizeof(T)];

        public:
            SPSCQueue() noexcept : head_(0), tail_(0) {};

            // Delete copy and move constructors and assignment operators to prevent copying or moving the queue.
            SPSCQueue(const SPSCQueue &) = delete;
            SPSCQueue(SPSCQueue &&) = delete;
            SPSCQueue &operator=(const SPSCQueue &) = delete;
            SPSCQueue &operator=(SPSCQueue &&) = delete;

            ~SPSCQueue() noexcept
            {
                T item;
                while (pop(item))
                    ;
            }

            template <typename... Args>
            [[nodiscard]] bool push(Args &&...args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
            {
                const std::size_t head = head_.load(std::memory_order_relaxed);
                const std::size_t next_head = (head + 1) & MASK;

                // Check if the queue is full by comparing the next head position with the tail position.
                if (next_head == tail_.load(std::memory_order_acquire))
                {
                    return false; // Queue is full
                }

                // Construct the new item in-place using placement new
                new (&buffer_[head]) T(std::forward<Args>(args)...);

                // Update the head after the item is constructed to ensure visibility to the consumer
                head_.store(next_head, std::memory_order_release);
                return true;
            }

            [[nodiscard]] bool pop(T &item) noexcept(std::is_nothrow_move_assignable_v<T>)
            {
                const std::size_t tail = tail_.load(std::memory_order_relaxed);

                if (tail == head_.load(std::memory_order_acquire))
                {
                    return false; // Queue is empty
                }

                const std::size_t next_tail = (tail + 1) & MASK;
                // __builtin_prefetch(&buffer_[next_tail], 0, 1); // Prefetch the next item for improved performance

                // Move the item from the buffer to the output parameter
                item = std::move(reinterpret_cast<T &>(buffer_[tail]));

                reinterpret_cast<T &>(buffer_[tail]).~T(); // Explicitly call the destructor for the item

                // Update the tail after the item is moved to ensure visibility to the producer
                tail_.store(next_tail, std::memory_order_release);
                return true;
            }

            [[nodiscard]] bool empty() const noexcept
            {
                return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
            }

            [[nodiscard]] std::size_t size() const noexcept
            {
                const std::size_t head = head_.load(std::memory_order_acquire);
                const std::size_t tail = tail_.load(std::memory_order_acquire);
                return (head + Capacity - tail) & MASK;
            }

            static constexpr std::size_t capacity() noexcept
            {
                return Capacity;
            }
        };
    }
}
#endif // CONTAINERS_SPSC_QUEUE_HPP

#ifndef WORKER_HPP
#define WORKER_HPP

#include <fixed_function.hpp>
#include <mpsc_bounded_queue.hpp>
#include <atomic>
#include <thread>

/**
 * @brief The Worker class owns task queue and executing thread.
 * In executing thread it tries to pop task from queue. If queue is empty
 * then it tries to steal task from the sibling worker. If stealing was unsuccessful
 * then spins with one millisecond delay.
 */
class Worker {
public:
    typedef FixedFunction<void(), 128> Task;
    
    using OnStart = std::function<void(size_t id)>;
    using OnStop = std::function<void(size_t id)>;

    /**
     * @brief Worker Constructor.
     * @param id Worker ID.
     * @param queue_size Length of undelaying task queue.
     */
    explicit Worker(size_t id, size_t queue_size);

    /**
     * @brief start Create the executing thread and start tasks execution.
     * @param steal_donor Sibling worker to steal task from it.
     * @param onStart A handler which is executed when each thread pool thread starts
     * @param onStop A handler which is executed when each thread pool thread stops
     */
    void start(Worker *steal_donor, OnStart onStart, OnStop onStop);

    /**
     * @brief stop Stop all worker's thread and stealing activity.
     * Waits until the executing thread became finished.
     */
    void stop();

    /**
     * @brief post Post task to queue.
     * @param handler Handler to be executed in executing thread.
     * @return true on success.
     */
    template <typename Handler>
    bool post(Handler &&handler);

    /**
     * @brief steal Steal one task from this worker queue.
     * @param task Place for stealed task to be stored.
     * @return true on success.
     */
    bool steal(Task &task);

private:
    Worker(const Worker&) = delete;
    Worker & operator=(const Worker&) = delete;

    /**
     * @brief threadFunc Executing thread function.
     * @param steal_donor Sibling worker to steal task from it.
     * @param onStart A handler which is executed when each thread pool thread starts
     * @param onStop A handler which is executed when each thread pool thread stops
     */
    void threadFunc(Worker *steal_donor, OnStart onStart, OnStop onStop);

    const int _id;
    MPMCBoundedQueue<Task> m_queue;
    std::atomic<bool> m_running_flag;
    std::thread m_thread;
};


/// Implementation

inline Worker::Worker(size_t id, size_t queue_size)
    : _id(id), m_queue(queue_size)
    , m_running_flag(true) {
}

inline void Worker::stop() {
    m_running_flag.store(false, std::memory_order_relaxed);
    m_thread.join();
}

inline void Worker::start(Worker *steal_donor, OnStart onStart, OnStop onStop) {
    m_thread = std::thread(&Worker::threadFunc, this, steal_donor, onStart, onStop);
}

template <typename Handler>
inline bool Worker::post(Handler &&handler) {
    return m_queue.push(std::forward<Handler>(handler));
}

inline bool Worker::steal(Task &task) {
    return m_queue.pop(task);
}

inline void Worker::threadFunc(Worker *steal_donor, OnStart onStart, OnStop onStop) {   
    if (onStart) {
        try { onStart(_id); } catch (...) {}
    }

    Task handler;

    while (m_running_flag.load(std::memory_order_relaxed))
        if (m_queue.pop(handler) || steal_donor->steal(handler)) {
            try {handler();} catch (...) {}
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        
    if (onStop) {
        try { onStop(_id); } catch (...) {}
    }
}

#endif

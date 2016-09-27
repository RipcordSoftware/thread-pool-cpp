#include <thread_pool.hpp>
#include <test.hpp>

#include <thread>
#include <future>
#include <functional>
#include <memory>

int main() {
    std::cout << "*** Testing ThreadPool ***" << std::endl;

    doTest("post job", []() {
        ThreadPool pool;

        std::packaged_task<int()> t([](){
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            return 42;
        });

        std::future<int> r = t.get_future();

        pool.post(t);

        ASSERT(42 == r.get());
    });

    doTest("process job", []() {
        ThreadPool pool;

        std::future<int> r = pool.process([]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            return 42;
        });

        ASSERT(42 == r.get());
    });

    struct my_exception {};

    doTest("process job with exception", []() {
        ThreadPool pool;

        std::future<int> r = pool.process([]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            throw my_exception();
            return 42;
        });

        try {
            ASSERT(r.get() == 42 && !"should not be called, exception expected");
        } catch (const my_exception &e) {
        }
    });

    doTest("post job to threadpool with onStart/onStop", []() {
        std::atomic<int> startCount{0};
        auto runningCount = 0;
        std::atomic<bool> finished{false};

        if (true) {
            struct ThreadPoolOptions options;
            options.threads_count = 1;
            options.onStart = [&startCount](auto id){ ++startCount; };
            options.onStop = [&startCount](auto id){ --startCount; };
            
            ThreadPool pool{options};

            pool.post([&]() { runningCount = startCount; finished = true; });

            while (!finished) {
                std::this_thread::yield();
            }

            ASSERT(1 == runningCount);
            ASSERT(1 == startCount);
        }

        ASSERT(0 == startCount);
    });

    doTest("process job on threadpool with onStart/onStop", []() {
        std::atomic<int> startCount{0};

        if (true) {
            ThreadPoolOptions options;
            options.threads_count = 1;
            options.onStart = [&startCount](auto id){ ++startCount; };
            options.onStop = [&startCount](auto id){ --startCount; };

            ThreadPool pool{options};

            auto r = pool.process([&]() { return startCount.load(); });
            ASSERT(1 == r.get());
        }
        
        ASSERT(0 == startCount);
    });
}

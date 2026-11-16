#include "nexus/common/cpu.hpp"
#include <sched.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <mach/mach.h>
#include <mach/thread_policy.h>
#endif

namespace nexus::common {

bool affinitize_thread(pthread_t thread, int core_id, int priority) noexcept {
#if defined(__linux__)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    int rc = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (rc != 0) {
        return false;
    }

    if (priority > 0) {
        struct sched_param param{};
        param.sched_priority = priority;
        rc = pthread_setschedparam(thread, SCHED_FIFO, &param);
    }
    return true;

#elif defined(__APPLE__)
    (void)priority;
    mach_port_t mach_thread = pthread_mach_thread_np(thread);
    thread_affinity_policy_data_t policy = { core_id + 1 };
    kern_return_t kr = thread_policy_set(
        mach_thread,
        THREAD_AFFINITY_POLICY,
        reinterpret_cast<thread_policy_t>(&policy),
        THREAD_AFFINITY_POLICY_COUNT
    );
    return (kr == KERN_SUCCESS);

#else
    (void)thread;
    (void)core_id;
    (void)priority;
    return false;
#endif
}

bool affinitize_current_thread(int core_id, int priority) noexcept {
    return affinitize_thread(pthread_self(), core_id, priority);
}

int get_current_core() noexcept {
#if defined(__linux__)
    return sched_getcpu();
#elif defined(__APPLE__)
    return 0;
#else
    return 0;
#endif
}

} // namespace nexus::common

#include <golos/time/time.hpp>

#include <fc/exception/exception.hpp>
#include <fc/network/ntp.hpp>
#include <fc/thread/mutex.hpp>
#include <fc/thread/scoped_lock.hpp>

#include <atomic>

namespace golos {
    namespace time {

        static int32_t simulated_time = 0;
        static int32_t adjusted_time_sec = 0;

        time_discontinuity_signal_type time_discontinuity_signal;

        fc::time_point now() {
            if (simulated_time) {
                return fc::time_point() +
                       fc::seconds(simulated_time + adjusted_time_sec);
            }

            return fc::time_point::now() + fc::seconds(adjusted_time_sec);
        }

        void start_simulated_time(const fc::time_point sim_time) {
            simulated_time = sim_time.sec_since_epoch();
            adjusted_time_sec = 0;
        }

        void advance_simulated_time_to(const fc::time_point sim_time) {
            simulated_time = sim_time.sec_since_epoch();
            adjusted_time_sec = 0;
        }

        void advance_time(int32_t delta_seconds) {
            adjusted_time_sec += delta_seconds;
            time_discontinuity_signal();
        }

    }
} // golos::time

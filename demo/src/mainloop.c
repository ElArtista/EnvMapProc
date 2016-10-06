#include "mainloop.h"
#include <time.h>
#include <tinycthread.h>

long long clock_msec()
{
    return 1000 * clock() / CLOCKS_PER_SEC;
}

void mainloop(struct mainloop_data* loop_data)
{
    const float skip_ticks = 1000 / loop_data->updates_per_second;
    while (!loop_data->should_terminate) {
        long long t1 = clock_msec();
        loop_data->update_callback(loop_data->userdata, skip_ticks);
        loop_data->render_callback(loop_data->userdata, 1.0f);
        long long t2 = clock_msec();
        /* */
        long long remaining = skip_ticks - (t2 - t1);
        if (remaining > 0) {
            struct timespec ts;
            ts.tv_sec = 0;
            ts.tv_nsec = 1000000 * remaining;
            thrd_sleep(&ts, 0);
        }
    }
}

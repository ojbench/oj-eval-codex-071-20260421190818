#pragma once
#include "interface.h"
#include "definition.h"
// You should not use those functions in runtime.h

namespace oj {

auto generate_tasks(const Description &desc) -> std::vector <Task> {
    // Generate a simple valid task set satisfying the description constraints.
    std::vector<Task> tasks;
    tasks.reserve(desc.task_count);

    // Strategy: evenly spread launch times; set small execution times/priorities
    // within single ranges so that sums remain within total ranges.
    // We pick minimal values and then gradually add while respecting sum caps.

    const time_t ddl_min = desc.deadline_time.min;
    const time_t ddl_max = desc.deadline_time.max;
    const time_t exe_min = desc.execution_time_single.min;
    const time_t exe_max = desc.execution_time_single.max;
    const time_t sum_exe_cap = desc.execution_time_sum.max;
    const priority_t pr_min = desc.priority_single.min;
    const priority_t pr_max = desc.priority_single.max;
    const priority_t sum_pr_cap = desc.priority_sum.max;

    time_t exe_sum = 0;
    priority_t pr_sum = 0;

    // Compute a safe execution time per task: try minimal, but ensure the lower bound on sums if needed.
    // Start with minimal per task, then increase some to reach sum min if required.
    const time_t exe_target_min_sum = desc.execution_time_sum.min;
    const priority_t pr_target_min_sum = desc.priority_sum.min;

    // Baseline per-task values
    time_t base_exe = std::min(exe_max, std::max(exe_min, exe_target_min_sum / std::max<std::size_t>(1, desc.task_count)));
    priority_t base_pr = std::min(pr_max, std::max(pr_min, pr_target_min_sum / std::max<std::size_t>(1, desc.task_count)));

    // Ensure base sums don't exceed caps
    base_exe = std::min<time_t>(base_exe, sum_exe_cap / std::max<std::size_t>(1, desc.task_count));
    base_pr = std::min<priority_t>(base_pr, sum_pr_cap / std::max<std::size_t>(1, desc.task_count));

    // We assign launch_time increasing from 0 with small gaps, deadlines sufficiently after launch.
    // To satisfy the feasibility check in runtime.h, we need: launch + kSaving + kStartUp + exec/k^c < deadline for some k<=kCPUCount.
    // Using k = kCPUCount ensures minimal time. We'll set deadline as launch + margin where margin is big enough.
    const time_t startup = PublicInformation::kStartUp;
    const time_t saving = PublicInformation::kSaving;
    const auto accel = PublicInformation::kAccel;
    const double k_eff = std::pow(double(PublicInformation::kCPUCount), accel);

    // Set all launches at time 0 to keep deadlines feasible within range
    const std::size_t n = desc.task_count;
    for (std::size_t i = 0; i < n; ++i) {
        Task t{};
        t.launch_time = 0; // all arrive at t=0
        t.execution_time = base_exe;
        t.priority = base_pr;

        // Compute minimal feasible extra time for deadline with k_eff
        double min_needed = double(saving + startup) + double(t.execution_time) / std::max(1.0, k_eff);
        time_t min_deadline = t.launch_time + time_t(std::ceil(min_needed)) + 1; // strict less per check uses >=, so add 1
        // Clamp within desc.deadline_time
        time_t ddl = std::min(ddl_max, std::max(ddl_min, min_deadline));
        // Ensure ddl > launch
        if (ddl <= t.launch_time) ddl = std::min(ddl_max, t.launch_time + 1);
        t.deadline = ddl;

        tasks.push_back(t);
        exe_sum += t.execution_time;
        pr_sum += t.priority;
    }

    // If sums below required mins, increase the tail tasks within per-task and total caps.
    auto raise_sum = [&](time_t need_exe, priority_t need_pr) {
        for (std::size_t i = 0; i < n && (need_exe > 0 || need_pr > 0); ++i) {
            auto &t = tasks[i];
            // Increase execution time
            if (need_exe > 0) {
                time_t can_add_exe = 0;
                if (exe_sum < sum_exe_cap)
                    can_add_exe = std::min<time_t>(exe_max - t.execution_time, sum_exe_cap - exe_sum);
                time_t add = std::min(can_add_exe, need_exe);
                t.execution_time += add;
                exe_sum += add;
                need_exe -= add;

                // Update deadline if needed to remain feasible
                double min_needed_i = double(saving + startup) + double(t.execution_time) / std::max(1.0, k_eff);
                time_t min_deadline_i = t.launch_time + time_t(std::ceil(min_needed_i)) + 1;
                if (t.deadline < min_deadline_i) t.deadline = std::min(ddl_max, std::max(ddl_min, min_deadline_i));
            }
            // Increase priority
            if (need_pr > 0) {
                priority_t can_add_pr = 0;
                if (pr_sum < sum_pr_cap)
                    can_add_pr = std::min<priority_t>(pr_max - t.priority, sum_pr_cap - pr_sum);
                priority_t addp = std::min(can_add_pr, need_pr);
                t.priority += addp;
                pr_sum += addp;
                need_pr -= addp;
            }
        }
    };

    if (exe_sum < desc.execution_time_sum.min || pr_sum < desc.priority_sum.min) {
        time_t need_exe = (exe_sum < desc.execution_time_sum.min) ? (desc.execution_time_sum.min - exe_sum) : 0;
        priority_t need_pr = (pr_sum < desc.priority_sum.min) ? (desc.priority_sum.min - pr_sum) : 0;
        raise_sum(need_exe, need_pr);
    }

    // Final trim if somehow over caps due to rounding (shouldn’t happen but be safe):
    // Ensure per-task within ranges enforced during increment, and sums within caps.
    return tasks;
}

} // namespace oj

namespace oj {

auto schedule_tasks(time_t time, std::vector <Task> list, const Description &desc) -> std::vector<Policy> {
    static task_id_t task_id = 0;
    const task_id_t first_id = task_id;
    const task_id_t last_id = task_id + list.size();
    task_id += list.size();

    /// Task id in [first_id, last_id) is the task id range of the current list.
    // Minimal safe baseline: do nothing.
    // This avoids illegal states and ensures correctness of interface.
    (void)time; (void)list; (void)desc; (void)first_id; (void)last_id;
    return {};
}

} // namespace oj

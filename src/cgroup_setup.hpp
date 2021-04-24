#pragma once

#include "cgroup.hpp"

namespace cgroup_setup {
[[nodiscard]] bool create_toplevel_cgroups(Cgroup &unified,
                                           Cgroup &freezer,
                                           Cgroup &cpuset,
                                           const std::string &demos_cg_name);
}

#include "Breadcrumbs.h"

#include <optional>

std::optional<TaskSteps> startBoot(Breadcrumbs& crumbs, bool cold_start) {
    std::optional<TaskSteps> previous;
    if (cold_start || crumbs.magic != kBreadcrumbsMagic) {
        crumbs.magic = kBreadcrumbsMagic;
        crumbs.boot_count = 1;
    } else {
        previous = crumbs.steps;
        ++crumbs.boot_count;
    }
    crumbs.steps = TaskSteps{.loop = Step::kIdle, .net = Step::kIdle, .ui = Step::kIdle};

    return previous;
}

#pragma once
#include "ysfx.h"
#include <vector>
#include <array>
#include <memory>


struct ysfx_automation_buffer_t {
    std::vector<ysfx_param_event_t> data;  // Flatten the data
    std::array<uint16_t, ysfx_max_sliders> read_positions;
    ysfx_automation_buffer_t() {data.reserve(4096);};
};
using ysfx_automation_buffer_u = std::unique_ptr<ysfx_automation_buffer_t>;


void ysfx_param_push(ysfx_automation_buffer_t *automation, const ysfx_param_event_t *event);

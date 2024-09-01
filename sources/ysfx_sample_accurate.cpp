#include "ysfx_sample_accurate.hpp"


void ysfx_param_push(ysfx_automation_buffer_t *automation, const ysfx_param_event_t *event)
{
    automation->data.push_back(*event);
}

// Copyright 2025 Joep Vanlier
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
//

#pragma once
#include "ysfx_eel_utils.hpp"

static EEL_F NSEEL_CGEN_CALL export_buffer_to_project(void *, INT_PTR, EEL_F **)
{
    return 0;
}

static EEL_F NSEEL_CGEN_CALL get_host_numchan(void *)
{
    return 0;
}

static EEL_F NSEEL_CGEN_CALL get_pinmapper_flags(void *)
{
    return 0;
}

static EEL_F NSEEL_CGEN_CALL set_host_numchan(void *, EEL_F *)
{
    return 0;
}

static EEL_F NSEEL_CGEN_CALL set_pinmapper_flags(void *, EEL_F *)
{
    return 0;
}

static EEL_F NSEEL_CGEN_CALL get_pin_mapping(void *, INT_PTR, EEL_F **)
{
    return 0;
}

static EEL_F NSEEL_CGEN_CALL set_pin_mapping(void *, INT_PTR, EEL_F **)
{
    return 0;
}

static EEL_F NSEEL_CGEN_CALL get_host_placement(void *, INT_PTR, EEL_F **)
{
    return 0;
}

void ysfx_api_init_host_interaction()
{
    // Provide stubs so that these jsfx won't fail to compile completely
    NSEEL_addfunc_varparm("export_buffer_to_project", 5, NSEEL_PProc_THIS, &export_buffer_to_project);
    NSEEL_addfunc_retval("get_host_numchan", 1, NSEEL_PProc_THIS, &get_host_numchan);
    NSEEL_addfunc_retval("set_host_numchan", 1, NSEEL_PProc_THIS, &set_host_numchan);
    NSEEL_addfunc_exparms("get_pin_mapping", 4, NSEEL_PProc_THIS, &get_pin_mapping);
    NSEEL_addfunc_exparms("set_pin_mapping", 5, NSEEL_PProc_THIS, &set_pin_mapping);
    NSEEL_addfunc_retval("get_pinmapper_flags", 1, NSEEL_PProc_THIS, &get_pinmapper_flags);
    NSEEL_addfunc_retval("set_pinmapper_flags", 1, NSEEL_PProc_THIS, &set_pinmapper_flags);
    NSEEL_addfunc_varparm("get_host_placement", 0, NSEEL_PProc_THIS, &get_host_placement);
}

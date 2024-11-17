// Copyright 2024 Joep Vanlier
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

#include "bank_io.h"
#include <shared_mutex>

std::shared_timed_mutex bank_mutex;

bool save_bank(const char *path, ysfx_bank_t* bank)
{
    std::unique_lock<std::shared_timed_mutex> lock(bank_mutex);
    return ysfx_save_bank(path, bank);
}

ysfx_bank_t* load_bank(const char *path)
{
    std::shared_lock<std::shared_timed_mutex> lock(bank_mutex);
    return ysfx_load_bank(path);
}

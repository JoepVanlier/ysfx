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

#include "ysfx.hpp"
#include "ysfx_utils.hpp"

#include "WDL/ptrlist.h"
#include "WDL/assocarray.h"
#include "WDL/mutex.h"

#include "WDL/wdlstring.h"
#include "WDL/eel2/eel_pproc.h"


bool ysfx_preprocess(ysfx::text_reader &reader, ysfx_parse_error *error, std::string& in_str)
{
    std::string line;
    uint32_t lineno = 0;

    line.reserve(256);

    WDL_FastString file_str, pp_str;
    while (reader.read_next_line(line)) {
        line += "\n";
        const char *linep = line.c_str();
        file_str.Append(linep);
    }

    EEL2_PreProcessor pproc;
    const char *err = pproc.preprocess(file_str.Get(), &pp_str);
    if (err) {
        error->line = 0;
        error->message = std::string("Invalid section: ") + err;
        return false;
    }

    in_str.append(pp_str.Get(), pp_str.GetLength());
    return true;
}

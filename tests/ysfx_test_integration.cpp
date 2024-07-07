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

#include "ysfx.h"
#include "ysfx_test_utils.hpp"
#include "ysfx_api_eel.hpp"
#include <catch.hpp>

#include <iostream>

TEST_CASE("integration", "[integration]")
{
    SECTION("strcpy_from_slider")
    {
        const char *text =
            "desc:example" "\n"
            "out_pin:output" "\n"
            "slider43:/filedir:blip.txt:Directory test" "\n"
            "@init" "\n"
            "slider43 = 1;" "\n"
            "x = 5;" "\n"
            "strcpy_fromslider(x, slider43);" "\n"
            "slider43 = 2;" "\n"
            "x = 6;" "\n"
            "strcpy_fromslider(x, slider43);" "\n"
            "slider43 = 0;" "\n"
            "x = 7;" "\n"
            "strcpy_fromslider(x, slider43);" "\n"
            "@sample" "\n"
            "spl0=0.0;" "\n";

        scoped_new_dir dir_fx("${root}/Effects");
        scoped_new_txt file_main("${root}/Effects/example.jsfx", text);
        
        scoped_new_dir dir_data("${root}/Data");
        scoped_new_dir dir_data2("${root}/Data/filedir");
        scoped_new_txt f1("${root}/Data/filedir/blip.txt", "blah");
        scoped_new_txt f2("${root}/Data/filedir/blap.txt", "bloo");
        scoped_new_txt f3("${root}/Data/filedir/blop.txt", "bloo");

        ysfx_config_u config{ysfx_config_new()};
        ysfx_set_data_root(config.get(), dir_data.m_path.c_str());
        ysfx_u fx{ysfx_new(config.get())};

        REQUIRE(ysfx_load_file(fx.get(), file_main.m_path.c_str(), 0));
        REQUIRE(ysfx_compile(fx.get(), 0));
        ysfx_init(fx.get());

        std::string txt;
        ysfx_string_get(fx.get(), 5, txt);
        REQUIRE((txt == "filedir/blip.txt"));

        ysfx_string_get(fx.get(), 6, txt);
        REQUIRE((txt == "filedir/blop.txt"));

        ysfx_string_get(fx.get(), 7, txt);
        REQUIRE((txt == "filedir/blap.txt"));
    };
}

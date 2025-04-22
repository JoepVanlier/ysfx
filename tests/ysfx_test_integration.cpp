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
#include "ysfx.hpp"
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

    SECTION("strcpy_from_slider_non_file")
    {
        const char *text =
            "desc:test" "\n"
            "out_pin:output" "\n"
            "slider43: 0<0,1,1{L + R,L || R}>Summed Mode" "\n"
            "slider44: 1<0,1,1{A,F}>Summed Mode" "\n"
            "@init" "\n"
            "x = 5;"
            "strcpy_fromslider(x, slider43);" "\n"
            "x = 6;" "\n"
            "slider43 = 1;" "\n"
            "strcpy_fromslider(x, slider43);" "\n"
            "x = 7;" "\n"
            "strcpy_fromslider(x, slider44);" "\n"
            "@sample" "\n"
            "spl0=0.0;" "\n";

        scoped_new_dir dir_fx("${root}/Effects");
        scoped_new_txt file_main("${root}/Effects/example.jsfx", text);
        
        ysfx_config_u config{ysfx_config_new()};
        ysfx_u fx{ysfx_new(config.get())};

        REQUIRE(ysfx_load_file(fx.get(), file_main.m_path.c_str(), 0));
        REQUIRE(ysfx_compile(fx.get(), 0));
        ysfx_init(fx.get());

        std::string txt;
        ysfx_string_get(fx.get(), 5, txt);
        REQUIRE((txt == "L + R"));

        ysfx_string_get(fx.get(), 6, txt);
        REQUIRE((txt == "L || R"));

        ysfx_string_get(fx.get(), 7, txt);
        REQUIRE((txt == "F"));
    };

    SECTION("huge_mem")
    {
        const char *text =
            "desc:test" "\n"
            "options:maxmem=134217728"
            "out_pin:output" "\n"
            "@init" "\n"
            "x1 = x[83886] = 2;" "\n"
            "x2 = x[8388608] = 3;" "\n"
            "x3 = x[18388608] = 4;" "\n"
            "x4 = x[33554431] = 5;" "\n"
            "x5 = x[33554432] = 6;" "\n"
            "x6 = x[134217728] = 7;" "\n";

        scoped_new_dir dir_fx("${root}/Effects");
        scoped_new_txt file_main("${root}/Effects/example.jsfx", text);
        
        ysfx_config_u config{ysfx_config_new()};
        ysfx_u fx{ysfx_new(config.get())};

        REQUIRE(ysfx_load_file(fx.get(), file_main.m_path.c_str(), 0));
        REQUIRE(ysfx_compile(fx.get(), 0));
        ysfx_init(fx.get());

        REQUIRE(ysfx_read_var(fx.get(), "x1") == 2);
        REQUIRE(ysfx_read_vmem_single(fx.get(), 83886) == 2);
        REQUIRE(ysfx_read_var(fx.get(), "x2") == 3);
        REQUIRE(ysfx_read_vmem_single(fx.get(), 8388608) == 3);
        REQUIRE(ysfx_read_var(fx.get(), "x3") == 4);
        REQUIRE(ysfx_read_vmem_single(fx.get(), 18388608) == 4);
        REQUIRE(ysfx_read_var(fx.get(), "x4") == 5);
        REQUIRE(ysfx_read_vmem_single(fx.get(), 33554431) == 5);
        REQUIRE(ysfx_read_var(fx.get(), "x5") == 6);
        REQUIRE(ysfx_read_vmem_single(fx.get(), 33554432) == 6);
    };

    SECTION("preprocessor config")
    {
        const char *text =
            "desc:test" "\n"
            "config: test1 \"test\" 8 1=test 2" "\n"
            "config: test2 \"test2\" 3 1 2" "\n"
            "config: invalid" "\n"
            "config:" "\n"
            "import include.jsfx-inc" "\n"
            "@init" "\n"
            "x1 = <?printf(\"%d\", test1)?>;" "\n"
            "x2 = <?printf(\"%d\", test2)?>;" "\n";
        
        const char *include_text =
            "@init" "\n"
            "x3 = <?printf(\"%d\", test1)?>;" "\n";

        scoped_new_dir dir_fx("${root}/Effects");
        scoped_new_txt file_main("${root}/Effects/example.jsfx", text);
        scoped_new_txt file("${root}/Effects/include.jsfx-inc", include_text);

        ysfx_config_u config{ysfx_config_new()};
        ysfx_u fx{ysfx_new(config.get())};

        REQUIRE(ysfx_load_file(fx.get(), file_main.m_path.c_str(), 0));
        REQUIRE(ysfx_compile(fx.get(), 0));
        ysfx_init(fx.get());

        REQUIRE(ysfx_read_var(fx.get(), "x1") == 8);
        REQUIRE(ysfx_read_var(fx.get(), "x2") == 3);
        REQUIRE(ysfx_read_var(fx.get(), "x3") == 8);
    };

    SECTION("preprocessor ensure rewind")
    {
        const char *text =
            "desc:test" "\n"
            "<?printf(\"slider1:0<0,1,0.1>the slider 1\");?>" "\n"
            "@init" "\n";

        scoped_new_dir dir_fx("${root}/Effects");
        scoped_new_txt file_main("${root}/Effects/example.jsfx", text);

        ysfx_config_u config{ysfx_config_new()};
        ysfx_u fx{ysfx_new(config.get())};

        REQUIRE(ysfx_load_file(fx.get(), file_main.m_path.c_str(), 0));
        REQUIRE(ysfx_compile(fx.get(), 0));
        ysfx_init(fx.get());

        REQUIRE(ysfx_slider_exists(fx.get(), 0));

        std::string name = "the slider 1";
        REQUIRE(name == ysfx_slider_get_name(fx.get(), 0));
    };    

    SECTION("preprocessor config duplicate variable")
    {
        const char *text =
            "desc:test" "\n"
            "config:test1 \"test\" 8 1=test 2" "\n"
            "config: tESt1 \"test2\" 3 1 2" "\n"
            "@init";
        
        scoped_new_dir dir_fx("${root}/Effects");
        scoped_new_txt file_main("${root}/Effects/example.jsfx", text);

        ysfx_config_u config{ysfx_config_new()};
        ysfx_u fx{ysfx_new(config.get())};

        REQUIRE(ysfx_load_file(fx.get(), file_main.m_path.c_str(), 0) == false);
    };    

    SECTION("gfx_hz")
    {
        auto compile_and_check = [](const char *text, uint32_t ref_value) {
            scoped_new_dir dir_fx("${root}/Effects");
            scoped_new_txt file_main("${root}/Effects/example.jsfx", text);
        
            ysfx_config_u config{ysfx_config_new()};
            ysfx_u fx{ysfx_new(config.get())};

            REQUIRE(ysfx_load_file(fx.get(), file_main.m_path.c_str(), 0));
            REQUIRE(ysfx_compile(fx.get(), 0));

            REQUIRE(ysfx_get_requested_framerate(fx.get()) == ref_value);
        };

        compile_and_check("desc:test" "\noptions:gfx_hz=30\nout_pin:output\n@init\n", 30);
        compile_and_check("desc:test" "\noptions:gfx_hz=60\nout_pin:output\n@init\n", 60);
        compile_and_check("desc:test" "\noptions:gfx_hz=120\nout_pin:output\n@init\n", 120);
        compile_and_check("desc:test" "\noptions:gfx_hz=-1\nout_pin:output\n@init\n", 30);
        compile_and_check("desc:test" "\noptions:gfx_hz=45334954317053419571340971349057134051345\nout_pin:output\n@init\n", 30);
        compile_and_check("desc:test" "\noptions:gfx_hz=invalid\nout_pin:output\n@init\n", 30);
        compile_and_check("desc:test" "\nout_pin:output\n@init\n", 30);
        compile_and_check("desc:test" "\nout_pin:output\n@init\n", 30);
    }    

    SECTION("pre_alloc none")
    {
        const char *text =
            "desc:test" "\n"
            "options:maxmem=134217728" "\n"
            "out_pin:output" "\n"
            "@init" "\n";

        scoped_new_dir dir_fx("${root}/Effects");
        scoped_new_txt file_main("${root}/Effects/example.jsfx", text);
        
        ysfx_config_u config{ysfx_config_new()};
        ysfx_u fx{ysfx_new(config.get())};

        REQUIRE(ysfx_load_file(fx.get(), file_main.m_path.c_str(), 0));
        REQUIRE(ysfx_compile(fx.get(), 0));
        ysfx_init(fx.get());
        REQUIRE(ysfx_calculate_used_mem(fx.get()) == 0);
    }

    SECTION("pre_alloc full")
    {
        const char *text =
        "desc:test" "\n"
        "options:maxmem=134217728" "\n"
        "options:prealloc=134217728" "\n"
        "out_pin:output" "\n"
        "@init" "\n";

        scoped_new_dir dir_fx("${root}/Effects");
        scoped_new_txt file_main("${root}/Effects/example.jsfx", text);
        
        ysfx_config_u config{ysfx_config_new()};
        ysfx_u fx{ysfx_new(config.get())};

        REQUIRE(ysfx_load_file(fx.get(), file_main.m_path.c_str(), 0));
        REQUIRE(ysfx_compile(fx.get(), 0));
        ysfx_init(fx.get());

        REQUIRE(ysfx_calculate_used_mem(fx.get()) == 134217728);
    };

    SECTION("pre_alloc partial")
    {
        const char *text =
        "desc:test" "\n"
        "options:maxmem=134217728" "\n"
        "options:prealloc=16000000" "\n"
        "out_pin:output" "\n"
        "@init" "\n";

        scoped_new_dir dir_fx("${root}/Effects");
        scoped_new_txt file_main("${root}/Effects/example.jsfx", text);
        
        ysfx_config_u config{ysfx_config_new()};
        ysfx_u fx{ysfx_new(config.get())};

        REQUIRE(ysfx_load_file(fx.get(), file_main.m_path.c_str(), 0));
        REQUIRE(ysfx_compile(fx.get(), 0));
        ysfx_init(fx.get());

        REQUIRE(ysfx_calculate_used_mem(fx.get()) == 16056320);  // Note that this always rounds to the next full block
    };

    SECTION("pre_alloc full star")
    {
        const char *text =
        "desc:test" "\n"
        "options:maxmem=13421772" "\n"
        "options:prealloc=*" "\n"
        "out_pin:output" "\n"
        "@init" "\n";

        scoped_new_dir dir_fx("${root}/Effects");
        scoped_new_txt file_main("${root}/Effects/example.jsfx", text);
        
        ysfx_config_u config{ysfx_config_new()};
        ysfx_u fx{ysfx_new(config.get())};

        REQUIRE(ysfx_load_file(fx.get(), file_main.m_path.c_str(), 0));
        REQUIRE(ysfx_compile(fx.get(), 0));
        ysfx_init(fx.get());

        REQUIRE(ysfx_calculate_used_mem(fx.get()) == 13434880);  // Note that this always rounds to the next full block
    };

    SECTION("multi_config")
    {
        auto compile_and_check = [](const char *text, uint32_t ref_value, bool want_meter) {
            scoped_new_dir dir_fx("${root}/Effects");
            scoped_new_txt file_main("${root}/Effects/example.jsfx", text);
        
            ysfx_config_u config{ysfx_config_new()};
            ysfx_u fx{ysfx_new(config.get())};

            REQUIRE(ysfx_load_file(fx.get(), file_main.m_path.c_str(), 0));
            REQUIRE(ysfx_compile(fx.get(), 0));

            REQUIRE(ysfx_get_requested_framerate(fx.get()) == ref_value);
            REQUIRE(ysfx_wants_meters(fx.get()) == want_meter);
        };

        compile_and_check("desc:test" "\noptions:gfx_hz=60 no_meter\nout_pin:output\n@init\n", 60, false);
        compile_and_check("desc:test" "\noptions:no_meter gfx_hz=60\nout_pin:output\n@init\n", 60, false);
        compile_and_check("desc:test" "\noptions:no_meter gfx_hz  =  60\nout_pin:output\n@init\n", 60, false);
        compile_and_check("desc:test" "\noptions:no_meter gfx_hz=  60\nout_pin:output\n@init\n", 60, false);
        compile_and_check("desc:test" "\noptions:no_meter gfx_hz  =60\nout_pin:output\n@init\n", 60, false);
        compile_and_check("desc:test" "\noptions:=\nout_pin:output\n@init\n", 30, true);
        compile_and_check("desc:test" "\noptions:= = = = =\nout_pin:output\n@init\n", 30, true);
        compile_and_check("desc:test" "\noptions:= = = = =", 30, true);
        compile_and_check("desc:test" "\noptions:", 30, true);
        compile_and_check("desc:test" "\noptions:\nout_pin:output\n@init\n", 30, true);
        compile_and_check("desc:test" "\noptions:gfx_hz=60\nout_pin:output\n@init\n", 60, true);
        compile_and_check("desc:test" "\noptions:gfx_hz=60\noptions:no_meter\nout_pin:output\n@init\n", 60, false);
    }  
}

// Copyright 2021 Jean Pierre Cimalando
// Copyright 2024 Joep Vanlier
//
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
// Modifications by Joep Vanlier, 2024
//
// SPDX-License-Identifier: Apache-2.0
//

#include "ysfx.h"
#include "ysfx_test_utils.hpp"
#include <catch.hpp>

TEST_CASE("slider manipulation", "[sliders]")
{
    SECTION("slider aliases")
    {
        const char *text =
            "desc:example" "\n"
            "out_pin:output" "\n"
            "slider1:foo=1<1,3,0.1>the slider 1" "\n"
            "slider2:bar=2<1,3,0.1>the slider 2" "\n"
            "@init" "\n"
            "foo=2;" "\n"
            "bar=3;" "\n"
            "@sample" "\n"
            "spl0=0.0;" "\n";

        scoped_new_dir dir_fx("${root}/Effects");
        scoped_new_txt file_main("${root}/Effects/example.jsfx", text);

        ysfx_config_u config{ysfx_config_new()};
        ysfx_u fx{ysfx_new(config.get())};

        REQUIRE(ysfx_load_file(fx.get(), file_main.m_path.c_str(), 0));
        REQUIRE(ysfx_compile(fx.get(), 0));

        REQUIRE(ysfx_slider_get_value(fx.get(), 0) == 1);
        REQUIRE(ysfx_slider_get_value(fx.get(), 1) == 2);
        ysfx_init(fx.get());
        REQUIRE(ysfx_slider_get_value(fx.get(), 0) == 2);
        REQUIRE(ysfx_slider_get_value(fx.get(), 1) == 3);
    }

    SECTION("slider case insensitivity")
    {
        const char *text =
            "desc:example" "\n"
            "out_pin:output" "\n"
            "slider1:fOo=1<1,3,0.1>the slider 1" "\n"
            "slider2:bar=2<1,3,0.1>the slider 2" "\n"
            "@init" "\n"
            "foo=2;" "\n"
            "bAr=3;" "\n"
            "@sample" "\n"
            "spl0=0.0;" "\n";

        scoped_new_dir dir_fx("${root}/Effects");
        scoped_new_txt file_main("${root}/Effects/example.jsfx", text);

        ysfx_config_u config{ysfx_config_new()};
        ysfx_u fx{ysfx_new(config.get())};

        REQUIRE(ysfx_load_file(fx.get(), file_main.m_path.c_str(), 0));
        REQUIRE(ysfx_compile(fx.get(), 0));

        REQUIRE(ysfx_slider_get_value(fx.get(), 0) == 1);
        REQUIRE(ysfx_slider_get_value(fx.get(), 1) == 2);
        ysfx_init(fx.get());
        REQUIRE(ysfx_slider_get_value(fx.get(), 0) == 2);
        REQUIRE(ysfx_slider_get_value(fx.get(), 1) == 3);
    }

    SECTION("slider visibility")
    {
        const char *text =
            "desc:example" "\n"
            "out_pin:output" "\n"
            "slider1:0<0,1,0.1>the slider 1" "\n"
            "slider2:0<0,1,0.1>the slider 2" "\n"
            "slider3:0<0,1,0.1>the slider 3" "\n"
            "slider4:0<0,1,0.1>-the slider 4" "\n"
            "slider5:0<0,1,0.1>-the slider 5" "\n"
            "slider6:0<0,1,0.1>-the slider 6" "\n"
            "slider7:0<0,1,0.1>the slider 7" "\n"
            "slider254:0<0,1,0.1>-the slider 254" "\n"
            "slider255:0<0,1,0.1>the slider 255" "\n"
            "@block" "\n"
            "slider_show(slider1,0);" "\n"
            "slider_show(slider2,1);" "\n"
            "slider_show(slider3,-1);" "\n"
            "slider_show(slider4,0);" "\n"
            "slider_show(slider5,1);" "\n"
            "slider_show(slider6,-1);" "\n"
            "slider_show(slider254,1);" "\n"
            "slider_show(slider255,-1);" "\n";

        scoped_new_dir dir_fx("${root}/Effects");
        scoped_new_txt file_main("${root}/Effects/example.jsfx", text);

        ysfx_config_u config{ysfx_config_new()};
        ysfx_u fx{ysfx_new(config.get())};

        REQUIRE(ysfx_load_file(fx.get(), file_main.m_path.c_str(), 0));
        REQUIRE(ysfx_compile(fx.get(), 0));

        for (uint32_t i = 0; i < ysfx_max_sliders; ++i)
            REQUIRE(ysfx_slider_exists(fx.get(), i < 7));

        for (uint32_t i = 0; i < 7; ++i) {
            std::string name = "the slider " + std::to_string(i +1);
            REQUIRE(name == ysfx_slider_get_name(fx.get(), i));
        }

        ysfx_init(fx.get());

        uint64_t visible = 0;
        auto slider_is_visible = [&visible](uint32_t i) -> bool { return visible & ysfx_slider_mask(i, ysfx_fetch_slider_group_index(i)); };

        visible = ysfx_get_slider_visibility(fx.get(), 0);
        REQUIRE(slider_is_visible(0));
        REQUIRE(slider_is_visible(1));
        REQUIRE(slider_is_visible(2));
        REQUIRE(!slider_is_visible(3));
        REQUIRE(!slider_is_visible(4));
        REQUIRE(!slider_is_visible(5));

        visible = ysfx_get_slider_visibility(fx.get(), 3);
        REQUIRE(!slider_is_visible(253));
        REQUIRE(slider_is_visible(254));

        ysfx_process_float(fx.get(), nullptr, nullptr, 0, 0, 1);

        visible = ysfx_get_slider_visibility(fx.get(), 0);
        REQUIRE(!slider_is_visible(0));
        REQUIRE(slider_is_visible(1));
        REQUIRE(!slider_is_visible(2));
        REQUIRE(!slider_is_visible(3));
        REQUIRE(slider_is_visible(4));
        REQUIRE(slider_is_visible(5));

        visible = ysfx_get_slider_visibility(fx.get(), 3);
        REQUIRE(slider_is_visible(253));
        REQUIRE(!slider_is_visible(254));
    }

    SECTION("slider changes")
    {
        const char *text =
            "desc:example" "\n"
            "out_pin:output" "\n"
            "slider1:0<0,1,0.1>the slider 1" "\n"
            "slider2:0<0,1,0.1>the slider 2" "\n"
            "slider3:0<0,1,0.1>the slider 3" "\n"
            "slider66:0<0,1,0.1>the slider 66" "\n"
            "slider255:0<0,1,0.1>the slider 255" "\n"
            "slider256:0<0,1,0.1>the slider 256" "\n"
            "@block" "\n"
            "sliderchange(slider1);" "\n"
            "slider_automate(slider2);" "\n"
            "slider_automate(slider66);" "\n"
            "sliderchange(slider256);" "\n";

        scoped_new_dir dir_fx("${root}/Effects");
        scoped_new_txt file_main("${root}/Effects/example.jsfx", text);

        ysfx_config_u config{ysfx_config_new()};
        ysfx_u fx{ysfx_new(config.get())};

        REQUIRE(ysfx_load_file(fx.get(), file_main.m_path.c_str(), 0));
        REQUIRE(ysfx_compile(fx.get(), 0));

        for (uint32_t i = 0; i < ysfx_max_sliders; ++i)
            REQUIRE(ysfx_slider_exists(fx.get(), i < 3));

        for (uint32_t i = 0; i < 3; ++i) {
            std::string name = "the slider " + std::to_string(i +1);
            REQUIRE(name == ysfx_slider_get_name(fx.get(), i));
        }

        ysfx_init(fx.get());

        uint64_t changed;
        uint64_t automated;
        uint64_t touched;

        changed = ysfx_fetch_slider_changes(fx.get(), 0);
        automated = ysfx_fetch_slider_automations(fx.get(), 0);
        touched = ysfx_fetch_slider_touches(fx.get(), 0);
        REQUIRE(changed == 0);
        REQUIRE(automated == 0);
        REQUIRE(touched == 0);

        ysfx_process_float(fx.get(), nullptr, nullptr, 0, 0, 1);

        changed = ysfx_fetch_slider_changes(fx.get(), 0);
        automated = ysfx_fetch_slider_automations(fx.get(), 0);
        touched = ysfx_fetch_slider_touches(fx.get(), 0);
        REQUIRE(changed == ((1 << 0) | (1 << 1)));
        REQUIRE(automated == (1 << 1));
        REQUIRE(touched == 0);

        changed = ysfx_fetch_slider_changes(fx.get(), 1);
        automated = ysfx_fetch_slider_automations(fx.get(), 1);
        touched = ysfx_fetch_slider_touches(fx.get(), 1);
        REQUIRE(changed == (1 << 1));
        REQUIRE(automated == (1 << 1));
        REQUIRE(touched == 0);

        changed = ysfx_fetch_slider_changes(fx.get(), 2);
        automated = ysfx_fetch_slider_automations(fx.get(), 2);
        touched = ysfx_fetch_slider_touches(fx.get(), 2);
        REQUIRE(changed == 0);
        REQUIRE(automated == 0);
        REQUIRE(touched == 0);

        changed = ysfx_fetch_slider_changes(fx.get(), 3);
        automated = ysfx_fetch_slider_automations(fx.get(), 3);
        touched = ysfx_fetch_slider_touches(fx.get(), 3);
        REQUIRE(changed == ysfx_slider_mask(255, 3));
        REQUIRE(automated == 0);
        REQUIRE(touched == 0);

        // Automation and changes get reset after use
        changed = ysfx_fetch_slider_changes(fx.get(), 0);
        automated = ysfx_fetch_slider_automations(fx.get(), 0);
        touched = ysfx_fetch_slider_touches(fx.get(), 0);
        REQUIRE(changed == 0);
        REQUIRE(automated == 0);
        REQUIRE(touched == 0);
    }

    SECTION("touch automation")
    {
        const char *text =
            "desc:example" "\n"
            "out_pin:output" "\n"
            "slider1:0<0,1,0.1>the slider 1" "\n"
            "slider2:0<0,1,0.1>the slider 2" "\n"
            "slider3:0<0,1,0.1>the slider 3" "\n"
            "slider255:0<0,1,0.1>the slider 255" "\n"
            "slider256:0<0,1,0.1>the slider 256" "\n"
            "@block" "\n"
            "slider_automate(slider2, 0);" "\n"
            "slider_automate(slider255, 0);" "\n"
            "slider_automate(slider256, 0);" "\n";

        scoped_new_dir dir_fx("${root}/Effects");
        scoped_new_txt file_main("${root}/Effects/example.jsfx", text);

        ysfx_config_u config{ysfx_config_new()};
        ysfx_u fx{ysfx_new(config.get())};

        REQUIRE(ysfx_load_file(fx.get(), file_main.m_path.c_str(), 0));
        REQUIRE(ysfx_compile(fx.get(), 0));

        ysfx_init(fx.get());
        ysfx_process_float(fx.get(), nullptr, nullptr, 0, 0, 1);

        uint64_t touched;

        touched = ysfx_fetch_slider_touches(fx.get(), 0);
        REQUIRE(touched == (1 << 1));

        touched = ysfx_fetch_slider_touches(fx.get(), 0);
        REQUIRE(touched == (1 << 1));  // Shouldn't clear!

        touched = ysfx_fetch_slider_touches(fx.get(), 3);
        REQUIRE(touched == (ysfx_slider_mask(254, 3) | ysfx_slider_mask(255, 3)));

        touched = ysfx_fetch_slider_touches(fx.get(), 3);
        REQUIRE(touched == (ysfx_slider_mask(254, 3) | ysfx_slider_mask(255, 3)));
    }

    SECTION("release touch")
    {
        const char *text =
            "desc:example" "\n"
            "out_pin:output" "\n"
            "slider1:0<0,1,0.1>the slider 1" "\n"
            "slider2:0<0,1,0.1>the slider 2" "\n"
            "slider3:0<0,1,0.1>the slider 3" "\n"
            "slider255:0<0,1,0.1>the slider 255" "\n"
            "slider256:0<0,1,0.1>the slider 256" "\n"
            "@block" "\n"
            "slider_automate(slider2, 0);" "\n"
            "slider_automate(slider2, 1);" "\n"
            "slider_automate(slider255, 0);" "\n"
            "slider_automate(slider255, 1);" "\n"
            "slider_automate(slider256, 0);" "\n";

        scoped_new_dir dir_fx("${root}/Effects");
        scoped_new_txt file_main("${root}/Effects/example.jsfx", text);

        ysfx_config_u config{ysfx_config_new()};
        ysfx_u fx{ysfx_new(config.get())};

        REQUIRE(ysfx_load_file(fx.get(), file_main.m_path.c_str(), 0));
        REQUIRE(ysfx_compile(fx.get(), 0));

        ysfx_init(fx.get());
        ysfx_process_float(fx.get(), nullptr, nullptr, 0, 0, 1);

        uint64_t touched;
        touched = ysfx_fetch_slider_touches(fx.get(), 0);
        REQUIRE(touched == 0);
        touched = ysfx_fetch_slider_touches(fx.get(), 0);
        REQUIRE(touched == 0);
        touched = ysfx_fetch_slider_touches(fx.get(), 3);
        REQUIRE(touched == ysfx_slider_mask(255, 3));
        touched = ysfx_fetch_slider_touches(fx.get(), 3);
        REQUIRE(touched == ysfx_slider_mask(255, 3));  // We didn't stop touching 255.
    }
}

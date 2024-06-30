// Copyright 2021 Jean Pierre Cimalando
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
// Modifications by Joep Vanlier, 2024
//
// SPDX-License-Identifier: Apache-2.0
//

#include "ysfx.h"
#include "ysfx_utils.hpp"
#include "ysfx_test_utils.hpp"
#include <catch.hpp>
#include <cstring>

TEST_CASE("preset handling", "[preset]")
{
    SECTION("Bank from RPL")
    {
        const char *source_text =
            "desc:TestCaseRPL" "\n"
            "slider1:0<0,1,0.01>S1" "\n"
            "slider2:0<0,1,0.01>S2" "\n"
            "slider4:0<0,1,0.01>S4" "\n"
            "@serialize" "\n"
            "file_var(0, slider4);" "\n"
            "file_var(0, slider2);" "\n"
            "file_var(0, slider1);" "\n";

        const char *rpl_text =
            "<REAPER_PRESET_LIBRARY \"JS: TestCaseRPL\"" "\n"
            "  <PRESET `1.defaults`" "\n"
            "    MCAwIC0gMCAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0g" "\n"
            "    LSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAxLmRlZmF1bHRzAAAAAAAAAAAAAAAAAA==" "\n"
            "  >" "\n"
            "  <PRESET `2.a preset with spaces in the name`" "\n"
            "    MC4zNCAwLjc1IC0gMC42MiAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAt" "\n"
            "    IC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAiMi5hIHByZXNldCB3aXRoIHNwYWNlcyBpbiB0aGUgbmFtZSIAUrgePwAAQD97FK4+" "\n"
            "  >" "\n"
            "  <PRESET `3.a preset with \"quotes\" in the name`" "\n"
            "    MC44NiAwLjA3IC0gMC4yNSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAt" "\n"
            "    IC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAnMy5hIHByZXNldCB3aXRoICJxdW90ZXMiIGluIHRoZSBuYW1lJwAAAIA+KVyPPfYoXD8=" "\n"
            "  >" "\n"
            "  <PRESET `>`" "\n"
            "    MSAwLjkgLSAwLjggLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0g" "\n"
            "    LSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gPgDNzEw/ZmZmPwAAgD8=" "\n"
            "  >" "\n"
            ">" "\n";

        scoped_new_dir dir_fx("${root}/Effects");
        scoped_new_txt file_main("${root}/Effects/example.jsfx", source_text);
        scoped_new_txt file_rpl("${root}/Effects/example.jsfx.rpl", rpl_text);

        ysfx_bank_u bank{ysfx_load_bank(file_rpl.m_path.c_str())};
        REQUIRE(bank);

        REQUIRE(!strcmp(bank->name, "JS: TestCaseRPL"));

        REQUIRE(bank->presets != nullptr);
        REQUIRE(bank->preset_count == 4);

        ysfx_preset_t *preset;
        ysfx_state_t *state;

        preset = &bank->presets[0];
        REQUIRE(!strcmp(preset->name, "1.defaults"));
        state = preset->state;
        REQUIRE(state->slider_count == 3);
        REQUIRE(state->sliders[0].index == 0);
        REQUIRE(state->sliders[0].value == 0.0);
        REQUIRE(state->sliders[1].index == 1);
        REQUIRE(state->sliders[1].value == 0.0);
        REQUIRE(state->sliders[2].index == 3);
        REQUIRE(state->sliders[2].value == 0.0);
        REQUIRE(state->data_size == 3 * sizeof(float));
        REQUIRE(ysfx::unpack_f32le(&state->data[0 * sizeof(float)]) == 0.0);
        REQUIRE(ysfx::unpack_f32le(&state->data[1 * sizeof(float)]) == 0.0);
        REQUIRE(ysfx::unpack_f32le(&state->data[2 * sizeof(float)]) == 0.0);

        preset = &bank->presets[1];
        REQUIRE(!strcmp(preset->name, "2.a preset with spaces in the name"));
        state = preset->state;
        REQUIRE(state->slider_count == 3);
        REQUIRE(state->sliders[0].index == 0);
        REQUIRE(state->sliders[0].value == Approx(0.34));
        REQUIRE(state->sliders[1].index == 1);
        REQUIRE(state->sliders[1].value == Approx(0.75));
        REQUIRE(state->sliders[2].index == 3);
        REQUIRE(state->sliders[2].value == Approx(0.62));
        REQUIRE(state->data_size == 3 * sizeof(float));
        REQUIRE(ysfx::unpack_f32le(&state->data[0 * sizeof(float)]) == Approx(0.62));
        REQUIRE(ysfx::unpack_f32le(&state->data[1 * sizeof(float)]) == Approx(0.75));
        REQUIRE(ysfx::unpack_f32le(&state->data[2 * sizeof(float)]) == Approx(0.34));

        preset = &bank->presets[2];
        REQUIRE(!strcmp(preset->name, "3.a preset with \"quotes\" in the name"));
        state = preset->state;
        REQUIRE(state->slider_count == 3);
        REQUIRE(state->sliders[0].index == 0);
        REQUIRE(state->sliders[0].value == Approx(0.86));
        REQUIRE(state->sliders[1].index == 1);
        REQUIRE(state->sliders[1].value == Approx(0.07));
        REQUIRE(state->sliders[2].index == 3);
        REQUIRE(state->sliders[2].value == Approx(0.25));
        REQUIRE(state->data_size == 3 * sizeof(float));
        REQUIRE(ysfx::unpack_f32le(&state->data[0 * sizeof(float)]) == Approx(0.25));
        REQUIRE(ysfx::unpack_f32le(&state->data[1 * sizeof(float)]) == Approx(0.07));
        REQUIRE(ysfx::unpack_f32le(&state->data[2 * sizeof(float)]) == Approx(0.86));

        preset = &bank->presets[3];
        REQUIRE(!strcmp(preset->name, ">"));
        state = preset->state;
        REQUIRE(state->slider_count == 3);
        REQUIRE(state->sliders[0].index == 0);
        REQUIRE(state->sliders[0].value == Approx(1.0));
        REQUIRE(state->sliders[1].index == 1);
        REQUIRE(state->sliders[1].value == Approx(0.9));
        REQUIRE(state->sliders[2].index == 3);
        REQUIRE(state->sliders[2].value == Approx(0.8));
        REQUIRE(state->data_size == 3 * sizeof(float));
        REQUIRE(ysfx::unpack_f32le(&state->data[0 * sizeof(float)]) == Approx(0.8));
        REQUIRE(ysfx::unpack_f32le(&state->data[1 * sizeof(float)]) == Approx(0.9));
        REQUIRE(ysfx::unpack_f32le(&state->data[2 * sizeof(float)]) == Approx(1.0));
    }

    SECTION("Locate preset bank")
    {
        const char *text =
            "desc:example" "\n"
            "out_pin:output" "\n"
            "@sample" "\n"
            "spl0=0.0;" "\n";

        scoped_new_dir dir_fx("${root}/Effects");
        scoped_new_txt file_main("${root}/Effects/example.jsfx", text);

        ysfx_config_u config{ysfx_config_new()};
        ysfx_u fx{ysfx_new(config.get())};

        {
            REQUIRE(ysfx_load_file(fx.get(), file_main.m_path.c_str(), 0));
            REQUIRE(ysfx_get_bank_path(fx.get()) == std::string());
        }

        {
            scoped_new_txt file_rpl("${root}/Effects/example.jsfx.rpl", "");
            REQUIRE(ysfx_load_file(fx.get(), file_main.m_path.c_str(), 0));
            REQUIRE(ysfx_get_bank_path(fx.get()) == file_rpl.m_path);
            ysfx_unload(fx.get());
            REQUIRE(ysfx_get_bank_path(fx.get()) == std::string());
        }

        {
            scoped_new_txt file_rpl("${root}/Effects/example.jsfx.RpL", "");
            REQUIRE(ysfx_load_file(fx.get(), file_main.m_path.c_str(), 0));
            REQUIRE(!ysfx::ascii_casecmp(ysfx_get_bank_path(fx.get()), file_rpl.m_path.c_str()));
            ysfx_unload(fx.get());
            REQUIRE(ysfx_get_bank_path(fx.get()) == std::string());
        }
    }

    SECTION("Newer RPL Bank")
    {
        const char *source_text =
            "desc:TestCaseNewRPL" "\n"
            "slider1:sl1=0<-150,12,1>sl1" "\n"
            "slider2:sl2=1<-150,12,1>sl2" "\n"
            "slider2:sl2=2<-150,12,1>sl3" "\n"
            "slider3:sl3=3<-150,12,1>sl4" "\n"
            "slider4:sl4=4<-150,12,1>sl5" "\n"
            "slider5:sl5=5<-150,12,0.5>sl6" "\n"
            "slider6:sl6=6<-150,12,0.5>sl7" "\n"
            "slider128:sl7=5<-150,12,1>sl8" "\n"
            "slider256:sl8=6<-150,12,1:log>sl9" "\n"
            "@serialize" "\n"
            "array_loc = 100;" "\n"
            "array_loc[0] = 5;" "\n"
            "array_loc[1] = 10;" "\n"
            "array_loc[2] = 15;" "\n"
            "array_loc[3] = 20;" "\n"
            "file_var(0, 1337);" "\n"
            "file_mem(0, potato, 4);" "\n"
            "file_var(0, 1338);" "\n";

        const char *rpl_text =
            "<REAPER_PRESET_LIBRARY \"JS: TestCaseNewRPL\"" "\n"
            "  <PRESET `Moar`" "\n"
            "    MCAyIDMgNCAzLjE0MTUgMS4yMzQ1NjggLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0g" "\n"
            "    LSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSBNb2FyIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAt" "\n"
            "    IC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIDUgLSAtIC0gLSAtIC0gLSAt" "\n"
            "    IC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAt" "\n"
            "    IC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAt" "\n"
            "    IC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSA2AAAgp0QAAKBAAAAgQQAAcEEAAKBBAECnRA==" "\n"
            "  >" "\n"
            "  <PRESET `Moar Moar`" "\n" // "Moar Moar"
            "    MCAyIDMgNCAzLjE0MTUgMS4yMzQ1NjggLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0g" "\n"
            "    LSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAiTW9hciBNb2FyIiAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0g" "\n"
            "    LSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSA1IC0gLSAtIC0g" "\n"
            "    LSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0g" "\n"
            "    LSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0g" "\n"
            "    LSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gNgAAIKdEAACgQAAAIEEAAHBBAACgQQBAp0Q=" "\n"
            "  >" "\n"
            "  <PRESET `Moar \"Moar\" Moar\"`" "\n"  // 'Moar "Moar" Moar"'
            "    MCAyIDMgNCAzLjE0MTUgMS4yMzQ1NjggLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0g" "\n"
            "    LSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAnTW9hciAiTW9hciIgTW9hciInIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0g" "\n"
            "    LSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIDUg" "\n"
            "    LSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0g" "\n"
            "    LSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0g" "\n"
            "    LSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSA2AAAgp0QAAKBAAAAgQQAAcEEAAKBBAECnRA==" "\n"
            "  >" "\n"
            "  <PRESET `Moar \"Moar\" 'Moar\"`" "\n"  // 
            "    MCAyIDMgNCAzLjE0MTUgMS4yMzQ1NjggLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0g" "\n"
            "    LSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSBgTW9hciAiTW9hciIgJ01vYXIiYCAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAt" "\n"
            "    IC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSA1" "\n"
            "    IC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAt" "\n"
            "    IC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAt" "\n"
            "    IC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gNgAAIKdEAACgQAAAIEEAAHBBAACgQQBAp0Q=" "\n"
            "  >" "\n"
            "  <PRESET `Moar \"Moar\"' 'Moar\"`" "\n"
            "    MCAyIDMgNCAzLjE0MTUgMS4yMzQ1NjggLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0g" "\n"
            "    LSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAnTW9hciAiTW9hciInICdNb2FyImAgLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0g" "\n"
            "    LSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0g" "\n"
            "    NSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0g" "\n"
            "    LSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0g" "\n"
            "    LSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIDYAACCnRAAAoEAAACBBAABwQQAAoEEAQKdE" "\n"
            "  >" "\n"
            ">" "\n";

        scoped_new_dir dir_fx("${root}/Effects");
        scoped_new_txt file_main("${root}/Effects/example.jsfx", source_text);
        scoped_new_txt file_rpl("${root}/Effects/example.jsfx.rpl", rpl_text);

        ysfx_bank_u bank{ysfx_load_bank(file_rpl.m_path.c_str())};
        REQUIRE(bank);

        REQUIRE(!strcmp(bank->name, "JS: TestCaseNewRPL"));

        REQUIRE(bank->presets != nullptr);
        REQUIRE(bank->preset_count == 5);

        ysfx_preset_t *preset;
        ysfx_state_t *state;

        preset = &bank->presets[0];
        REQUIRE(!strcmp(preset->name, "Moar"));
        preset = &bank->presets[1];
        REQUIRE(!strcmp(preset->name, "Moar Moar"));
        preset = &bank->presets[2];
        REQUIRE(!strcmp(preset->name, "Moar \"Moar\" Moar\""));
        preset = &bank->presets[3];
        REQUIRE(!strcmp(preset->name, "Moar \"Moar\" 'Moar\""));
        preset = &bank->presets[4];
        REQUIRE(!strcmp(preset->name, "Moar \"Moar\"' 'Moar\""));

        for (size_t i=0; i<4; i++)
        {
            preset = &bank->presets[i];
            state = preset->state;
            REQUIRE(state->slider_count == 8);
            REQUIRE(state->sliders[0].index == 0);
            REQUIRE(state->sliders[0].value == 0);
            REQUIRE(state->sliders[1].index == 1);
            REQUIRE(state->sliders[1].value == 2);
            REQUIRE(state->sliders[2].index == 2);
            REQUIRE(state->sliders[2].value == 3);
            REQUIRE(state->sliders[3].index == 3);
            REQUIRE(state->sliders[3].value == 4);
            REQUIRE(state->sliders[4].index == 4);
            REQUIRE(state->sliders[4].value == 3.1415);
            REQUIRE(state->sliders[5].index == 5);
            REQUIRE(state->sliders[5].value == 1.234568);
            REQUIRE(state->sliders[6].index == 127);
            REQUIRE(state->sliders[6].value == 5);
            REQUIRE(state->sliders[7].index == 255);
            REQUIRE(state->sliders[7].value == 6);
            REQUIRE(state->data_size == 6 * sizeof(float));
            REQUIRE(ysfx::unpack_f32le(&state->data[0 * sizeof(float)]) == 1337.0);
            REQUIRE(ysfx::unpack_f32le(&state->data[1 * sizeof(float)]) == 5.0);
            REQUIRE(ysfx::unpack_f32le(&state->data[2 * sizeof(float)]) == 10.0);
            REQUIRE(ysfx::unpack_f32le(&state->data[3 * sizeof(float)]) == 15.0);
            REQUIRE(ysfx::unpack_f32le(&state->data[4 * sizeof(float)]) == 20.0);
            REQUIRE(ysfx::unpack_f32le(&state->data[5 * sizeof(float)]) == 1338.0);
        }
    }

    SECTION("Locate preset bank")
    {
        const char *text =
            "desc:example" "\n"
            "out_pin:output" "\n"
            "@sample" "\n"
            "spl0=0.0;" "\n";

        scoped_new_dir dir_fx("${root}/Effects");
        scoped_new_txt file_main("${root}/Effects/example.jsfx", text);

        ysfx_config_u config{ysfx_config_new()};
        ysfx_u fx{ysfx_new(config.get())};

        {
            REQUIRE(ysfx_load_file(fx.get(), file_main.m_path.c_str(), 0));
            REQUIRE(ysfx_get_bank_path(fx.get()) == std::string());
        }

        {
            scoped_new_txt file_rpl("${root}/Effects/example.jsfx.rpl", "");
            REQUIRE(ysfx_load_file(fx.get(), file_main.m_path.c_str(), 0));
            REQUIRE(ysfx_get_bank_path(fx.get()) == file_rpl.m_path);
            ysfx_unload(fx.get());
            REQUIRE(ysfx_get_bank_path(fx.get()) == std::string());
        }

        {
            scoped_new_txt file_rpl("${root}/Effects/example.jsfx.RpL", "");
            REQUIRE(ysfx_load_file(fx.get(), file_main.m_path.c_str(), 0));
            REQUIRE(!ysfx::ascii_casecmp(ysfx_get_bank_path(fx.get()), file_rpl.m_path.c_str()));
            ysfx_unload(fx.get());
            REQUIRE(ysfx_get_bank_path(fx.get()) == std::string());
        }
    }
}

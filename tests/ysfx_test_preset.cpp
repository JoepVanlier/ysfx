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
#include "ysfx_preset.hpp"
#include <catch.hpp>
#include <cstring>

void validatePreset(ysfx_preset_t *preset, const char* name, float slider1, float slider2, float slider3, float memory1, float memory2, float memory3)
{
    REQUIRE(!strcmp(preset->name, name));
    ysfx_state_t *state = preset->state;
    REQUIRE(state->slider_count == 3);
    REQUIRE(state->sliders[0].index == 0);
    REQUIRE(state->sliders[0].value == Approx(slider1));
    REQUIRE(state->sliders[1].index == 1);
    REQUIRE(state->sliders[1].value == Approx(slider2));
    REQUIRE(state->sliders[2].index == 3);
    REQUIRE(state->sliders[2].value == Approx(slider3));
    REQUIRE(state->data_size == 3 * sizeof(float));
    REQUIRE(ysfx::unpack_f32le(&state->data[0 * sizeof(float)]) == Approx(memory1));
    REQUIRE(ysfx::unpack_f32le(&state->data[1 * sizeof(float)]) == Approx(memory2));
    REQUIRE(ysfx::unpack_f32le(&state->data[2 * sizeof(float)]) == Approx(memory3));
}

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

        validatePreset(&bank->presets[0], "1.defaults", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
        validatePreset(&bank->presets[1], "2.a preset with spaces in the name", 0.34f, 0.75f, 0.62f, 0.62f, 0.75f, 0.34f);
        validatePreset(&bank->presets[2], "3.a preset with \"quotes\" in the name", 0.86f, 0.07f, 0.25f, 0.25f, 0.07f, 0.86f);
        validatePreset(&bank->presets[3], ">", 1.0f, 0.9f, 0.8f, 0.8f, 0.9f, 1.0f);
    }

    SECTION("Store preset in bank")
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
            ">" "\n";

        scoped_new_dir dir_fx("${root}/Effects");
        scoped_new_txt file_main("${root}/Effects/example.jsfx", source_text);
        scoped_new_txt file_rpl("${root}/Effects/example.jsfx.rpl", rpl_text);

        ysfx_bank_u bank{ysfx_load_bank(file_rpl.m_path.c_str())};
        REQUIRE(bank);

        REQUIRE(!strcmp(bank->name, "JS: TestCaseRPL"));

        REQUIRE(bank->presets != nullptr);
        REQUIRE(bank->preset_count == 1);

        validatePreset(&bank->presets[0], "1.defaults", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
        ysfx_state_t *state = bank->presets[0].state;

        // Note that the new bank will own the state we are adding, so we need explicit duplication
        ysfx_state_t *state2 = ysfx_state_dup(state);
        state2->sliders[0].value = 5.0;
        state2->sliders[2].value = 1337.0;
        ysfx::pack_f32le(1337.0, &state2->data[1 * sizeof(float)]);
        ysfx_bank_u new_bank{ysfx_add_preset_to_bank(bank.get(), "added preset", state2)};

        REQUIRE(bank->presets != nullptr);
        REQUIRE(bank->preset_count == 1);
        REQUIRE(new_bank->presets != nullptr);
        REQUIRE(new_bank->preset_count == 2);

        validatePreset(&bank->presets[0], "1.defaults", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
        validatePreset(&new_bank->presets[0], "1.defaults", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
        validatePreset(&new_bank->presets[1], "added preset", 5.0f, 0.0f, 1337.0f, 0.0f, 1337.0f, 0.0f);

        // Validate that the banks don't point to the same memory (it should have made its own copy)
        REQUIRE(new_bank->presets[0].name != bank->presets[0].name);
        REQUIRE(new_bank->presets[0].state != bank->presets[0].state);

        ysfx_state_t *state3 = ysfx_state_dup(state);
        state3->sliders[0].value = 15.0;
        state3->sliders[1].value = -2.0;
        ysfx::pack_f32le(60083773.0f, &state3->data[2 * sizeof(float)]);
        ysfx_bank_u new_bank2{ysfx_add_preset_to_bank(new_bank.get(), "preset ' with \"quotes\" in the name", state3)};

        validatePreset(&new_bank2->presets[0], "1.defaults", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
        validatePreset(&new_bank2->presets[1], "added preset", 5.0f, 0.0f, 1337.0f, 0.0f, 1337.0f, 0.0f);
        validatePreset(&new_bank2->presets[2], "preset ' with \"quotes\" in the name", 15.0f, -2.0f, 0.0f, 0.0f, 0.0f, 60083773.0f);

        REQUIRE(ysfx_preset_exists(nullptr, "test") == 0);
        REQUIRE(ysfx_preset_exists(new_bank2.get(), "added preset") == 2);
        REQUIRE(ysfx_preset_exists(new_bank2.get(), "doesn't exist") == 0);

        // We're gonna overwrite a particular state
        ysfx_state_t *state4 = ysfx_state_dup(state);
        state4->sliders[0].value = 3.141592657;
        state4->sliders[1].value = 42;
        ysfx::pack_f32le(-1.5f, &state4->data[0 * sizeof(float)]);
        ysfx_bank_u new_bank3{ysfx_add_preset_to_bank(new_bank2.get(), "added preset", state4)};

        // Verify that we didn't change the old bank
        REQUIRE(new_bank2->preset_count == 3);
        validatePreset(&new_bank2->presets[0], "1.defaults", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
        validatePreset(&new_bank2->presets[1], "added preset", 5.0f, 0.0f, 1337.0f, 0.0f, 1337.0f, 0.0f);
        validatePreset(&new_bank2->presets[2], "preset ' with \"quotes\" in the name", 15.0f, -2.0f, 0.0f, 0.0f, 0.0f, 60083773.0f);

        REQUIRE(new_bank3->preset_count == 3);
        validatePreset(&new_bank3->presets[0], "1.defaults", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
        validatePreset(&new_bank3->presets[1], "added preset", 3.141592657f, 42.0f, 0.0f, -1.5f, 0.0f, 0.0f);
        validatePreset(&new_bank3->presets[2], "preset ' with \"quotes\" in the name", 15.0f, -2.0f, 0.0f, 0.0f, 0.0f, 60083773.0f);
        REQUIRE(new_bank3->presets[1].state == state4);
    }

    SECTION("Delete preset from bank")
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

        validatePreset(&bank->presets[0], "1.defaults", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
        validatePreset(&bank->presets[1], "2.a preset with spaces in the name", 0.34f, 0.75f, 0.62f, 0.62f, 0.75f, 0.34f);
        validatePreset(&bank->presets[2], "3.a preset with \"quotes\" in the name", 0.86f, 0.07f, 0.25f, 0.25f, 0.07f, 0.86f);
        validatePreset(&bank->presets[3], ">", 1.0f, 0.9f, 0.8f, 0.8f, 0.9f, 1.0f);

        ysfx_bank_u new_bank{ysfx_delete_preset_from_bank(bank.get(), "2.a preset with spaces in the name")};

        REQUIRE(bank->preset_count == 4);
        validatePreset(&bank->presets[0], "1.defaults", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
        validatePreset(&bank->presets[1], "2.a preset with spaces in the name", 0.34f, 0.75f, 0.62f, 0.62f, 0.75f, 0.34f);
        validatePreset(&bank->presets[2], "3.a preset with \"quotes\" in the name", 0.86f, 0.07f, 0.25f, 0.25f, 0.07f, 0.86f);
        validatePreset(&bank->presets[3], ">", 1.0f, 0.9f, 0.8f, 0.8f, 0.9f, 1.0f);
        
        REQUIRE(new_bank->preset_count == 3);
        validatePreset(&new_bank->presets[0], "1.defaults", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
        validatePreset(&new_bank->presets[1], "3.a preset with \"quotes\" in the name", 0.86f, 0.07f, 0.25f, 0.25f, 0.07f, 0.86f);
        validatePreset(&new_bank->presets[2], ">", 1.0f, 0.9f, 0.8f, 0.8f, 0.9f, 1.0f);
    }
    
    SECTION("Create empty bank")
    {
        ysfx_bank_u bank{ysfx_create_empty_bank("test")};
        REQUIRE(strcmp(bank->name, "test") == 0);

        ysfx_bank_u bank2{ysfx_create_empty_bank("preset ' with \"quotes\" in the name")};
        REQUIRE(strcmp(bank2->name, "preset ' with \"quotes\" in the name") == 0);
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
            "  <PRESET `- -`"
            "    MCAyIDMgNCAzLjE0MTUgMS4yMzQ1NjggLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0g" "\n"
            "    LSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAiLSAtIiAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0g" "\n"
            "    LSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSA1IC0gLSAtIC0gLSAtIC0g" "\n"
            "    LSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0g" "\n"
            "    LSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0g" "\n"
            "    LSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gLSAtIC0gNgAAIKdEAACgQAAAIEEAAHBBAACgQQBAp0Q=" "\n"
            "  >" "\n"
            ">" "\n";

        scoped_new_dir dir_fx("${root}/Effects");
        scoped_new_txt file_main("${root}/Effects/example.jsfx", source_text);
        scoped_new_txt file_rpl("${root}/Effects/example.jsfx.rpl", rpl_text);

        ysfx_bank_u bank{ysfx_load_bank(file_rpl.m_path.c_str())};
        REQUIRE(bank);

        REQUIRE(!strcmp(bank->name, "JS: TestCaseNewRPL"));

        REQUIRE(bank->presets != nullptr);
        REQUIRE(bank->preset_count == 6);

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
        preset = &bank->presets[5];
        REQUIRE(!strcmp(preset->name, "- -"));

        for (size_t i=0; i<bank->preset_count; i++)
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

    SECTION("round-trip")
    {
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
        ">" "\n";
        
        scoped_new_dir dir_fx("${root}/Effects");
        scoped_new_txt file_rpl("${root}/Effects/test.rpl", rpl_text);

        ysfx_bank_u bank{ysfx_load_bank(file_rpl.m_path.c_str())};
        std::string stored_bank = ysfx_save_bank_to_rpl_text(bank.get());

        REQUIRE(stored_bank == rpl_text);
        bool save_success = ysfx_save_bank(resolve_path("${root}/Effects/saved.rpl").c_str(), bank.get());

        REQUIRE(save_success == true);

        ysfx_bank_u bank2{ysfx_load_bank(resolve_path("${root}/Effects/saved.rpl").c_str())};

        REQUIRE(strcmp(bank->name, bank2->name) == 0);
        for (uint32_t i=0; i < bank->preset_count; i++) {
            REQUIRE(strcmp(bank->presets[i].name, bank2->presets[i].name) == 0);
            REQUIRE(bank->presets[i].state->slider_count == bank2->presets[i].state->slider_count);
            for (uint32_t s=0; s < bank->presets[i].state->slider_count; s++) {
                REQUIRE(bank->presets[i].state->sliders->index == bank2->presets[i].state->sliders->index);
                REQUIRE(bank->presets[i].state->sliders->value == bank2->presets[i].state->sliders->value);
            }

            REQUIRE(bank->presets[i].state->data_size == bank2->presets[i].state->data_size);
            for (uint32_t s=0; s < bank->presets[i].state->data_size; s++) {
                REQUIRE(bank->presets[i].state->data[s] == bank2->presets[i].state->data[s]);
            }
        }
    }
}

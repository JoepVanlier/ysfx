// Copyright 2026 Joep Vanlier
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

#include <filesystem>
#include "../sources/ysfx.hpp"
#include "../sources/ysfx_config.hpp"
#include "../sources/ysfx_eel_utils.hpp"
#include "../sources/ysfx_api_eel.hpp"
#include "../sources/ysfx_preprocess.hpp"
#include <vector>
#include <functional>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <set>
#include <map>

namespace fs = std::filesystem;


struct {
    const char *input_file = nullptr;
} args;

template <typename T>
decltype(auto) printf_arg(T&& value)
{
    using U = std::decay_t<T>;

    if constexpr (std::is_same_v<U, std::string>)
        return value.c_str();
    else
        return std::forward<T>(value);
}

template <typename... Args>
void log(const char* format, Args&&... args)
{
    std::printf(format, printf_arg(std::forward<Args>(args))...);
}

void print_help()
{
    log("               ____        __              __    \n");
    log("   __  _______/ __/  __   / /_____  ____  / /____\n");
    log("  / / / / ___/ /_| |/_/  / __/ __ \\/ __ \\/ / ___/\n");
    log(" / /_/ (__  ) __/>  <   / /_/ /_/ / /_/ / (__  ) \n");
    log(" \\__, /____/_/ /_/|_|   \\__/\\____/\\____/_/____/  \n");
    log("/____/        \n\n");
    log("Usage: \n");
    log("  ysfx_tools -f <filename.jsfx>\n");
    log("    Runs preprocessor on jsfx file and prints output\n");
    log("  ysfx_tools -s <filename.jsfx>\n");
    log("    Lists (free) sliders and their indices\n");
    log("  ysfx_tools -t <from_filename.jsfx> <to_filename.jsfx> <from_rpl.rpl> <to_rpl.rpl>");
    log("    Transfer presets from an RPL file from one JSFX to another. Sliders are matched by name.\n");
    log("  ysfx_tools -m <from_filename.jsfx> <to_filename.jsfx> <from_rpl.rpl> <to_rpl.rpl>");
    log("    Transfer presets from an RPL file from one JSFX to another. Sliders are matched by index.\n");    
}

int preprocess_jsfx(const char* filepath)
{
    ysfx::file_uid main_uid;
    ysfx::FILE_u stream{ysfx::fopen_utf8(filepath, "rb")};
    if (!stream || !ysfx::get_stream_file_uid(stream.get(), main_uid)) {
        log("%s: cannot open file for reading", ysfx::path_file_name(filepath));
        return 2;
    }

    ysfx_parse_error error;
    ysfx::stdio_text_reader raw_reader(stream.get());

    // run the preprocessor first
    std::map<std::string, ysfx_real> preprocessor_values;

    std::string preprocessed;
    if (!ysfx_preprocess(raw_reader, &error, preprocessed, preprocessor_values)) {
        log("%s:%u: %s", ysfx::path_file_name(filepath), error.line + 1, error.message);
        return 2;
    }
    
    log("%s", preprocessed);
    
    return 1;
}

ysfx_t *quick_load(const char* filepath, bool compile)
{
    uint32_t loadopts = 0;
    uint32_t compileopts = 0;

    ysfx_config_u config{ysfx_config_new()};
    ysfx_register_builtin_audio_formats(config.get());
    ysfx_guess_file_roots(config.get(), filepath);

    ysfx_t *fx = ysfx_new(config.get());   
    if (!ysfx_load_file(fx, filepath, loadopts)) {
        ysfx_free(fx);
        return nullptr;
    }

    if (compile) {
        ysfx_compile(fx, 0);
        ysfx_init(fx);
    }

    return fx;
}

int parse_sliders(const char* filepath)
{
    ysfx_u fx{quick_load(filepath, false)};
    if (!fx) return 2;

    std::vector<int> unused_slots;
    for (int i = 0; i < ysfx_max_sliders; i++)
    {
        if (ysfx_slider_exists(fx.get(), i)) {
            log("%d: %s: %f\n", i, ysfx_slider_get_name(fx.get(), i), ysfx_slider_get_value(fx.get(), i));
        } else {
            unused_slots.push_back(i);
        }
    }

    std::string free_str{""};
    for (auto it: unused_slots)
        free_str += std::to_string(it) + " ";

    log("\nFree sliders (%d):\n%s", unused_slots.size(), free_str);

    return 1;
}

int map_presets(const char* from_jsfx_filepath, const char* to_jsfx_filepath, const char* from_bank_filepath, const char* to_bank_filepath)
{
    ysfx_u from_fx{quick_load(from_jsfx_filepath, false)};
    if (!from_fx) {
        log("Failed to compile %s", from_jsfx_filepath);
        return 2;
    }
    ysfx_u to_fx{quick_load(to_jsfx_filepath, false)};
    if (!to_fx) {
        log("Failed to compile %s", to_jsfx_filepath);
        return 2;
    }

    ysfx_bank_u from_bank{ysfx_load_bank(from_bank_filepath)};
    if (!from_bank) {
        log("Failed to load bank %s", from_bank_filepath);
        return 2;
    }

    ysfx_bank_u to_bank{ysfx_create_empty_bank(to_bank_filepath)};
    if (!to_bank) return 2;

    // First we load the preset into the old plugin.
    log("\nProcessing presets");
    for (uint32_t idx = 0; idx < from_bank->preset_count; ++idx) {
        log(".");
        //log("Processing preset %s\n", from_bank->presets[idx].name);
        auto preset = from_bank->presets[idx];
        auto target_state = ysfx_convert_state(from_fx.get(), to_fx.get(), preset.state);
        to_bank.reset(ysfx_add_preset_to_bank(to_bank.get(), preset.name, target_state));
    }

    ysfx_save_bank(to_bank_filepath, to_bank.get());
    return 1;
}

int map_presets_reinterpret(const char* from_jsfx_filepath, const char* to_jsfx_filepath, const char* from_bank_filepath, const char* to_bank_filepath)
{
    ysfx_u from_fx{quick_load(from_jsfx_filepath, true)};
    if (!from_fx) {
        log("Failed to compile %s", from_jsfx_filepath);
        return 2;
    }
    ysfx_u to_fx{quick_load(to_jsfx_filepath, true)};
    if (!to_fx) {
        log("Failed to compile %s", to_jsfx_filepath);
        return 2;
    }

    ysfx_bank_u from_bank{ysfx_load_bank(from_bank_filepath)};
    if (!from_bank) {
        log("Failed to load bank %s", from_bank_filepath);
        return 2;
    }

    ysfx_bank_u to_bank{ysfx_create_empty_bank(to_bank_filepath)};
    if (!to_bank) return 2;

    std::vector<int> sliders_to;
    for (int i = 0; i < ysfx_max_sliders; i++)
    {
        if (ysfx_slider_exists(to_fx.get(), i)) {
            sliders_to.push_back(i);

            if (ysfx_slider_exists(from_fx.get(), i)) {
                log("Mapped %30s <=> %s\n", ysfx_slider_get_name(from_fx.get(), i), ysfx_slider_get_name(to_fx.get(), i));
            } else {
                log("Missing %30s <=> %s\n", "-", ysfx_slider_get_name(to_fx.get(), i));
            }
        }
    }

    std::vector<int> sliders_from;
    for (int i = 0; i < ysfx_max_sliders; i++)
    {
        if (ysfx_slider_exists(from_fx.get(), i)) {
            sliders_from.push_back(i);
            if (!ysfx_slider_exists(to_fx.get(), i)) {
                log("Missing %30s <=> %s\n", ysfx_slider_get_name(from_fx.get(), i), "-");
            }
        }
    }

    // First we load the preset into the old plugin.
    log("\nProcessing presets");
    for (uint32_t idx = 0; idx < from_bank->preset_count; ++idx) {
        log(".");
        //log("Processing preset %s\n", from_bank->presets[idx].name);
        auto preset = from_bank->presets[idx];
        
        ysfx_init(from_fx.get());
        ysfx_load_state(from_fx.get(), preset.state);

        // Reinitialize the target jsfx
        restore_slider_defaults(to_fx.get());
        ysfx_init(to_fx.get());

        // We load the serialized state into the target jsfx
        ysfx_load_serialized_state(to_fx.get(), preset.state);

        // Load our slider data in the target jsfx
        for (uint32_t i=0; i < preset.state->slider_count; ++i) {
            auto from_slider_idx = preset.state->sliders[i].index;
            
            if (ysfx_slider_exists(to_fx.get(), from_slider_idx) && ysfx_slider_exists(from_fx.get(), from_slider_idx)) {
                ysfx_real value = ysfx_slider_get_value(from_fx.get(), from_slider_idx);

                const auto to_slider_idx = from_slider_idx;  // Slider indices don't change)
                ysfx_slider_set_value(to_fx.get(), to_slider_idx, value, false);
            }
        }

        // We load the serialized state into the target jsfx
        ysfx_load_serialized_state(to_fx.get(), preset.state);

        auto target_state = ysfx_save_state(to_fx.get());
        to_bank.reset(ysfx_add_preset_to_bank(to_bank.get(), preset.name, target_state));
    }

    ysfx_save_bank(to_bank_filepath, to_bank.get());

    return 1;
}

int compare_rpl(const char* jsfx_filepath, const char* bank1_filepath, const char* bank2_filepath)
{
    ysfx_u fx{quick_load(jsfx_filepath, true)};
    if (!fx) {
        log("Failed to compile %s", jsfx_filepath);
        return 2;
    }

    ysfx_bank_u bank1{ysfx_load_bank(bank1_filepath)};
    if (!bank1) {
        log("Failed to load bank %s", bank1_filepath);
        return 2;
    }

    ysfx_bank_u bank2{ysfx_load_bank(bank2_filepath)};
    if (!bank2) {
        log("Failed to load bank %s", bank2_filepath);
        return 2;
    }

    for (uint32_t idx = 0; idx < bank1->preset_count; ++idx) {
        if (ysfx_preset_exists(bank2.get(), bank1->presets[idx].name) == 0)
            log("Preset %s exists in bank 1, but not 2");
    }

    for (uint32_t idx = 0; idx < bank2->preset_count; ++idx) {
        if (ysfx_preset_exists(bank1.get(), bank2->presets[idx].name) == 0)
            log("Preset %s exists in bank 2, but not 1");
    }

    // First we load the preset into the old plugin.
    log("\nComparing presets ...\n");
    int pass{0};
    int fail{0};
    for (uint32_t idx = 0; idx < bank1->preset_count; ++idx) {
        auto preset1 = bank1->presets[idx];
        auto preset2_idx = ysfx_preset_exists(bank2.get(), preset1.name);
        if (preset2_idx > 0) {
            auto preset2 = bank2->presets[preset2_idx - 1];
            
            restore_slider_defaults(fx.get());
            ysfx_init(fx.get());
            ysfx_load_state(fx.get(), preset1.state);
            auto state1 = ysfx_save_state(fx.get());
            
            restore_slider_defaults(fx.get());
            ysfx_init(fx.get());
            ysfx_load_state(fx.get(), preset2.state);
            auto state2 = ysfx_save_state(fx.get());

            if (!ysfx_is_state_equal(state1, state2)) {
                log("Unequal state for %s / %s\n", preset1.name, preset2.name);
                fail += 1;

                if (state1->slider_count != state2->slider_count) {
                    log("Slider count %d vs %s.\n", state1->slider_count, state2->slider_count);
                } else {
                    for (uint32_t i=0; i < state1->slider_count; ++i) {
                        auto index = state1->sliders[i].index;
                        auto name = ysfx_slider_get_identifier(fx.get(), index);
                        if (state1->sliders[i].value != state2->sliders[i].value) {
                            log("Slider values are different %s (%d): %f vs %f.\n", name, index, state1->sliders[i].value, state2->sliders[i].value);
                        }
                    }
                }
                if (state1->data_size != state2->data_size) {
                    log("Data size %d vs %s.\n", state1->data_size, state2->data_size);
                } else {
                    if (memcmp(state1->data, state2->data, state1->data_size) != 0) {
                        log("Serialization chunk is different");
                    };
                }
            } else {
                pass += 1;
            };
        }
    }
    log("\nPassed: %d, Failed: %d\n\n", pass, fail);

    return 1;
}

int interpret_args(int argc, char *argv[])
{
    if (argc < 3) return 0;

    if (strncmp(argv[1], "-f", 2) == 0) {
        return preprocess_jsfx(argv[2]);
    }
    if (strncmp(argv[1], "-s", 2) == 0) {
        return parse_sliders(argv[2]);
    }
    if (strncmp(argv[1], "-m", 2) == 0) {
        if (argc < 6) return 0;
        return map_presets_reinterpret(argv[2], argv[3], argv[4], argv[5]);
    }    
    if (strncmp(argv[1], "-t", 2) == 0) {
        if (argc < 6) return 0;
        return map_presets(argv[2], argv[3], argv[4], argv[5]);
    }
    if (strncmp(argv[1], "-c", 2) == 0) {
        if (argc < 5) return 0;
        return compare_rpl(argv[2], argv[3], argv[4]);
    }

    return 0;
}

int main(int argc, char *argv[])
{    
    if (interpret_args(argc, argv) == 0) {
        print_help();
        return 0;
    }
}

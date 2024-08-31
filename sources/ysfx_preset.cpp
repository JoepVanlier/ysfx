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
#include "ysfx_preset.hpp"
#include "ysfx_utils.hpp"
#include <vector>
#include <string>
#include <cstring>
#include <cmath>
#include <algorithm>

#include <iomanip>
#include <sstream>
#include <locale>

#include "WDL/lineparse.h"

static void ysfx_preset_clear(ysfx_preset_t *preset);
static ysfx_bank_t *ysfx_load_bank_from_rpl_text(const std::string &text);
static void ysfx_parse_preset_from_rpl_blob(ysfx_preset_t *preset, const char *name, const std::vector<uint8_t> &data);

ysfx_bank_t *ysfx_load_bank(const char *path)
{
#if defined(_WIN32)
    std::wstring wpath = ysfx::widen(path);
    ysfx::FILE_u stream{_wfopen(wpath.c_str(), L"rb")};
#else
    ysfx::FILE_u stream{fopen(path, "rb")};
#endif

    if (!stream)
        return nullptr;

    std::string input;
    constexpr uint32_t max_input = 1u << 24;
    input.reserve(1u << 16);

    for (int ch; input.size() < max_input && (ch = fgetc(stream.get())) != EOF; ) {
        ch = (ch == '\r' || ch == '\n') ? ' ' : ch;
        input.push_back((unsigned char)ch);
    }

    if (ferror(stream.get()))
        return nullptr;

    stream.reset();
    return ysfx_load_bank_from_rpl_text(input);
}

void ysfx_bank_free(ysfx_bank_t *bank)
{
    if (!bank)
        return;

    delete[] bank->name;

    if (ysfx_preset_t *presets = bank->presets) {
        uint32_t count = bank->preset_count;
        for (uint32_t i = 0; i < count; ++i)
            ysfx_preset_clear(&presets[i]);
        delete[] presets;
    }

    delete bank;
}

static void ysfx_preset_clear(ysfx_preset_t *preset)
{
    delete[] preset->name;
    preset->name = nullptr;

    delete[] preset->blob_name;
    preset->blob_name = nullptr;

    ysfx_state_free(preset->state);
    preset->state = nullptr;
}

static ysfx_bank_t *ysfx_load_bank_from_rpl_text(const std::string &text)
{
    LineParser parser;
    if (parser.parse(text.c_str()) < 0)
        return nullptr;

    ///
    std::vector<ysfx_preset_t> preset_list;
    preset_list.reserve(256);

    auto list_cleanup = ysfx::defer([&preset_list]() {
        for (ysfx_preset_t &pst : preset_list)
            ysfx_preset_clear(&pst);
    });

    ///
    int ntok = parser.getnumtokens();
    int itok = 0;

    if (strcmp("<REAPER_PRESET_LIBRARY", parser.gettoken_str(itok++)) != 0)
        return nullptr;

    const char *bank_name = parser.gettoken_str(itok++);

    while (itok < ntok) {
        if (strcmp("<PRESET", parser.gettoken_str(itok++)) == 0) {
            const char *preset_name = parser.gettoken_str(itok++);

            std::vector<uint8_t> blob;
            blob.reserve(64 * 1024);

            for (const char *part; itok < ntok &&
                     strcmp(">", (part = parser.gettoken_str(itok++))) != 0; )
            {
                std::vector<uint8_t> blobChunk = ysfx::decode_base64(part);
                blob.insert(blob.end(), blobChunk.begin(), blobChunk.end());
            }

            preset_list.emplace_back();
            ysfx_preset_t &preset = preset_list.back();

            ysfx_parse_preset_from_rpl_blob(&preset, preset_name, blob);
        }
    }

    ///
    ysfx_bank_u bank{new ysfx_bank_t{}};
    bank->name = ysfx::strdup_using_new(bank_name);
    bank->presets = new ysfx_preset_t[(uint32_t)preset_list.size()]{};
    bank->preset_count = (uint32_t)preset_list.size();

    for (uint32_t i = (uint32_t)preset_list.size(); i-- > 0; ) {
        bank->presets[i] = preset_list[i];
        preset_list.pop_back();
    }

    return bank.release();
}

static int hasFunkyCharacters(const char *in)
{
    int flags = 0;
    const char *p = in;
    while (*p && (flags != 15))
    {
        char c = *p++;
        if (c == '"') flags |= 1;
        else if (c == '\'') flags |= 2;
        else if (c == '`') flags |= 4;
        else if (c == ' ') flags |= 8;
    }

    return flags;
}

std::string escapeString(const char *in)
{
    int flags = hasFunkyCharacters(in);

    if (flags == 0) return std::string(in);
    std::string outString{""};
    outString.reserve(64);

    if (flags != 15)
    {
        const char src = (flags & 1) ? ((flags & 2) ? '`' : '\'') : '"';
        outString.append(1, src).append(in).append(1, src);
    }
    else  // ick, change ` into '
    {
        outString.append(1, '`').append(in).append(1, '`');
        std::replace(outString.begin() + 1, outString.end() - 1, '`', '\'');
    }

    return outString;
}

static std::string remove_name_from_preset_blob(const char *text, const char *name, std::string& replaced_name)
{
    // Unfortunately, there's some questionable escaping going on sometimes. This is not a problem
    // when it comes to the preset header, but in the middle of the blob it can be problematic.
    // Here we try to find the name in the blob and replace it, so that special characters don't
    // make our life harder than it needs to be.
    if (!(hasFunkyCharacters(name) & 7)) return std::string{""};

    std::string name_replace{text};
    std::size_t name_pos = name_replace.find(name);

    if (name_pos == std::string::npos) return std::string{""};

    // Make sure what we found is *unique*, otherwise bail out
    std::size_t differentMatch = name_replace.find("name", name_pos + 1, 6);
    if (differentMatch != std::string::npos) return std::string{""};

    // Move left from the name we found until we hit a space (separator).
    std::size_t start_pos = name_pos;
    while((name_replace[start_pos] != ' ') && (start_pos > 0)) start_pos -= 1;
    start_pos += 1;

    // Move right until we hit a space.
    std::size_t stop_pos = name_pos + strlen(name);
    while((name_replace[stop_pos] != ' ') && (stop_pos < name_replace.length())) stop_pos += 1;

    replaced_name = name_replace.substr(start_pos, stop_pos - start_pos);
    name_replace.replace(start_pos, stop_pos - start_pos, stop_pos - start_pos, '_');

    return name_replace;
}

static void ysfx_parse_preset_from_rpl_blob(ysfx_preset_t *preset, const char *name, const std::vector<uint8_t> &data)
{
    ysfx_state_t state{};
    std::vector<ysfx_state_slider_t> sliders;

    preset->blob_name = nullptr;
    const char *text = (const char *)data.data();
    size_t len = data.size();

    // find the null terminator
    size_t pos = 0;
    while (pos < len && data[pos] != 0)
        ++pos;

    // skip null terminator if there was one
    // otherwise null-terminate the text
    std::string textbuf;
    if (pos < len)
        ++pos;
    else {
        textbuf.assign(text, len);
        text = textbuf.c_str();
    }

    // whatever follows null is the raw serialization
    state.data = const_cast<uint8_t *>(data.data() + pos);
    state.data_size = len - pos;

    std::string replaced_name;
    std::string name_removed = remove_name_from_preset_blob(text, name, replaced_name);
    if (!name_removed.empty()) {
        text = name_removed.c_str();

        // Store the blob name for recall later. This allows us to save files without changing
        // these names in any way (they may not actually be the same as the preset name unfortunately)
        preset->blob_name = ysfx::strdup_using_new(replaced_name.c_str());
    }

    // parse a line of 64 slider floats (or '-' if missing)
    LineParser parser;
    if (parser.parse(text) >= 0) {
        sliders.reserve(ysfx_max_sliders);

        // Grab the first 64 sliders
        for (uint32_t i = 0; i < 64; ++i) {
            const char *str = parser.gettoken_str(i);
            bool skip = str[0] == '-' && str[1] == '\0';
            if (!skip) {
                ysfx_state_slider_t slider{};
                slider.index = i;
                slider.value = (ysfx_real)ysfx::dot_atof(str);
                sliders.push_back(slider);
            }
        }

        // Token 64 has the "name" of the preset again, but escaped weirdly (see WDL/projectcontext.cpp maybe?).
        const char *str = parser.gettoken_str(65);  // Determines whether we continue
        if (name_removed.empty()) {
            // Store the blob name for recall later
            preset->blob_name = ysfx::strdup_using_new(escapeString(parser.gettoken_str(64)).c_str());
        }

        if (str[0] != '\0') {
            // Grab the rest
            for (uint32_t i = 0; i < ysfx_max_sliders - 64; ++i) {
                const char *str = parser.gettoken_str(i + 65);
                bool skip = str[0] == '-' && str[1] == '\0';
                if (!skip) {
                    ysfx_state_slider_t slider{};
                    slider.index = i + 64;
                    slider.value = (ysfx_real)ysfx::dot_atof(str);
                    sliders.push_back(slider);
                }
            }
        }

        state.sliders = sliders.data();
        state.slider_count = (uint32_t)sliders.size();
    }

    // If for whatever reason we didn't find a blob name, use the regular name instead.
    if (!preset->blob_name) {
        preset->blob_name = ysfx::strdup_using_new(escapeString(name).c_str());
    }

    preset->name = ysfx::strdup_using_new(name);
    preset->state = ysfx_state_dup(&state);
}

uint32_t ysfx_preset_exists(ysfx_bank_t *bank, const char* preset_name)
{
    if (!bank) return 0;

    uint32_t found = 0;
    for (uint32_t i=0; i < bank->preset_count; i++) {
        if (strcmp(bank->presets[i].name, preset_name) == 0) {
            // Preset already exists! We're gonna be overwriting this thing.
            found = i + 1;
        }
    }

    return found;
}

ysfx_bank_t *ysfx_create_empty_bank(const char* bank_name)
{
    ysfx_bank_u bank{new ysfx_bank_t{}};

    bank->name = ysfx::strdup_using_new(bank_name);
    bank->preset_count = 0;
    return bank.release();
}

// Adds preset to a bank and returns new bank with extra preset. Note that the preset takes responsibility for the memory 
// ysfx_state_t* is pointing to. This function returns a *new* bank and you are responsible for cleaning up the old bank.
ysfx_bank_t *ysfx_add_preset_to_bank(ysfx_bank_t *bank_in, const char* preset_name, ysfx_state_t *state)
{
    ysfx_bank_u bank{new ysfx_bank_t{}};
    bank->name = ysfx::strdup_using_new(bank_in->name);

    uint32_t found = ysfx_preset_exists(bank_in, preset_name);
    bank->preset_count = (uint32_t)(bank_in->preset_count);
    if (found == 0) bank->preset_count += 1;

    bank->presets = new ysfx_preset_t[(uint32_t)bank->preset_count]{};
    for (uint32_t i=0; i < bank_in->preset_count; i++) {
        if ((!found) || (i != (found - 1))) {
            bank->presets[i].name = ysfx::strdup_using_new(bank_in->presets[i].name);
            bank->presets[i].blob_name = ysfx::strdup_using_new(bank_in->presets[i].blob_name);
            bank->presets[i].state = ysfx_state_dup(bank_in->presets[i].state);
        }
    }

    uint32_t index = (found == 0) ? (bank->preset_count - 1) : (found - 1);
    bank->presets[index].name = ysfx::strdup_using_new(preset_name);
    bank->presets[index].blob_name = ysfx::strdup_using_new(escapeString(preset_name).c_str());
    bank->presets[index].state = state;

    return bank.release();
}

// Deletes a preset from the bank. This function returns a *new* bank and you are responsible for cleaning up the old bank.
ysfx_bank_t *ysfx_delete_preset_from_bank(ysfx_bank_t *bank_in, const char* preset_name)
{
    ysfx_bank_u bank{new ysfx_bank_t{}};
    bank->name = ysfx::strdup_using_new(bank_in->name);

    uint32_t found = ysfx_preset_exists(bank_in, preset_name);
    bank->preset_count = (uint32_t)(bank_in->preset_count);
    if (found > 0) bank->preset_count -= 1;

    bank->presets = new ysfx_preset_t[(uint32_t)bank->preset_count]{};
    uint32_t j = 0;
    for (uint32_t i=0; i < bank_in->preset_count; i++) {
        if (i != (found - 1)) {
            bank->presets[j].name = ysfx::strdup_using_new(bank_in->presets[i].name);
            bank->presets[j].blob_name = ysfx::strdup_using_new(bank_in->presets[i].blob_name);
            bank->presets[j].state = ysfx_state_dup(bank_in->presets[i].state);
            j += 1;
        }
    }

    return bank.release();
}

std::string double_string(double value) {
    std::ostringstream oss;
    oss.imbue(std::locale::classic()); // ensure the period is the decimal separator
    oss << std::fixed << std::setprecision(6) << value;

    std::string result{oss.str()};
    result.erase(result.find_last_not_of('0') + 1, std::string::npos);
    if (result.back() == '.') {
        result.pop_back();
    }
    result.push_back(' ');
    return result;
}

static std::string preset_blob(std::string blob_preset_name, ysfx_state_t *state)
{
    std::string blob{""};
    blob.reserve(4096);

    std::vector<ysfx_real> slider_values(ysfx_max_sliders, 0.0);
    std::vector<int> slider_used(ysfx_max_sliders, 0);

    bool more_than_64 = true;
    for (uint32_t i = 0; i < state->slider_count; i++) {
        uint32_t slider_index = state->sliders[i].index;
        slider_used[slider_index] = 1;
        slider_values[slider_index] = state->sliders[i].value;

        if (slider_index >= 64) more_than_64 = true;
    }

    // Serialize the first 64 sliders
    for (uint32_t i = 0; i < 64; i++) {
        if (slider_used[i]) {
            blob += double_string(slider_values[i]);
        } else {
            blob += "- ";
        }
    }

    // Print escaped name again
    blob += blob_preset_name + " ";

    // Serialize the remaining 192 sliders
    if (more_than_64) {
        for (uint32_t i = 0; i < 192; i++) {
            if (slider_used[i + 64]) {
                blob += double_string(slider_values[i + 64]);
            } else {
                blob += "- ";
            }
        }
    }
    blob = blob.substr(0, blob.length() - 1);  // Strip final space.

    // Terminate slider section with a null terminator
    blob += '\0';

    // Serialize the binary blob
    blob += std::string(reinterpret_cast<const char *>(state->data), state->data_size);

    const uint8_t *uint8_blob = reinterpret_cast<const uint8_t*>(&blob[0]);
    std::string base64_preset = ysfx::encode_base64(uint8_blob, blob.length());

    std::string preset_with_linebreaks{""};
    for (size_t i = 0; i < base64_preset.length(); i += 128) {
        preset_with_linebreaks += std::string("    ") + base64_preset.substr(i, 128) + std::string("\n");
    }

    return preset_with_linebreaks;
}

std::string ysfx_save_bank_to_rpl_text(ysfx_bank_t *bank)
{
    std::string rpl_text{"<REAPER_PRESET_LIBRARY " + escapeString(bank->name) + "\n"};

    for (uint32_t i = 0; i < bank->preset_count; i++) {
        ysfx_preset_t preset = bank->presets[i];
        std::string preset_name{preset.name};
        std::string blob_name{preset.blob_name};
        std::string presetString{"  <PRESET `" + preset_name + "`\n" + preset_blob(blob_name, preset.state) + "  >\n"};
        rpl_text += presetString;
    }

    rpl_text += ">\n";

    return rpl_text;
}

bool ysfx_save_bank(const char *path, ysfx_bank_t *bank)
{
#if defined(_WIN32)
    std::wstring wpath = ysfx::widen(path);
    ysfx::FILE_u stream{_wfopen(wpath.c_str(), L"wb")};
#else
    ysfx::FILE_u stream{fopen(path, "wb")};
#endif

    if (!stream) {
        return false;
    }

    std::string txt = ysfx_save_bank_to_rpl_text(bank);
    fwrite(txt.data(), 1, txt.length(), stream.get());
    
    if (ferror(stream.get())) {
        return false;
    }

    stream.reset();

    return true;
}

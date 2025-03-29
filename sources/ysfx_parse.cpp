// Copyright 2021 Jean Pierre Cimalando
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

#include "ysfx_parse.hpp"
#include "ysfx_utils.hpp"
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <vector>
#include <cctype>
#include <unordered_set>


ysfx_section_t* new_or_append(ysfx_section_u &section, uint32_t line_no)
{
    if (!section) {
        section.reset(new ysfx_section_t);
        section->line_offset = line_no + 1;
    } else {
        // We insert blank lines to ensure that the line numbers provided will be correct
        std::string::difference_type num_lines = std::count(section->text.begin(), section->text.end(), '\n');
        section->text.append(line_no - section->line_offset - num_lines + 1, '\n');
    }

    return section.get();
}

bool ysfx_parse_toplevel(ysfx::text_reader &reader, ysfx_toplevel_t &toplevel, ysfx_parse_error *error, bool onlyHeader=false)
{
    toplevel = ysfx_toplevel_t{};

    ysfx_section_t *current = new ysfx_section_t;
    toplevel.header.reset(current);

    std::string line;
    uint32_t lineno = 0;

    line.reserve(256);

    while (reader.read_next_line(line)) {
        const char *linep = line.c_str();

        if (linep[0] == '@') {
            if (onlyHeader) return true;

            // a new section starts
            ysfx::string_list tokens = ysfx::split_strings_noempty(linep, &ysfx::ascii_isspace);

            if (tokens[0] == "@init")
                current = new_or_append(toplevel.init, lineno);
            else if (tokens[0] == "@slider")
                current = new_or_append(toplevel.slider, lineno);
            else if (tokens[0] == "@block")
                current = new_or_append(toplevel.block, lineno);
            else if (tokens[0] == "@sample")
                current = new_or_append(toplevel.sample, lineno);
            else if (tokens[0] == "@serialize")
                current = new_or_append(toplevel.serialize, lineno);
            else if (tokens[0] == "@gfx") {
                current = new_or_append(toplevel.gfx, lineno);
                long gfx_w = 0;
                long gfx_h = 0;
                if (tokens.size() > 1)
                    gfx_w = (long)ysfx::dot_atof(tokens[1].c_str());
                if (tokens.size() > 2)
                    gfx_h = (long)ysfx::dot_atof(tokens[2].c_str());
                toplevel.gfx_w = (gfx_w > 0) ? (uint32_t)gfx_w : 0;
                toplevel.gfx_h = (gfx_h > 0) ? (uint32_t)gfx_h : 0;
            }
            else {
                if (error) {
                    error->line = lineno;
                    error->message = std::string("Invalid section: ") + line;
                }
                return false;
            }
        }
        else {
            current->text.append(line);
            current->text.push_back('\n');
        }

        ++lineno;
    }

    return true;
}

ysfx_config_item ysfx_parse_config_line(const char *rest)
{
    ysfx_config_item item;

    // Whitespace before the identifier is ignored
    auto cur = rest;
    while (*cur && ysfx::ascii_isspace(*cur)) ++cur;

    // Identifier
    auto pos = cur;
    while (*pos && !ysfx::ascii_isspace(*pos)) ++pos;
    item.identifier = std::string(cur, pos - cur);
    
    // Skip whitespace
    while (*pos && ysfx::ascii_isspace(*pos)) ++pos;
    if (!*pos) return item;
    cur = pos;
    
    auto closing_item = ((*pos == '"') || (*pos == '\'')) ? *pos : ' ';
    pos += 1;
    if (!*pos) return item;
    
    while (*pos && *pos != closing_item) ++pos;
    if (closing_item == '"') {
        item.name = std::string(cur + 1, pos - cur - 1);
    } else {
        item.name = std::string(cur, pos - cur);
    }
    
    cur = pos + 1;
    while (*cur && ysfx::ascii_isspace(*cur)) ++cur;  // Skip whitespace
        
    // Read double
    ysfx_real value = (ysfx_real)ysfx::dot_strtod(cur, (char **)&pos);
    item.default_value = value;
    if (cur == pos) return item;  // TODO: emit error

    auto key = std::string(cur, pos - cur);

    cur = pos + 1;
    while (*cur) {
        while (*cur && ysfx::ascii_isspace(*cur)) ++cur;  // Skip whitespace
        
        // Read double
        ysfx_real value = (ysfx_real)ysfx::dot_strtod(cur, (char **)&pos);
        if (cur == pos) return item;  // TODO: emit error

        auto key = std::string(cur, pos - cur);
        cur = pos;

        while (*cur && ysfx::ascii_isspace(*cur)) ++cur;  // Skip whitespace

        if (*cur == '=') {
            cur++;
            while (*cur && ysfx::ascii_isspace(*cur)) ++cur;  // Skip whitespace

            if (*cur) {
                closing_item = ((*cur == '"') || (*cur == '\'')) ? *cur : ' ';
                pos = cur + 1;
                while (*pos && (*pos != closing_item)) ++pos;
                switch(closing_item)
                {
                    case '"':
                        key = std::string(cur + 1, pos - cur - 1);
                        break;
                    case ' ':
                        key = std::string(cur, pos - cur);
                        break;
                    default:
                        key = std::string(cur, pos - cur + (*pos ? 1 : 0));  // blegh
                }
                cur = pos + (*pos ? 1 : 0);  // blegh x2
            }
        }
        
        item.var_names.push_back(key);
        item.var_values.push_back(value);
    }

    return item;
}

bool ysfx_config_item_is_valid(const ysfx_config_item& item) {
    if (item.identifier.size() < 2) return false;
    if (item.name.size() < 2) return false;
    if (item.var_names.size() < 2) return false;
    if (item.var_values.size() < 2) return false;
    if (item.var_names.size() != item.var_names.size()) return false;

    for (auto v : item.var_names) {
        if (v.empty()) return false;
    }

    return true;
}

bool ysfx_parse_header(ysfx_section_t *section, ysfx_header_t &header, ysfx_parse_error *error)
{
    header = ysfx_header_t{};

    ysfx::string_text_reader reader(section->text.c_str());

    std::string line;
    uint32_t lineno = section->line_offset;

    line.reserve(256);

    ///
    auto unprefix = [](const char *text, const char **restp, const char *prefix) -> bool {
        size_t len = strlen(prefix);
        if (strncmp(text, prefix, len))
            return false;
        if (restp)
            *restp = text + len;
        return true;
    };

    //--------------------------------------------------------------------------
    // pass 1: regular metadata

    std::unordered_set<std::string> config_identifiers{};
    while (reader.read_next_line(line)) {
        const char *linep = line.c_str();
        const char *rest = nullptr;

        ysfx_slider_t slider;
        ysfx_parsed_filename_t filename;

        ///
        if (unprefix(linep, &rest, "desc:")) {
            if (header.desc.empty())
                header.desc = ysfx::trim(rest, &ysfx::ascii_isspace);
        }
        else if (unprefix(linep, &rest, "author:")) {
            if (header.author.empty())
                header.author = ysfx::trim(rest, &ysfx::ascii_isspace);
        }
        else if (unprefix(linep, &rest, "tags:")) {
            if (header.tags.empty()) {
                for (const std::string &tag : ysfx::split_strings_noempty(rest, &ysfx::ascii_isspace))
                    header.tags.push_back(tag);
            }
        }
        else if (unprefix(linep, &rest, "in_pin:")) {
            header.explicit_pins = true;
            header.in_pins.push_back(ysfx::trim(rest, &ysfx::ascii_isspace));
        }
        else if (unprefix(linep, &rest, "out_pin:")) {
            header.explicit_pins = true;
            header.out_pins.push_back(ysfx::trim(rest, &ysfx::ascii_isspace));
        }
        else if (unprefix(linep, &rest, "config:")) {
            auto item = ysfx_parse_config_line(rest);
            if (ysfx_config_item_is_valid(item)) {
                std::string identifier{item.identifier};
                std::transform(identifier.begin(), identifier.end(), identifier.begin(), ysfx::ascii_tolower);

                if (config_identifiers.find(identifier) != config_identifiers.end()) {
                    if (error) {
                        error->line = lineno;
                        error->message = std::string("Duplicate config variable: ") + item.identifier;
                    }
                    return false;
                }
                config_identifiers.insert(identifier);
                header.config_items.push_back(item);
            }
        }
        else if (unprefix(linep, &rest, "options:")) {
            auto option_line = ysfx::trim_spaces_around_equals(rest);
            for (const std::string &opt : ysfx::split_strings_noempty(option_line.c_str(), &ysfx::ascii_isspace)) {
                size_t pos = opt.find('=');
                std::string name = (pos == opt.npos) ? opt : opt.substr(0, pos);
                std::string value = (pos == opt.npos) ? std::string{} : opt.substr(pos + 1);
                if (name == "gmem")
                    header.options.gmem = value;
                else if (name == "maxmem") {
                    int32_t maxmem = (int32_t)ysfx::dot_atof(value.c_str());
                    header.options.maxmem = (maxmem < 0) ? 0 : (uint32_t)maxmem;
                }
                else if (name == "prealloc") {
                    int32_t prealloc = (value.compare("*") == 0) ? -1 : (int32_t) ysfx::dot_atof(value.c_str());
                    header.options.prealloc = prealloc;
                }
                else if (name == "want_all_kb") {
                    header.options.want_all_kb = true;
                } else if (name == "no_meter") {
                    header.options.no_meter = true;
                } else if (name == "gfx_hz") {
                    int32_t gfx_hz = (int32_t) ysfx::dot_atof(value.c_str());
                    if (gfx_hz > 0 && gfx_hz < 2000) {
                        header.options.gfx_hz = static_cast<uint32_t>(gfx_hz);
                    }
                }
            }
        }
        else if (unprefix(linep, &rest, "import") && ysfx::ascii_isspace(rest[0]))
            header.imports.push_back(ysfx::trim(rest + 1, &ysfx::ascii_isspace));
        else if (ysfx_parse_slider(linep, slider)) {
            if (slider.id >= ysfx_max_sliders)
                continue;
            slider.exists = true;
            header.sliders[slider.id] = slider;
        }
        else if (ysfx_parse_filename(linep, filename)) {
            if (filename.index != header.filenames.size())
                continue;
            header.filenames.push_back(std::move(filename.filename));
        }

        ++lineno;
    }

    //--------------------------------------------------------------------------
    // pass 2: comments

    reader = ysfx::string_text_reader{section->text.c_str()};

    while (reader.read_next_line(line)) {
        const char *linep = line.c_str();
        const char *rest = nullptr;

        // some files contain metadata in the form of comments
        // this is not part of spec, but we'll take this info regardless

        if (unprefix(linep, &rest, "//author:")) {
            if (header.author.empty())
                header.author = ysfx::trim(rest, &ysfx::ascii_isspace);
        }
        else if (unprefix(linep, &rest, "//tags:")) {
            if (header.tags.empty()) {
                for (const std::string &tag : ysfx::split_strings_noempty(rest, &ysfx::ascii_isspace))
                    header.tags.push_back(tag);
            }
        }
    }

    //--------------------------------------------------------------------------
    if (header.in_pins.size() == 1 && !ysfx::ascii_casecmp(header.in_pins.front().c_str(), "none"))
        header.in_pins.clear();
    if (header.out_pins.size() == 1 && !ysfx::ascii_casecmp(header.out_pins.front().c_str(), "none"))
        header.out_pins.clear();

    if (header.in_pins.size() > ysfx_max_channels)
        header.in_pins.resize(ysfx_max_channels);
    if (header.out_pins.size() > ysfx_max_channels)
        header.out_pins.resize(ysfx_max_channels);

    return true;
}

bool ysfx_parse_slider(const char *line, ysfx_slider_t &slider)
{
    // NOTE this parser is intentionally very permissive,
    //   in order to match the reference behavior

    slider = ysfx_slider_t{};

    #define PARSE_FAIL do {                                                   \
        /*fprintf(stderr, "parse error (line %d): `%s`\n", __LINE__, line);*/ \
        return false;                                                         \
    } while (0)

    const char *cur = line;

    if (strnicmp("slider", cur, 6)) {
        PARSE_FAIL;
    }
    cur += 6;

    // parse ID (1-index)
    unsigned long id = strtoul(cur, (char **)&cur, 10);
    if (id < 1 || id > ysfx_max_sliders)
        PARSE_FAIL;
    slider.id = (uint32_t)--id;

    // semicolon
    if (*cur++ != ':')
        PARSE_FAIL;
    
    // Whitespace before the identifier is ignored
    while (*cur && ysfx::ascii_isspace(*cur)) ++cur;

    // search if there is an '=' sign prior to any '<' or ','
    // if there is, it's a custom variable
    {
        const char *pos = cur;
        for (char c; pos && (c = *pos) && c != '='; )
            pos = (c == '<' || c == ',') ? nullptr : (pos + 1);
        if (pos && *pos) {
            slider.var.assign(cur, pos);
            cur = pos + 1;
        }
        else
            slider.var = "slider" + std::to_string(id + 1);
    }

    // a regular slider
    if (*cur != '/') {
        slider.def = (ysfx_real)ysfx::dot_strtod(cur, (char **)&cur);

        while (*cur && *cur != ',' && *cur != '<') ++cur;
        if (!*cur) PARSE_FAIL;

        // no range specification
        if (*cur == ',')
            ++cur;
        // range specification
        else if (*cur == '<') {
            ++cur;

            // min
            slider.min = (ysfx_real)ysfx::dot_strtod(cur, (char **)&cur);

            while (*cur && *cur != ',' && *cur != '>') ++cur;
            if (!*cur) PARSE_FAIL;

            // max
            if (*cur == ',') {
                ++cur;
                slider.max = (ysfx_real)ysfx::dot_strtod(cur, (char **)&cur);

                while (*cur && *cur != ',' && *cur != '>') ++cur;
                if (!*cur) PARSE_FAIL;
            }

            // inc
            if (*cur == ',') {
                ++cur;
                slider.inc = (ysfx_real)ysfx::dot_strtod(cur, (char **)&cur);

                while (*cur && *cur != '{' && *cur != '>' && *cur != ':') ++cur;
                if (!*cur) PARSE_FAIL;

                // enumeration values
                if (*cur == '{') {
                    const char *pos = ++cur;

                    while (*cur && *cur != '}' && *cur != '>') ++cur;
                    if (!*cur) PARSE_FAIL;

                    slider.is_enum = true;
                    slider.enum_names = ysfx::split_strings_noempty(
                        std::string(pos, cur).c_str(),
                        [](char c) -> bool { return c == ','; });
                    for (std::string &name : slider.enum_names)
                        name = ysfx::trim(name.c_str(), &ysfx::ascii_isspace);
                }

                // shaping
                if (*cur == ':') {
                    ++cur;
                    if (strnicmp("log", cur, 3) == 0) {
                        slider.shape = 1;
                        cur += 3;
                    } else if (strnicmp("sqr", cur, 3) == 0) {
                        slider.shape = 2;
                        slider.shape_modifier = 2;
                        cur += 3;
                    }

                    // Do we have a modifier on this shape?
                    if (*cur == '=') {
                        ++cur;

                        slider.shape_modifier = (ysfx_real)ysfx::dot_strtod(cur, (char **)&cur);

                        // Do some checking on the modifiers for validity
                        if (std::abs(slider.shape_modifier) < 0.0001) {
                            if (slider.shape == 2) {
                                // A power law slider with power zero is invalid.
                                slider.shape = 0;
                            };
                        } else {
                            if (std::abs(slider.shape_modifier - slider.min) < 0.0000001) {
                                slider.shape = 0;
                            }
                        }
                        if (std::abs(slider.max - slider.min) < 1e-12) {
                            slider.shape = 0;
                        }

                        while (*cur && *cur != '>') ++cur;
                        if (!*cur) PARSE_FAIL;
                    }
                }
            }

            while (*cur && *cur != '>') ++cur;
            if (!*cur) PARSE_FAIL;

            ++cur;
        }
        else
            PARSE_FAIL;

        // NOTE: skip ',' and whitespace. not sure why, it's how it is
        while (*cur && (*cur == ',' || ysfx::ascii_isspace(*cur))) ++cur;
        if (!*cur) PARSE_FAIL;
    }
    // a path slider
    else
    {
        const char *pos = cur;
        while (*cur && *cur != ':') ++cur;
        if (!*cur) PARSE_FAIL;

        slider.path.assign(pos, cur);
        ++cur;
        slider.def = (ysfx_real)ysfx::dot_strtod(cur, (char **)&cur);
        slider.inc = 1;
        slider.is_enum = true;

        while (*cur && *cur != ':') ++cur;
        if (!*cur) PARSE_FAIL;

        ++cur;
    }

    // description and visibility
    while (ysfx::ascii_isspace(*cur))
        ++cur;

    slider.initially_visible = true;
    if (*cur == '-') {
        ++cur;
        slider.initially_visible = false;
    }

    slider.desc = ysfx::trim(cur, &ysfx::ascii_isspace);
    if (slider.desc.empty())
        PARSE_FAIL;

    //
    #undef PARSE_FAIL

    return true;
}

bool ysfx_parse_filename(const char *line, ysfx_parsed_filename_t &filename)
{
    filename = ysfx_parsed_filename_t{};

    const char *cur = line;

    for (const char *p = "filename:"; *p; ++p) {
        if (*cur++ != *p)
            return false;
    }

    int64_t index = (int64_t)ysfx::dot_strtod(cur, (char **)&cur);
    if (index < 0 || index > ~(uint32_t)0)
        return false;

    while (*cur && *cur != ',') ++cur;
    if (!*cur) return false;;

    ++cur;

    filename.index = (uint32_t)index;
    filename.filename.assign(cur);
    return true;
}

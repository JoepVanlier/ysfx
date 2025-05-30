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

#include "ysfx.hpp"
#include "ysfx_preprocess.hpp"
#include "ysfx_parse.hpp"
#include "ysfx_test_utils.hpp"
#include <catch.hpp>
#include <map>
#include <string>

TEST_CASE("preprocessor", "[basic]")
{
    SECTION("preprocessor ran correctly")
    {
        const char *text =
            "// the header" "\n"
            "@init" "\n"
            "<?c = 12; c += 1; printf(\"c = %d;\", c);?>" "\n"
            "@block" "\n";
        
        ysfx::string_text_reader raw_reader(text);

        ysfx_parse_error err;
        std::string processed_str;
        
        std::map<std::string, ysfx_real> preprocessor_values;
        REQUIRE(ysfx_preprocess(raw_reader, &err, processed_str, preprocessor_values));
        ysfx::string_text_reader processed_reader = ysfx::string_text_reader(processed_str.c_str());
        REQUIRE(!err);

        std::string line;
        processed_reader.read_next_line(line);
        REQUIRE(line == "// the header");
        processed_reader.read_next_line(line);
        REQUIRE(line == "@init");
        processed_reader.read_next_line(line);
        REQUIRE(line == "c = 13;");
        processed_reader.read_next_line(line);
        REQUIRE(line == "@block");
    }

    SECTION("preprocessor malformed preprocessor code")
    {
        const char *text =
            "// the header" "\n"
            "@init" "\n"
            "<?c = 1a2; c += 1; printf(\"c = %d;\", c);?>" "\n"
            "@block" "\n";
        
        ysfx::string_text_reader raw_reader(text);

        ysfx_parse_error err;
        std::string processed_str;
        std::map<std::string, ysfx_real> preprocessor_values;
        bool success = ysfx_preprocess(raw_reader, &err, processed_str, preprocessor_values);
        
        REQUIRE(!success);
        ysfx::string_text_reader processed_reader = ysfx::string_text_reader(processed_str.c_str());
        REQUIRE(err.message == "Invalid section: 3: preprocessor: syntax error: 'c = 1 <!> a2; c += 1; printf(\"c = %d;\", c);'");
    }

    SECTION("preprocessor with variable")
    {
        const char *text =
            "// the header" "\n"
            "@init" "\n"
            "<?printf(\"c = %d;\", preproc_value);?>" "\n"
            "@block" "\n";
        
        ysfx::string_text_reader raw_reader(text);

        ysfx_parse_error err;
        std::string processed_str;
        
        std::map<std::string, ysfx_real> preprocessor_values{std::make_pair("preproc_value", 42)};
        REQUIRE(ysfx_preprocess(raw_reader, &err, processed_str, preprocessor_values));
        ysfx::string_text_reader processed_reader = ysfx::string_text_reader(processed_str.c_str());
        REQUIRE(!err);

        std::string line;
        processed_reader.read_next_line(line);
        REQUIRE(line == "// the header");
        processed_reader.read_next_line(line);
        REQUIRE(line == "@init");
        processed_reader.read_next_line(line);
        REQUIRE(line == "c = 42;");
        processed_reader.read_next_line(line);
        REQUIRE(line == "@block");
    }
}

TEST_CASE("section splitting", "[parse]")
{
    SECTION("sections 1")
    {
        const char *text =
            "// the header" "\n"
            "@init" "\n"
            "the init" "\n"
            "@slider" "\n"
            "the slider, part 1" "\n"
            "the slider, part 2" "\n"
            "@block" "\n"
            "the block" "\n";
        ysfx::string_text_reader reader(text);

        ysfx_parse_error err;
        ysfx_toplevel_t toplevel;
        REQUIRE(ysfx_parse_toplevel(reader, toplevel, &err, false));
        REQUIRE(!err);

        REQUIRE(toplevel.header);
        REQUIRE(toplevel.init);
        REQUIRE(toplevel.slider);
        REQUIRE(toplevel.block);
        REQUIRE(!toplevel.sample);
        REQUIRE(!toplevel.serialize);
        REQUIRE(!toplevel.gfx);

        REQUIRE(toplevel.header->line_offset == 0);
        REQUIRE(toplevel.header->text == "// the header" "\n");
        REQUIRE(toplevel.init->line_offset == 2);
        REQUIRE(toplevel.init->text == "the init" "\n");
        REQUIRE(toplevel.slider->line_offset == 4);
        REQUIRE(toplevel.slider->text == "the slider, part 1" "\n" "the slider, part 2" "\n");
        REQUIRE(toplevel.block->line_offset == 7);
        REQUIRE(toplevel.block->text == "the block" "\n");
    }

    SECTION("sections 2")
    {
        const char *text =
            "// the header" "\n"
            "@sample" "\n"
            "the sample" "\n"
            "@serialize" "\n"
            "the serialize" "\n"
            "@gfx" "\n"
            "the gfx" "\n";
        ysfx::string_text_reader reader(text);

        ysfx_parse_error err;
        ysfx_toplevel_t toplevel;
        REQUIRE(ysfx_parse_toplevel(reader, toplevel, &err, false));
        REQUIRE(!err);

        REQUIRE(toplevel.header);
        REQUIRE(!toplevel.init);
        REQUIRE(!toplevel.slider);
        REQUIRE(!toplevel.block);
        REQUIRE(toplevel.sample);
        REQUIRE(toplevel.serialize);
        REQUIRE(toplevel.gfx);

        REQUIRE(toplevel.header->line_offset == 0);
        REQUIRE(toplevel.header->text == "// the header" "\n");
        REQUIRE(toplevel.sample->line_offset == 2);
        REQUIRE(toplevel.sample->text == "the sample" "\n");
        REQUIRE(toplevel.serialize->line_offset == 4);
        REQUIRE(toplevel.serialize->text == "the serialize" "\n");
        REQUIRE(toplevel.gfx->line_offset == 6);
        REQUIRE(toplevel.gfx->text == "the gfx" "\n");
    }

    SECTION("empty")
    {
        const char *text = "";
        ysfx::string_text_reader reader(text);

        ysfx_parse_error err;
        ysfx_toplevel_t toplevel;
        REQUIRE(ysfx_parse_toplevel(reader, toplevel, &err, false));
        REQUIRE(!err);

        // toplevel always has a header, empty or not

        REQUIRE(toplevel.header);
        REQUIRE(!toplevel.init);
        REQUIRE(!toplevel.slider);
        REQUIRE(!toplevel.block);
        REQUIRE(!toplevel.sample);
        REQUIRE(!toplevel.serialize);
        REQUIRE(!toplevel.gfx);

        REQUIRE(toplevel.header->line_offset == 0);
        REQUIRE(toplevel.header->text.empty());
    }

    SECTION("unrecognized section")
    {
        const char *text = "@abc";
        ysfx::string_text_reader reader(text);

        ysfx_parse_error err;
        ysfx_toplevel_t toplevel;
        REQUIRE(!ysfx_parse_toplevel(reader, toplevel, &err, false));
        REQUIRE(bool(err));
    }

    SECTION("trailing garbage")
    {
        const char *text = "@init zzz";
        ysfx::string_text_reader reader(text);

        ysfx_parse_error err;
        ysfx_toplevel_t toplevel;
        REQUIRE(ysfx_parse_toplevel(reader, toplevel, &err, false));
        REQUIRE(!err);

        REQUIRE(toplevel.init);
    }

    SECTION("gfx dimensions (default)")
    {
        const char *text = "@gfx";
        ysfx::string_text_reader reader(text);

        ysfx_parse_error err;
        ysfx_toplevel_t toplevel;
        REQUIRE(ysfx_parse_toplevel(reader, toplevel, &err, false));
        REQUIRE(!err);

        REQUIRE(toplevel.gfx);
        REQUIRE(toplevel.gfx_w == 0);
        REQUIRE(toplevel.gfx_h == 0);
    }

    SECTION("gfx dimensions (both)")
    {
        const char *text = "@gfx 123 456";
        ysfx::string_text_reader reader(text);

        ysfx_parse_error err;
        ysfx_toplevel_t toplevel;
        REQUIRE(ysfx_parse_toplevel(reader, toplevel, &err, false));
        REQUIRE(!err);

        REQUIRE(toplevel.gfx);
        REQUIRE(toplevel.gfx_w == 123);
        REQUIRE(toplevel.gfx_h == 456);
    }

    SECTION("gfx dimensions (just one)")
    {
        const char *text = "@gfx 123";
        ysfx::string_text_reader reader(text);

        ysfx_parse_error err;
        ysfx_toplevel_t toplevel;
        REQUIRE(ysfx_parse_toplevel(reader, toplevel, &err, false));
        REQUIRE(!err);

        REQUIRE(toplevel.gfx);
        REQUIRE(toplevel.gfx_w == 123);
        REQUIRE(toplevel.gfx_h == 0);
    }

    SECTION("gfx dimensions (garbage)")
    {
        const char *text = "@gfx aa bb cc";
        ysfx::string_text_reader reader(text);

        ysfx_parse_error err;
        ysfx_toplevel_t toplevel;
        REQUIRE(ysfx_parse_toplevel(reader, toplevel, &err, false));
        REQUIRE(!err);

        REQUIRE(toplevel.gfx);
        REQUIRE(toplevel.gfx_w == 0);
        REQUIRE(toplevel.gfx_h == 0);
    }

    SECTION("sections more init")
    {
        const char *text =
            "// the header" "\n"
            "@init" "\n"
            "the init" "\n"
            "@slider" "\n"
            "the slider, part 1" "\n"
            "the slider, part 2" "\n"
            "@block" "\n"
            "the block" "\n"
            "@init" "\n"
            "more init!" "\n"
            "@block" "\n"
            "more block" "\n"
            "@init" "\n"
            "more?" "\n";
        ysfx::string_text_reader reader(text);

        ysfx_parse_error err;
        ysfx_toplevel_t toplevel;
        REQUIRE(ysfx_parse_toplevel(reader, toplevel, &err, false));
        REQUIRE(!err);

        REQUIRE(toplevel.header);
        REQUIRE(toplevel.init);
        REQUIRE(toplevel.slider);
        REQUIRE(toplevel.block);
        REQUIRE(!toplevel.sample);
        REQUIRE(!toplevel.serialize);
        REQUIRE(!toplevel.gfx);

        REQUIRE(toplevel.header->line_offset == 0);
        REQUIRE(toplevel.header->text == "// the header" "\n");
        REQUIRE(toplevel.init->line_offset == 2);
        REQUIRE(toplevel.init->text == "the init" "\n" "\n" "\n" "\n" "\n" "\n" "\n" "more init!" "\n" "\n" "\n" "\n" "more?" "\n");
        REQUIRE(toplevel.slider->line_offset == 4);
        REQUIRE(toplevel.slider->text == "the slider, part 1" "\n" "the slider, part 2" "\n");
        REQUIRE(toplevel.block->line_offset == 7);
        REQUIRE(toplevel.block->text == "the block" "\n" "\n" "\n" "\n" "more block" "\n");
    }
}

//------------------------------------------------------------------------------
static void ensure_basic_slider(const ysfx_slider_t &slider, int32_t id, const std::string &var, const std::string &desc)
{
    if (id != -1)
        REQUIRE(slider.id == (uint32_t)id);
    if (!var.empty()) {
        if (var != "/*skip*/")
            REQUIRE(slider.var == var);
    }
    else if (id != -1)
        REQUIRE(slider.var == "slider" + std::to_string((uint32_t)id + 1));
    if (!desc.empty())
        REQUIRE(slider.desc == desc);
}

static void ensure_regular_slider(const ysfx_slider_t &slider, int32_t id, const std::string &var, const std::string &desc, ysfx_real def, ysfx_real min, ysfx_real max, ysfx_real inc, uint8_t shape=0, ysfx_real shape_modifier=0.0f)
{
    ensure_basic_slider(slider, id, var, desc);
    REQUIRE(slider.def == Approx(def));
    REQUIRE(slider.min == Approx(min));
    REQUIRE(slider.max == Approx(max));
    REQUIRE(slider.inc == Approx(inc));
    REQUIRE(!slider.is_enum);
    REQUIRE(slider.enum_names.empty());
    REQUIRE(slider.path.empty());
    REQUIRE(slider.shape == shape);
    REQUIRE(slider.shape_modifier == Approx(shape_modifier));
}

static void ensure_enum_slider(const ysfx_slider_t &slider, int32_t id, const std::string &var, const std::string &desc, ysfx_real def, const std::vector<std::string> &enums)
{
    ensure_basic_slider(slider, id, var, desc);
    REQUIRE(slider.def == Approx(def));
    REQUIRE(slider.min == 0);
    REQUIRE(slider.max == enums.size() - 1);
    REQUIRE(slider.inc == 1);
    REQUIRE(slider.is_enum);
    REQUIRE(slider.enum_names == enums);
    REQUIRE(slider.path.empty());
}

static void ensure_path_slider(const ysfx_slider_t &slider, int32_t id, const std::string &var, const std::string &desc, ysfx_real def, const std::string &path)
{
    ensure_basic_slider(slider, id, var, desc);
    REQUIRE(slider.def == Approx(def));
    REQUIRE(slider.min == 0);
    REQUIRE(slider.max == 0);
    REQUIRE(slider.inc == 1);
    REQUIRE(slider.is_enum);
    REQUIRE(slider.enum_names.empty());
    if (!path.empty())
        REQUIRE(slider.path == path);
    else
        REQUIRE(!slider.path.empty());
}

TEST_CASE("slider parsing", "[parse]")
{
    SECTION("minimal range syntax")
    {
        const char *line = "slider43:123,Cui cui";
        ysfx_slider_t slider;
        REQUIRE(ysfx_parse_slider(line, slider));
        ensure_regular_slider(slider, 42, {}, "Cui cui", 123, 0, 0, 0);
    }

    SECTION("slider 0 invalid")
    {
        const char *line = "slider0:123,Cui cui";
        ysfx_slider_t slider;
        REQUIRE(!ysfx_parse_slider(line, slider));
    }

    SECTION("normal range syntax (no min-max-inc, no enum)")
    {
        const char *line = "slider43:123.1,Cui cui";
        ysfx_slider_t slider;
        REQUIRE(ysfx_parse_slider(line, slider));
        ensure_regular_slider(slider, 42, {}, "Cui cui", 123.1, 0, 0, 0);
    }

    SECTION("normal range syntax (no min-max-inc (2), no enum)")
    {
        const char *line = "slider43:123.1<>,Cui cui";
        ysfx_slider_t slider;
        REQUIRE(ysfx_parse_slider(line, slider));
        ensure_regular_slider(slider, 42, {}, "Cui cui", 123.1, 0, 0, 0);
    }

    SECTION("normal range syntax (min-max-inc, no enum)")
    {
        const char *line = "slider43:123.1<45.2,67.3,89.4>Cui cui";
        ysfx_slider_t slider;
        REQUIRE(ysfx_parse_slider(line, slider));
        ensure_regular_slider(slider, 42, {}, "Cui cui", 123.1, 45.2, 67.3, 89.4);
    }

    SECTION("log shape")
    {
        const char *line = "slider43:20<20.0,22050,0.01:log>log me";
        ysfx_slider_t slider;
        REQUIRE(ysfx_parse_slider(line, slider));
        ensure_regular_slider(slider, 42, {}, "log me", 20, 20, 22050.0, 0.01, 1, 0);
    }

    SECTION("log shape middle")
    {
        const char *line = "slider43:20<20.0,22050,0.01:log=5000>log me";
        ysfx_slider_t slider;
        REQUIRE(ysfx_parse_slider(line, slider));
        ensure_regular_slider(slider, 42, {}, "log me", 20, 20, 22050.0, 0.01, 1, 5000);
    }

    SECTION("log shape middle permissive")
    {
        const char *line = "slider43:20<20.0,22050,0.01,-.,#+,@abcd:log=5000>log me";
        ysfx_slider_t slider;
        REQUIRE(ysfx_parse_slider(line, slider));
        ensure_regular_slider(slider, 42, {}, "log me", 20, 20, 22050.0, 0.01, 1, 5000);
    }

    SECTION("log shape middle even more permissive")
    {
        const char *line = "slider43:20<20.0,22050,0.01,-.,#+,@abcd:log=5000.#=1414?-+<,>log me";
        ysfx_slider_t slider;
        REQUIRE(ysfx_parse_slider(line, slider));
        ensure_regular_slider(slider, 42, {}, "log me", 20, 20, 22050.0, 0.01, 1, 5000);
    }

    SECTION("log shape capitalization")
    {
        const char *line = "slider43:20<20.0,22050,0.01:LOg>captains log";
        ysfx_slider_t slider;
        REQUIRE(ysfx_parse_slider(line, slider));
        ensure_regular_slider(slider, 42, {}, "captains log", 20, 20, 22050.0, 0.01, 1, 0);
    }

    SECTION("bad log shape (minimum too close to center point)")
    {
        const char *line = "slider43:20<20.0,22050,0.01:LOg=20>captains log";
        ysfx_slider_t slider;
        REQUIRE(ysfx_parse_slider(line, slider));
        ensure_regular_slider(slider, 42, {}, "captains log", 20, 20, 22050.0, 0.01, 0, 20);
    }

    SECTION("bad log shape (minimum too close to maximum)")
    {
        const char *line = "slider43:20<20.0,20.0,0.01:LOg=10>captains log";
        ysfx_slider_t slider;
        REQUIRE(ysfx_parse_slider(line, slider));
        ensure_regular_slider(slider, 42, {}, "captains log", 20, 20, 20.0, 0.01, 0, 10);
    }

    SECTION("sqr shape")
    {
        const char *line = "slider43:20<20.0,22050,0.01:sqr>square";
        ysfx_slider_t slider;
        REQUIRE(ysfx_parse_slider(line, slider));
        ensure_regular_slider(slider, 42, {}, "square", 20, 20, 22050.0, 0.01, 2, 2);
    }

    SECTION("sqr shape 3")
    {
        const char *line = "slider43:20<20.0,22050,0.01:sqr=3>square";
        ysfx_slider_t slider;
        REQUIRE(ysfx_parse_slider(line, slider));
        ensure_regular_slider(slider, 42, {}, "square", 20, 20, 22050.0, 0.01, 2, 3);
    }

    SECTION("invalid sqr shape (reverts to linear)")
    {
        // Modifier of zero leads to bad behavior and is therefore ignored entirely.
        const char *line = "slider43:20<20.0,22050,0.01:sqr=0>square";
        ysfx_slider_t slider;
        REQUIRE(ysfx_parse_slider(line, slider));
        ensure_regular_slider(slider, 42, {}, "square", 20, 20, 22050.0, 0.01, 0, 0);
    }

    SECTION("path syntax")
    {
        const char *line = "slider43:/titi:777:Cui cui";
        ysfx_slider_t slider;
        REQUIRE(ysfx_parse_slider(line, slider));
        ensure_path_slider(slider, 42, {}, "Cui cui", 777, "/titi");
    }

    SECTION("enum syntax")
    {
        const char *line = "slider5:0<0,2,1{LP,BP,HP}>Type";
        ysfx_slider_t slider;
        REQUIRE(ysfx_parse_slider(line, slider));
        ensure_enum_slider(slider, 4, {}, "Type", 0, {"LP", "BP", "HP"});
    }

    SECTION("enum syntax, permissive")
    {
        const char *line = "slider5:0<0,2,1<{LP,BP,HP}>Type";
        ysfx_slider_t slider;
        REQUIRE(ysfx_parse_slider(line, slider));
        ensure_enum_slider(slider, 4, {}, "Type", 0, {"LP", "BP", "HP"});
    }

    SECTION("misc")
    {
        for (const char *line : {
                "slider1:official=0<-150,12,1>official",
                "slider2:0<-150,12,1>official no var.name",
                "slider3:=0<-150,12,1>=value",
                "slider4:<-150,12,1>no default",
                "slider5:0<-150,12,1,,,>toomanycommas",
                "slider6:0<-150,12,1,2,3,4>toomanyvalues",
                "slider7:0time<-150kilo,12uhr,1euro>strings",
                "slider8:0*2<-150-151,12=13,1+3>math?",
                "slider9:+/-0a0<-150<<-149<,12...13,1 3><v<<al..u e>",
                "slider10:a1?+!%&<-150%&=/?+!,12!%/&?+=,1=/?+!%&>?+!%&=/",
                "SLIDER11:shouty=0<-150,12,1>shouty",
                "SlIdEr12:infantile=0<-150,12,1>hehe",
                "slider13: compRatio=0<-150,12,1> Ratio [x:1]",
                "slider14:  compRatio2=0<-150,12,1> Ratio [x:1]",
                "slider15:  all_the_spaces   = 0 < -150 , 12 , 1    > Ratio [x:1]",
            })
        {
            ysfx_slider_t slider;
            REQUIRE(ysfx_parse_slider(line, slider));
            ensure_regular_slider(slider, -1, "/*skip*/", {}, 0, -150, 12, 1);
        }
    }
}

static void validate_config_item(const char* line, const std::string id, const std::string name, const ysfx::string_list var_names, const std::vector<ysfx_real> var_values, const ysfx_real default_value) {
    ysfx_config_item item = ysfx_parse_config_line(line);
    
    REQUIRE(item.identifier == id);
    REQUIRE(item.name == name);

    for (auto ix = 0; ix < var_names.size(); ++ix) WARN(var_names[ix]);
    for (auto ix = 0; ix < item.var_names.size(); ++ix) WARN(item.var_names[ix]);
    REQUIRE(item.var_names.size() == var_names.size());
    REQUIRE(item.var_values.size() == var_values.size());
    REQUIRE(item.default_value == default_value);

    for (auto ix = 0; ix < var_names.size(); ++ix)
    {
        REQUIRE(item.var_names[ix] == var_names[ix]);
    }

    for (auto ix = 0; ix < var_values.size(); ++ix)
    {
        REQUIRE(item.var_values[ix] == var_values[ix]);
    }

    REQUIRE(ysfx_config_item_is_valid(item) == true);
}

static void check_invalid_config_line(const char* line) {
    ysfx_config_item item = ysfx_parse_config_line(line);
    REQUIRE(ysfx_config_item_is_valid(item) == false);
}

TEST_CASE("header parsing", "[parse]")
{
    SECTION("config", "config")
    {
        validate_config_item(
            " nch \"Channels\" 8 1 2 4 8=\"8 (namesake)\" 12 16 24 32 48",
            "nch",
            "Channels",
            {"1", "2", "4", "8 (namesake)", "12", "16", "24", "32", "48"},
            {1, 2, 4, 8, 12, 16, 24, 32, 48},
            8
        );

        validate_config_item(
            "nch \"Channels\" 8 1 2 4 8=\"8 (namesake)\" 12 16 24 32 48",
            "nch",
            "Channels",
            {"1", "2", "4", "8 (namesake)", "12", "16", "24", "32", "48"},
            {1, 2, 4, 8, 12, 16, 24, 32, 48},
            8
        );

        validate_config_item(
            "nch \"Channels\" 8 1 2 4 8='8 (namesake)' 12 16 24 32 48",
            "nch",
            "Channels",
            {"1", "2", "4", "'8 (namesake)'", "12", "16", "24", "32", "48"},
            {1, 2, 4, 8, 12, 16, 24, 32, 48},
            8
        );

        validate_config_item(
            "nch \"Channels\" 8 1 2 4 8='8 (namesake)\" 12 16 24 32 48",
            "nch",
            "Channels",
            {"1", "2", "4", "'8 (namesake)\" 12 16 24 32 48"},
            {1, 2, 4, 8},
            8
        );

        validate_config_item(
            "nch \"Channels\" 8 1 2 4 8='8 (name\"sake)' 12 16 24 32 48",
            "nch",
            "Channels",
            {"1", "2", "4", "'8 (name\"sake)'", "12", "16", "24", "32", "48"},
            {1, 2, 4, 8, 12, 16, 24, 32, 48},
            8
        );

        validate_config_item(
            "nch \"Channels\" 8 1 2 4 8 =   \"8 (namesake)\" 12 16 24 32 48",
            "nch",
            "Channels",
            {"1", "2", "4", "8 (namesake)", "12", "16", "24", "32", "48"},
            {1, 2, 4, 8, 12, 16, 24, 32, 48},
            8
        );

        validate_config_item(
            "nch \"Channels\" 8 1 2 4 8=\"8 (namesake)\" 12 16 24 32 48=",
            "nch",
            "Channels",
            {"1", "2", "4", "8 (namesake)", "12", "16", "24", "32", "48"},
            {1, 2, 4, 8, 12, 16, 24, 32, 48},
            8
        );

        validate_config_item(
            "nch \"Channels\" 8 1 2 4 8=\"8 (namesake)\" 12 16 24 32 48='blip'",
            "nch",
            "Channels",
            {"1", "2", "4", "8 (namesake)", "12", "16", "24", "32", "'blip'"},
            {1, 2, 4, 8, 12, 16, 24, 32, 48},
            8
        );

        validate_config_item(
            "nch \"Channels\" 8 1 2 4 8=\"8 (namesake)\" 12 16 24 32 48= blip",
            "nch",
            "Channels",
            {"1", "2", "4", "8 (namesake)", "12", "16", "24", "32", "blip"},
            {1, 2, 4, 8, 12, 16, 24, 32, 48},
            8
        );

        validate_config_item(
            "nch \"Channels\" 8 1 2 24 8=\"8 (namesake)\" 12 416 24 32 48=blip",
            "nch",
            "Channels",
            {"1", "2", "24", "8 (namesake)", "12", "416", "24", "32", "blip"},
            {1, 2, 24, 8, 12, 416, 24, 32, 48},
            8
        );

        validate_config_item(
            "nch \"Channels\" 8 1 2 4 8=\"8 (namesake)\" 12 16 24 32 48=\"blip",
            "nch",
            "Channels",
            {"1", "2", "4", "8 (namesake)", "12", "16", "24", "32", "blip"},
            {1, 2, 4, 8, 12, 16, 24, 32, 48},
            8
        );

        validate_config_item(
            "nch \"Channels\" 8 1 2=test 4 8=\"8 (namesake)\" 12 16 24 32 48='blip",
            "nch",
            "Channels",
            {"1", "test", "4", "8 (namesake)", "12", "16", "24", "32", "'blip"},
            {1, 2, 4, 8, 12, 16, 24, 32, 48},
            8
        );

        validate_config_item(
            "nch Channels 8 1 2 = test    4 8  =   \"8 (namesake)\"    12 16 24   32 48  = 'blip",
            "nch",
            "Channels",
            {"1", "test", "4", "8 (namesake)", "12", "16", "24", "32", "'blip"},
            {1, 2, 4, 8, 12, 16, 24, 32, 48},
            8
        );

        validate_config_item(
            "nch Channels 100 1 2 = test    4 8  =   \"8 (namesake)\"    12 14 24   32 48  = 'blip",
            "nch",
            "Channels",
            {"1", "test", "4", "8 (namesake)", "12", "14", "24", "32", "'blip"},
            {1, 2, 4, 8, 12, 14, 24, 32, 48},
            100
        );

        validate_config_item(
            "nch Channels 3 1 =5 2=",
            "nch",
            "Channels",
            {"5", "2"},
            {1, 2},
            3
        );

        check_invalid_config_line("nch Channels");
        check_invalid_config_line("nch ");
        check_invalid_config_line("");
        check_invalid_config_line("nch Channels 8");
        check_invalid_config_line("nch Channels ");
        check_invalid_config_line("nch Channels 8 1");  // At least two options are mandated by REAPER
        check_invalid_config_line("nch Channels 8 1 ");
        check_invalid_config_line("nch Channels 8 1 =5");
        check_invalid_config_line("nch Channels 8=\"test\" 1 2 3");   
    }

    SECTION("ordinary header", "[parse]")
    {
        const char *text =
            "desc:The desc" "\n"
            "in_pin:The input 1" "\n"
            "in_pin:The input 2" "\n"
            "out_pin:The output 1" "\n"
            "out_pin:The output 2" "\n"
            "slider43:123.1<45.2,67.3,89.4>Cui cui" "\n"
            "import foo.jsfx-inc" "\n";

        ysfx_section_t section;
        section.line_offset = 0;
        section.text.assign(text);

        ysfx_header_t header;
        ysfx_parse_header(&section, header, nullptr);
        REQUIRE(header.desc == "The desc");
        REQUIRE(header.in_pins.size() == 2);
        REQUIRE(header.in_pins[0] == "The input 1");
        REQUIRE(header.in_pins[1] == "The input 2");
        REQUIRE(header.out_pins.size() == 2);
        REQUIRE(header.out_pins[0] == "The output 1");
        REQUIRE(header.out_pins[1] == "The output 2");
        REQUIRE(header.sliders[42].exists);
        REQUIRE(header.imports.size() == 1);
        REQUIRE(header.imports[0] == "foo.jsfx-inc");
    }

    SECTION("explicit pins to none", "[parse]")
    {
        const char *text =
            "in_pin:none" "\n"
            "out_pin:none" "\n";

        ysfx_section_t section;
        section.line_offset = 0;
        section.text.assign(text);

        ysfx_header_t header;
        ysfx_parse_header(&section, header, nullptr);
        REQUIRE(header.in_pins.empty());
        REQUIRE(header.out_pins.empty());
    }

    SECTION("explicit pins to none, case sensitive", "[parse]")
    {
        const char *text =
            "in_pin:nOnE" "\n"
            "out_pin:NoNe" "\n";

        ysfx_section_t section;
        section.line_offset = 0;
        section.text.assign(text);

        ysfx_header_t header;
        ysfx_parse_header(&section, header, nullptr);
        REQUIRE(header.in_pins.empty());
        REQUIRE(header.out_pins.empty());
    }

    SECTION("multiple pins with none", "[parse]")
    {
        const char *text =
            "in_pin:none" "\n"
            "in_pin:Input" "\n"
            "out_pin:Output" "\n"
            "out_pin:none" "\n";

        ysfx_section_t section;
        section.line_offset = 0;
        section.text.assign(text);

        ysfx_header_t header;
        ysfx_parse_header(&section, header, nullptr);
        REQUIRE(header.in_pins.size() == 2);
        REQUIRE(header.in_pins[0] == "none");
        REQUIRE(header.in_pins[1] == "Input");
        REQUIRE(header.out_pins.size() == 2);
        REQUIRE(header.out_pins[0] == "Output");
        REQUIRE(header.out_pins[1] == "none");
    }

    SECTION("unspecified pins with @sample", "[parse]")
    {
        const char *text =
            "desc:Example" "\n"
            "@sample" "\n"
            "donothing();" "\n";

        scoped_new_dir dir_fx("${root}/Effects");
        scoped_new_txt file_main("${root}/Effects/example.jsfx", text);

        ysfx_config_u config{ysfx_config_new()};
        ysfx_u fx{ysfx_new(config.get())};

        REQUIRE(ysfx_load_file(fx.get(), file_main.m_path.c_str(), 0));

        ysfx_header_t &header = fx->source.main->header;
        REQUIRE(header.in_pins.size() == 2);
        REQUIRE(header.out_pins.size() == 2);
    }

    SECTION("unspecified pins without @sample", "[parse]")
    {
        const char *text =
            "desc:Example" "\n"
            "@block" "\n"
            "donothing();" "\n";

        scoped_new_dir dir_fx("${root}/Effects");
        scoped_new_txt file_main("${root}/Effects/example.jsfx", text);

        ysfx_config_u config{ysfx_config_new()};
        ysfx_u fx{ysfx_new(config.get())};

        REQUIRE(ysfx_load_file(fx.get(), file_main.m_path.c_str(), 0));

        ysfx_header_t &header = fx->source.main->header;
        REQUIRE(header.in_pins.size() == 0);
        REQUIRE(header.out_pins.size() == 0);
    }

    SECTION("filenames", "[parse]")
    {
        const char *text =
            "filename:0,toto" "\n"
            "filename:1,titi" "\n"
            "filename:2,tata" "\n";

        ysfx_section_t section;
        section.line_offset = 0;
        section.text.assign(text);

        ysfx_header_t header;
        ysfx_parse_header(&section, header, nullptr);
        REQUIRE(header.filenames.size() == 3);
        REQUIRE(header.filenames[0] == "toto");
        REQUIRE(header.filenames[1] == "titi");
        REQUIRE(header.filenames[2] == "tata");
    }

    SECTION("out-of-order filenames", "[parse]")
    {
        const char *text =
            "filename:0,toto" "\n"
            "filename:2,tata" "\n"
            "filename:1,titi" "\n";

        ysfx_section_t section;
        section.line_offset = 0;
        section.text.assign(text);

        ysfx_header_t header;
        ysfx_parse_header(&section, header, nullptr);
        REQUIRE(header.filenames.size() == 2);
        REQUIRE(header.filenames[0] == "toto");
        REQUIRE(header.filenames[1] == "titi");
    }
}

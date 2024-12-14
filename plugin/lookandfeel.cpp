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

#include "lookandfeel.h"
#include <juce_gui_extra/juce_gui_extra.h>


std::map<std::string, std::array<uint8_t, 3>> getDefaultColors()
{
    return std::map<std::string, std::array<uint8_t, 3>>{
        {"background", std::array<uint8_t, 3>{32, 32, 32}},
        {"element_background", std::array<uint8_t, 3>{16, 16, 16}},
        {"slider_fill", std::array<uint8_t, 3>{102, 102, 102}},
        {"slider_thumb", std::array<uint8_t, 3>{140, 150, 153}},
        {"off_fill", std::array<uint8_t, 3>{16, 16, 16}},
        {"selection_fill", std::array<uint8_t, 3>{65, 65, 65}},
        {"font_color", std::array<uint8_t, 3>{189, 189, 189}},
        {"font_color_light", std::array<uint8_t, 3>{210, 210, 210}},
        {"error", std::array<uint8_t, 3>{255, 204, 00}},
        {"comment", std::array<uint8_t, 3>{96, 128, 192}},
        {"builtin_variable", std::array<uint8_t, 3>{255, 128, 128}},
        {"builtin_function", std::array<uint8_t, 3>{255, 255, 48}},
        {"builtin_core_function", std::array<uint8_t, 3>{0, 192, 255}},
        {"builtin_section", std::array<uint8_t, 3>{0, 255, 255}},
        {"operator", std::array<uint8_t, 3>{0, 255, 255}},
        {"identifier", std::array<uint8_t, 3>{192, 192, 192}},
        {"integer", std::array<uint8_t, 3>{0, 255, 0}},
        {"float", std::array<uint8_t, 3>{0, 255, 0}},
        {"string", std::array<uint8_t, 3>{255, 192, 192}},
        {"bracket", std::array<uint8_t, 3>{192, 192, 255}},
        {"punctuation", std::array<uint8_t, 3>{0, 255, 255}},
        {"preprocessor_text", std::array<uint8_t, 3>{32, 192, 255}},
        {"string_hash", std::array<uint8_t, 3>{192, 255, 128}}
    };
}

std::map<std::string, float> getDefaultParams()
{
    return std::map<std::string, float>{
        {"vertical_pad", 5},
        {"left_pad", 3},
    };
}

void setParams(juce::LookAndFeel& lnf, std::map<std::string, float> params)
{
    auto get = [params](std::string key) {
        auto it = params.find(key); 
        jassert(it != params.end());  // This parameter doesn't have a default!

        if (it != params.end()) {
            return it->second;
        } else {
            return 1.0f;
        }
    };

    YsfxLookAndFeel& ysfx_lnf = dynamic_cast<YsfxLookAndFeel&>(lnf);
    ysfx_lnf.m_gap = static_cast<int>(get("vertical_pad"));
    ysfx_lnf.m_pad = static_cast<int>(get("left_pad"));
}

std::map<std::string, float> fillMissingParams(std::map<std::string, float> params) {
    std::map<std::string, float> currentParams = getDefaultParams();
    for (auto it = params.begin(); it != params.end(); ++it) {
        currentParams[it->first] = it->second;
    }

    return currentParams;
}

// Grabs a complete theme from a possibly incomplete theme
std::map<std::string, std::array<uint8_t, 3>> fillMissingColors(std::map<std::string, std::array<uint8_t, 3>> colormap)
{
    std::map<std::string, std::array<uint8_t, 3>> currentColorMap = getDefaultColors();
    for (auto it = colormap.begin(); it != colormap.end(); ++it) {
        currentColorMap[it->first] = it->second;
    }

    return currentColorMap;
}

void setColors(juce::LookAndFeel& lnf, std::map<std::string, std::array<uint8_t, 3>> colormap)
{
    auto get = [colormap](std::string key) {
        auto it = colormap.find(key); 
        jassert(it != colormap.end());  // This color doesn't have a default!

        if (it != colormap.end()) {
            return juce::Colour(int(it->second[0]), int(it->second[1]), int(it->second[2]));
        } else {
            return juce::Colour(255, 200, 200);
        } 
    };

    juce::Colour backgroundColour = get("background");
    juce::Colour elementBackgroundColour = get("element_background");

    juce::Colour sliderFillColour = get("slider_fill");
    juce::Colour thumbColour = get("slider_thumb");

    juce::Colour offFillColour = get("off_fill");
    juce::Colour selectionFillColour = get("selection_fill");

    juce::Colour fontColour = get("font_color");
    juce::Colour fontColourHighlight = get("font_color_light");

    lnf.setColour(juce::DocumentWindow::textColourId, fontColour);
    lnf.setColour(juce::TextButton::textColourOnId, fontColour);
    lnf.setColour(juce::TextButton::textColourOffId, fontColour);
    lnf.setColour(juce::ListBox::textColourId, fontColour);
    lnf.setColour(juce::PopupMenu::textColourId, fontColour);
    lnf.setColour(juce::Label::textColourId, fontColour);
    lnf.setColour(juce::PopupMenu::highlightedTextColourId, fontColourHighlight);
    lnf.setColour(juce::PopupMenu::headerTextColourId, fontColour);
    lnf.setColour(juce::ComboBox::textColourId, fontColour);

    lnf.setColour(juce::DocumentWindow::backgroundColourId, backgroundColour);
    lnf.setColour(juce::ComboBox::backgroundColourId, offFillColour);
    lnf.setColour(juce::ComboBox::buttonColourId, offFillColour);
    lnf.setColour(juce::PopupMenu::highlightedBackgroundColourId, selectionFillColour);

    lnf.setColour(juce::TextButton::buttonColourId, elementBackgroundColour);
    lnf.setColour(juce::TextButton::buttonDown, sliderFillColour);
    lnf.setColour(juce::TextButton::buttonOnColourId, selectionFillColour);
    lnf.setColour(juce::TextEditor::backgroundColourId, offFillColour);

    lnf.setColour(juce::ListBox::backgroundColourId, offFillColour);
    lnf.setColour(juce::ListBox::backgroundColourId, offFillColour);

    lnf.setColour(juce::ScrollBar::thumbColourId, thumbColour);
    lnf.setColour(juce::ScrollBar::trackColourId, sliderFillColour);
    lnf.setColour(juce::Slider::thumbColourId, thumbColour);
    lnf.setColour(juce::Slider::trackColourId, sliderFillColour);
    lnf.setColour(juce::Slider::backgroundColourId, offFillColour);

    lnf.setColour(juce::PopupMenu::backgroundColourId, offFillColour);
    lnf.setColour(juce::AlertWindow::backgroundColourId, offFillColour);

    lnf.setColour(juce::CodeEditorComponent::backgroundColourId, elementBackgroundColour);
    lnf.setColour(juce::CodeEditorComponent::defaultTextColourId, fontColour);
    lnf.setColour(juce::CodeEditorComponent::highlightColourId, selectionFillColour);
    lnf.setColour(juce::CodeEditorComponent::lineNumberBackgroundId, offFillColour);
    lnf.setColour(juce::CodeEditorComponent::lineNumberTextId, fontColour);

    lnf.setColour(juce::LookAndFeel_V4::ColourScheme::widgetBackground, elementBackgroundColour);

    auto& lnf4 = dynamic_cast<juce::LookAndFeel_V4&>(lnf);
    lnf4.getCurrentColourScheme().setUIColour(juce::LookAndFeel_V4::ColourScheme::widgetBackground, elementBackgroundColour);
}

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
        {"font_color_light", std::array<uint8_t, 3>{210, 210, 210}}
    };
}

void setColors(juce::LookAndFeel& lnf, std::map<std::string, std::array<uint8_t, 3>> colormap)
{
    std::map<std::string, std::array<uint8_t, 3>> currentColorMap = getDefaultColors();
    for (auto it = colormap.begin(); it != colormap.end(); ++it) {
        currentColorMap[it->first] = it->second;
    }

    auto get = [currentColorMap](std::string key) {
        auto it = currentColorMap.find(key); 
        jassert(it != currentColorMap.end());  // This color doesn't have a default!

        if (it != currentColorMap.end()) {
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
    lnf.setColour(juce::TextButton::buttonColourId, offFillColour);
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
}

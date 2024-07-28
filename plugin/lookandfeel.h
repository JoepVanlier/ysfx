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

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class YsfxLookAndFeel : public juce::LookAndFeel_V4 {
public:
    YsfxLookAndFeel()
    {
        juce::Colour white = juce::Colours::white;
        juce::Colour black = juce::Colours::black;
        juce::Colour grayBg = juce::Colour(32, 32, 32);
        juce::Colour grayElementBg = juce::Colour(16, 16, 16);

        juce::Colour c1 = juce::Colour(255, 255, 255);
        juce::Colour c2 = juce::Colour(222, 222, 222);

        setColour(juce::DocumentWindow::backgroundColourId, grayBg);
        setColour(juce::ComboBox::backgroundColourId, grayElementBg);
        setColour(juce::ComboBox::buttonColourId, grayElementBg);

        setColour(juce::TextButton::buttonColourId, grayElementBg);
        setColour(juce::TextEditor::backgroundColourId, grayElementBg);

        setColour(juce::ListBox::backgroundColourId, grayElementBg);
        setColour(juce::ListBox::backgroundColourId, grayElementBg);

        setColour(juce::ScrollBar::thumbColourId, c2);
        setColour(juce::ScrollBar::trackColourId, c1);
        setColour(juce::Slider::thumbColourId, c2);
        setColour(juce::Slider::trackColourId, c1);
        setColour(juce::Slider::backgroundColourId, grayElementBg);

        setColour(juce::PopupMenu::backgroundColourId, grayElementBg);

        setColour(juce::AlertWindow::backgroundColourId, juce::Colour(23, 23, 14));
        
        auto &scheme = getCurrentColourScheme();
        scheme.setUIColour(ColourScheme::widgetBackground, juce::Colour(16, 16, 16));
    }
};

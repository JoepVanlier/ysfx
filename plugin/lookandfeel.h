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


void setColors(juce::LookAndFeel& lnf, std::map<std::string, std::array<uint8_t, 3>> colormap);
std::map<std::string, std::array<uint8_t, 3>> getDefaultColors();


class YsfxLookAndFeel : public juce::LookAndFeel_V4 {
public:
    YsfxLookAndFeel()
    {
        setColors(*this, {});
    }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float minSliderPos, float maxSliderPos, const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (slider.isBar())
        {
            g.setColour (slider.findColour (juce::Slider::trackColourId));
            g.fillRect (slider.isHorizontal() ? juce::Rectangle<float> (static_cast<float> (x), (float) y + 0.5f, sliderPos - (float) x, (float) height - 1.0f)
                                            : juce::Rectangle<float> ((float) x + 0.5f, sliderPos, (float) width - 1.0f, (float) y + ((float) height - sliderPos)));

            drawLinearSliderOutline (g, x, y, width, height, style, slider);
        }
        else
        {
            auto isTwoVal   = (style == juce::Slider::SliderStyle::TwoValueVertical   || style == juce::Slider::SliderStyle::TwoValueHorizontal);
            auto isThreeVal = (style == juce::Slider::SliderStyle::ThreeValueVertical || style == juce::Slider::SliderStyle::ThreeValueHorizontal);

            auto trackWidth = juce::jmin (6.0f, slider.isHorizontal() ? (float) height * 0.25f : (float) width * 0.25f) + 2;

            juce::Point<float> startPoint (slider.isHorizontal() ? (float) x : (float) x + (float) width * 0.5f,
                                    slider.isHorizontal() ? (float) y + (float) height * 0.5f : (float) (height + y));

            juce::Point<float> endPoint (slider.isHorizontal() ? (float) (width + x) : startPoint.x,
                                slider.isHorizontal() ? startPoint.y : (float) y);

            juce::Path backgroundTrack;
            backgroundTrack.startNewSubPath (startPoint);
            backgroundTrack.lineTo (endPoint);
            g.setColour (slider.findColour (juce::Slider::backgroundColourId));
            g.setGradientFill(juce::ColourGradient(slider.findColour(juce::Slider::backgroundColourId), (float) x, (float) y - 10, juce::Colour(255, 255, 255), (float) x, (float) y + 650, false));

            g.strokePath (backgroundTrack, { trackWidth + 4, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });

            juce::Path valueTrack;
            juce::Point<float> minPoint, maxPoint, thumbPoint;

            if (isTwoVal || isThreeVal)
            {
                minPoint = { slider.isHorizontal() ? minSliderPos : (float) width * 0.5f,
                            slider.isHorizontal() ? (float) height * 0.5f : minSliderPos };

                if (isThreeVal)
                    thumbPoint = { slider.isHorizontal() ? sliderPos : (float) width * 0.5f,
                                slider.isHorizontal() ? (float) height * 0.5f : sliderPos };

                maxPoint = { slider.isHorizontal() ? maxSliderPos : (float) width * 0.5f,
                            slider.isHorizontal() ? (float) height * 0.5f : maxSliderPos };
            }
            else
            {
                auto kx = slider.isHorizontal() ? sliderPos : ((float) x + (float) width * 0.5f);
                auto ky = slider.isHorizontal() ? ((float) y + (float) height * 0.5f) : sliderPos;

                minPoint = startPoint;
                maxPoint = { kx, ky };
            }

            auto thumbWidth = getSliderThumbRadius (slider);

            valueTrack.startNewSubPath (minPoint);
            valueTrack.lineTo (isThreeVal ? thumbPoint : maxPoint);
            g.setColour (slider.findColour (juce::Label::textColourId));
            g.strokePath (valueTrack, { trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });

            g.setColour (slider.findColour (juce::Slider::trackColourId));
            g.strokePath (valueTrack, { trackWidth-2, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });

            if (! isTwoVal)
            {
                g.setColour (slider.findColour (juce::Label::textColourId));
                g.fillEllipse (juce::Rectangle<float> (static_cast<float> (thumbWidth), static_cast<float> (thumbWidth)).withCentre (isThreeVal ? thumbPoint : maxPoint));

                g.setColour (slider.findColour (juce::Slider::thumbColourId));
                g.fillEllipse (juce::Rectangle<float> (static_cast<float> (thumbWidth - 2), static_cast<float> (thumbWidth - 2)).withCentre (isThreeVal ? thumbPoint : maxPoint));
            }

            if (isTwoVal || isThreeVal)
            {
                auto sr = juce::jmin (trackWidth, (slider.isHorizontal() ? (float) height : (float) width) * 0.4f);
                auto pointerColour = slider.findColour (juce::Slider::thumbColourId);

                if (slider.isHorizontal())
                {
                    drawPointer (g, minSliderPos - sr,
                                juce::jmax (0.0f, (float) y + (float) height * 0.5f - trackWidth * 2.0f),
                                trackWidth * 2.0f, pointerColour, 2);

                    drawPointer (g, maxSliderPos - trackWidth,
                                juce::jmin ((float) (y + height) - trackWidth * 2.0f, (float) y + (float) height * 0.5f),
                                trackWidth * 2.0f, pointerColour, 4);
                }
                else
                {
                    drawPointer (g, juce::jmax (0.0f, (float) x + (float) width * 0.5f - trackWidth * 2.0f),
                                minSliderPos - trackWidth,
                                trackWidth * 2.0f, pointerColour, 1);

                    drawPointer (g, juce::jmin ((float) (x + width) - trackWidth * 2.0f, (float) x + (float) width * 0.5f), maxSliderPos - sr,
                                trackWidth * 2.0f, pointerColour, 3);
                }
            }

            if (slider.isBar())
                drawLinearSliderOutline (g, x, y, width, height, style, slider);
        }
    }
};

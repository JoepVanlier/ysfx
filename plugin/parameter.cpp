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

#include "parameter.h"
#include <cmath>

YsfxParameter::YsfxParameter(ysfx_t *fx, int sliderIndex)
    : RangedAudioParameter(
        "slider" + juce::String(sliderIndex + 1),
        "Slider " + juce::String(sliderIndex + 1)),
      m_sliderIndex(sliderIndex), m_displayName("Slider " + juce::String(sliderIndex + 1))
{
    setEffect(fx);
}

void YsfxParameter::setEffect(ysfx_t *fx)
{
    if (m_fx.get() == fx)
        return;

    m_fx.reset(fx);
    if (fx) {
        ysfx_add_ref(fx);
        {
            juce::ScopedLock nameLock(m_nameSection);
            m_displayName = juce::String(ysfx_slider_get_name(fx, static_cast<uint32_t>(m_sliderIndex)));
        }
    }
}

bool YsfxParameter::existsAsSlider() const
{
    return ysfx_slider_exists(m_fx.get(), (uint32_t)m_sliderIndex);
}

juce::CharPointer_UTF8 YsfxParameter::getSliderName() const
{
    return juce::CharPointer_UTF8{ysfx_slider_get_name(m_fx.get(), (uint32_t)m_sliderIndex)};
}

ysfx_slider_range_t YsfxParameter::getSliderRange() const
{
    ysfx_slider_range_t range{};
    ysfx_slider_get_range(m_fx.get(), (uint32_t)m_sliderIndex, &range);
    return range;
}

ysfx_slider_curve_t YsfxParameter::getSliderCurve() const
{
    ysfx_slider_curve_t curve{};
    ysfx_slider_get_curve(m_fx.get(), (uint32_t)m_sliderIndex, &curve);
    return curve;
}

bool YsfxParameter::isEnumSlider() const
{
    return ysfx_slider_is_enum(m_fx.get(), (uint32_t)m_sliderIndex);
}

int YsfxParameter::getSliderEnumSize() const
{
    return (int)ysfx_slider_get_enum_size(m_fx.get(), (uint32_t)m_sliderIndex);
}

juce::CharPointer_UTF8 YsfxParameter::getSliderEnumName(int index) const
{
    return juce::CharPointer_UTF8{ysfx_slider_get_enum_name(m_fx.get(), (uint32_t)m_sliderIndex, (uint32_t)index)};
}

ysfx_real YsfxParameter::convertToYsfxValue(float normValue) const
{
    ysfx_slider_curve_t curve = getSliderCurve();

    ysfx_real actualValue{ysfx_normalized_to_ysfx_value(normValue, &curve)};

    // NOTE: if enumerated, round the value to nearest,
    //    to make sure imprecision does not land us on the wrong index
    if (isEnumSlider())
        actualValue = juce::roundToInt(actualValue);

    return actualValue;
}

float YsfxParameter::convertFromYsfxValue(ysfx_real actualValue) const
{
    ysfx_slider_curve_t curve = getSliderCurve();
    if (std::abs(curve.max - curve.min) < 1e-12)
        return 0.0f;
    
    // NOTE: if enumerated, round value into an index
    if (isEnumSlider())
        actualValue = juce::roundToInt(actualValue);

    ysfx_real normalized = ysfx_ysfx_value_to_normalized(actualValue, &curve);

    return static_cast<float>(normalized);
}

float YsfxParameter::getValue() const
{
    return m_value;
}

void YsfxParameter::setValue(float newValue)
{
    m_value = newValue;
    m_hostUpdated = true;
}

void YsfxParameter::setValueNoNotify(float newValue)
{
    m_value = newValue;
}

float YsfxParameter::getDefaultValue() const
{
    return 0.0f;
}

juce::String YsfxParameter::getName(int maximumStringLength) const
{
    juce::ScopedLock nameLock(m_nameSection);
    return m_displayName.substring(0, maximumStringLength);
}

bool YsfxParameter::wasUpdatedByHost() {
    if (m_hostUpdated) {
        m_hostUpdated = false;
        return true;
    } else {
        return false;
    }
}

juce::String YsfxParameter::getText(float normalisedValue, int) const
{
    ysfx_real actualValue = convertToYsfxValue(normalisedValue);

    if (isEnumSlider()) {
        int enumSize = getSliderEnumSize();
        // NOTE: if enumerated, round the value to nearest,
        //    to make sure imprecision does not land us on the wrong index
        int index = juce::roundToInt(actualValue);
        if (index >= 0 && index < enumSize)
            return getSliderEnumName(index);
    } else {
        // NOTE: Unfortunately, things have to map to 0-1 so you lose some precision 
        // coming back (and can't rely on integer floats being exact anymore).
        auto curve = getSliderCurve();
        auto rounded = std::round((actualValue - curve.min) / curve.inc) * curve.inc + curve.min;

        if (std::abs(rounded - actualValue) < 0.00001) {
            actualValue = rounded > -curve.inc ? abs(rounded) : rounded;
        }
    }

    return juce::String(actualValue);
}

float YsfxParameter::getValueForText(const juce::String &text) const
{
    ysfx_real actualValue{};

    bool foundEnum = false;
    if (isEnumSlider()) {
        int enumSize = getSliderEnumSize();
        for (int i = 0; !foundEnum && i < enumSize; ++i) {
            foundEnum = text == getSliderEnumName(i);
            if (foundEnum)
                actualValue = i;
        }
    }
    if (!foundEnum)
        actualValue = text.getFloatValue();

    return convertFromYsfxValue(actualValue);
}

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
#include "parameters_panel.h"
#include "../parameter.h"

#define LOOKANDFEEL dynamic_cast<YsfxLookAndFeel&>(this->getLookAndFeel())

class YsfxParameterListener : private YsfxParameter::Listener,
                          private juce::Timer {
public:
    explicit YsfxParameterListener(YsfxParameter &param)
        : parameter(param)
    {
        parameter.addListener(this);

        startTimer(100);
    }

    ~YsfxParameterListener() override
    {
        parameter.removeListener(this);
    }

    YsfxParameter &getParameter() const noexcept
    {
        return parameter;
    }

    virtual void handleNewParameterValue() = 0;

private:
    //==============================================================================
    void parameterValueChanged(int, float) override
    {
        parameterValueHasChanged = 1;
    }

    void parameterGestureChanged(int, bool) override
    {
    }

    //==============================================================================
    void timerCallback() override
    {
        if (parameterValueHasChanged.compareAndSetBool(0, 1)) {
            handleNewParameterValue();
            startTimerHz(50);
        }
        else {
            startTimer(juce::jmin(250, getTimerInterval() + 10));
        }
    }

    YsfxParameter &parameter;
    juce::Atomic<int> parameterValueHasChanged{0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(YsfxParameterListener)
};

//==============================================================================
class YsfxBooleanParameterComponent final : public juce::Component, private YsfxParameterListener {
public:
    explicit YsfxBooleanParameterComponent(YsfxParameter &param)
        : YsfxParameterListener(param)
    {
        // Set the initial value.
        handleNewParameterValue();

        button.onClick = [this] { buttonClicked(); };

        addAndMakeVisible(button);
    }

    void paint(juce::Graphics &) override
    {
    }

    void resized() override
    {
        auto area = getLocalBounds();
        area.removeFromLeft(LOOKANDFEEL.m_pad + 2);
        button.setBounds(area.reduced(LOOKANDFEEL.m_pad, LOOKANDFEEL.m_gap));
    }

private:
    void handleNewParameterValue() override
    {
        button.setToggleState(isParameterOn(), juce::dontSendNotification);
    }

    void buttonClicked()
    {
        if (isParameterOn() != button.getToggleState()) {
            getParameter().beginChangeGesture();
            getParameter().setValueNotifyingHost(button.getToggleState() ? 1.0f : 0.0f);
            getParameter().endChangeGesture();
        }
    }

    bool isParameterOn() const
    {
        return getParameter().getValue() != 0.0f;
    }

    juce::ToggleButton button;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(YsfxBooleanParameterComponent)
};

//==============================================================================
class YsfxSwitchParameterComponent final : public juce::Component, private YsfxParameterListener {
public:
    explicit YsfxSwitchParameterComponent(YsfxParameter &param)
        : YsfxParameterListener(param)
    {
        for (auto &button : buttons) {
            button.setRadioGroupId(293847);
            button.setClickingTogglesState(true);
        }

        buttons[0].setButtonText(getParameter().getText(0.0f, 16));
        buttons[1].setButtonText(getParameter().getText(1.0f, 16));

        buttons[0].setConnectedEdges(juce::Button::ConnectedOnRight);
        buttons[1].setConnectedEdges(juce::Button::ConnectedOnLeft);

        // Set the initial value.
        buttons[0].setToggleState(true, juce::dontSendNotification);
        handleNewParameterValue();

        buttons[1].onStateChange = [this] { rightButtonChanged(); };

        for (auto &button : buttons)
            addAndMakeVisible(button);
    }

    void paint(juce::Graphics &) override
    {
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(LOOKANDFEEL.m_pad, LOOKANDFEEL.m_gap);
        area.removeFromLeft(LOOKANDFEEL.m_pad + 2);

        for (auto &button : buttons)
            button.setBounds(area.removeFromLeft(80));
    }

private:
    void handleNewParameterValue() override
    {
        bool newState = isParameterOn();

        if (buttons[1].getToggleState() != newState) {
            buttons[1].setToggleState(newState, juce::dontSendNotification);
            buttons[0].setToggleState(!newState, juce::dontSendNotification);
        }
    }

    void rightButtonChanged()
    {
        auto buttonState = buttons[1].getToggleState();

        if (isParameterOn() != buttonState) {
            getParameter().beginChangeGesture();
            getParameter().setValueNotifyingHost(buttonState ? 1.0f : 0.0f);
            getParameter().endChangeGesture();
        }
    }

    bool isParameterOn() const
    {
        return getParameter().getValue() != 0.0f;
    }

    juce::TextButton buttons[2];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(YsfxSwitchParameterComponent)
};

//==============================================================================
class YsfxChoiceParameterComponent final : public juce::Component, private YsfxParameterListener {
public:
    explicit YsfxChoiceParameterComponent(YsfxParameter &param)
        : YsfxParameterListener(param)
    {
        int enumSize = (int)param.getSliderEnumSize();
        for (int i = 0; i < enumSize; ++i)
            box.addItem(param.getSliderEnumName(i), i + 1);

        // Set the initial value.
        handleNewParameterValue();

        box.onChange = [this] { boxChanged(); };
        addAndMakeVisible(box);
    }

    void paint(juce::Graphics &) override
    {
    }

    void resized() override
    {
        auto area = getLocalBounds();
        area.removeFromLeft(LOOKANDFEEL.m_pad + 2);
        box.setBounds(area.reduced(LOOKANDFEEL.m_pad, LOOKANDFEEL.m_gap));
    }

private:
    void handleNewParameterValue() override
    {
        int index = -1;

        juce::String valueText = getParameter().getCurrentValueAsText();
        int enumSize = getParameter().getSliderEnumSize();

        for (int i = 0; index == -1 && i < enumSize; ++i) {
            if (valueText == getParameter().getSliderEnumName(i))
                index = i;
        }

        if (index < 0) {
            // The parameter is producing some unexpected text, so we'll do
            // some linear interpolation.
            index = juce::roundToInt(getParameter().getValue() *
                               (float)(enumSize - 1));
        }

        box.setSelectedItemIndex(index);
    }

    void boxChanged()
    {
        if (getParameter().getCurrentValueAsText() != box.getText()) {
            getParameter().beginChangeGesture();

            // When a parameter provides a list of strings we must set its
            // value using those strings, rather than a float, because VSTs can
            // have uneven spacing between the different allowed values.
            getParameter().setValueNotifyingHost(
                getParameter().getValueForText(box.getText()));

            getParameter().endChangeGesture();
        }
    }

    juce::ComboBox box;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(YsfxChoiceParameterComponent)
};

//==============================================================================
class YsfxSliderParameterComponent final : public juce::Component, private YsfxParameterListener {
public:
    explicit YsfxSliderParameterComponent(YsfxParameter &param)
        : YsfxParameterListener(param)
    {
        ysfx_slider_range_t range = getParameter().getSliderRange();

        if (range.inc != 0 && range.min != range.max)
            slider.setRange(0.0, 1.0, std::abs(range.inc / (range.max - range.min)));
        else
            slider.setRange(0.0, 1.0);

        slider.setDoubleClickReturnValue(true, param.convertFromYsfxValue(range.def));

        slider.setScrollWheelEnabled(false);
        addAndMakeVisible(slider);

        valueLabel.setColour(juce::Label::outlineColourId,
                             slider.findColour(juce::Slider::textBoxOutlineColourId));
        valueLabel.setBorderSize({1, 1, 1, 1});
        valueLabel.setJustificationType(juce::Justification::centred);
        valueLabel.setEditable(true);
        addAndMakeVisible(valueLabel);

        // Set the initial value.
        handleNewParameterValue();

        slider.onValueChange = [this] { sliderValueChanged(); };
        slider.onDragStart = [this] { sliderStartedDragging(); };
        slider.onDragEnd = [this] { sliderStoppedDragging(); };
        valueLabel.onTextChange = [this] { labelValueChanged(); };
    }

    void paint(juce::Graphics &) override
    {
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(LOOKANDFEEL.m_pad, LOOKANDFEEL.m_gap);

        valueLabel.setBounds(area.removeFromRight(80));

        //area.removeFromLeft(LOOKANDFEEL.m_pad);
        slider.setBounds(area.withTrimmedRight(15));
    }

private:
    void updateTextDisplay()
    {
        valueLabel.setText(getParameter().getCurrentValueAsText(), juce::dontSendNotification);
    }

    void handleNewParameterValue() override
    {
        if (!isDragging) {
            slider.setValue(getParameter().getValue(), juce::dontSendNotification);
            updateTextDisplay();
        }
    }

    void sliderValueChanged()
    {
        auto newVal = (float)slider.getValue();

        if (getParameter().getValue() != newVal) {
            if (!isDragging)
                getParameter().beginChangeGesture();

            getParameter().setValueNotifyingHost((float)slider.getValue());
            updateTextDisplay();

            if (!isDragging)
                getParameter().endChangeGesture();
        }
    }

    void sliderStartedDragging()
    {
        isDragging = true;
        getParameter().beginChangeGesture();
    }

    void sliderStoppedDragging()
    {
        isDragging = false;
        getParameter().endChangeGesture();
    }

    void labelValueChanged()
    {
        juce::String textValue{valueLabel.getText()};
        const auto charptr = textValue.getCharPointer();
        auto ptr = charptr;
        auto newVal = juce::CharacterFunctions::readDoubleValue(ptr);
        size_t chars_read = ptr - charptr;
        
        if (chars_read == textValue.getNumBytesAsUTF8()) {
            if (getParameter().getValue() != newVal) {
                getParameter().setValueNotifyingHost(getParameter().convertFromYsfxValue(newVal));
            }
        } else {
            updateTextDisplay();
        }
    }

    juce::Slider slider{juce::Slider::LinearHorizontal, juce::Slider::TextEntryBoxPosition::NoTextBox};
    juce::Label valueLabel;
    bool isDragging = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(YsfxSliderParameterComponent)
};

//==============================================================================
class YsfxParameterDisplayComponent : public juce::Component {
public:
    explicit YsfxParameterDisplayComponent(YsfxParameter &param)
        : parameter(param)
    {
        parameterName.setText(parameter.getSliderName(), juce::dontSendNotification);
        parameterName.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(parameterName);
        addAndMakeVisible(*(parameterComp = createParameterComp()));

        setSize(400, 20 + 2 * LOOKANDFEEL.m_gap);
    }

    void paint(juce::Graphics &) override
    {
    }

    void resized() override
    {
        auto area = getLocalBounds().withTrimmedRight(10);

        parameterName.setBounds(area.removeFromLeft(200 - std::max(0, 400 - area.getWidth())));
        parameterComp->setBounds(area);
    }

private:
    YsfxParameter &parameter;
    juce::Label parameterName;
    std::unique_ptr<Component> parameterComp;

    std::unique_ptr<Component> createParameterComp() const
    {
        ysfx_slider_range_t range = parameter.getSliderRange();
        bool isEnum = parameter.isEnumSlider();

        if (isEnum) {
            jassert(range.min == 0);
            jassert(range.inc == 1);
            if (range.max == 1)
                return std::make_unique<YsfxSwitchParameterComponent>(parameter);
            else
                return std::make_unique<YsfxChoiceParameterComponent>(parameter);
        }

        if (range.min == 0 && range.max == 1 && range.inc == 1)
            return std::make_unique<YsfxBooleanParameterComponent>(parameter);

        return std::make_unique<YsfxSliderParameterComponent>(parameter);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(YsfxParameterDisplayComponent)
};

//==============================================================================
YsfxParametersPanel::YsfxParametersPanel()
{
}

YsfxParametersPanel::~YsfxParametersPanel()
{
    paramComponents.clear();
}

void YsfxParametersPanel::setParametersDisplayed(const juce::Array<YsfxParameter *> &parameters)
{
    paramComponents.clear();
    setSize(0, 0);

    for (auto *param : parameters)
        if (param->isAutomatable())
            addAndMakeVisible(paramComponents.add(
                new YsfxParameterDisplayComponent(*param)));

    int maxWidth = 800;

    for (auto &comp : paramComponents) {
        maxWidth = juce::jmax(maxWidth, comp->getWidth());
    }

    setSize(maxWidth, getRecommendedHeight());
}

int YsfxParametersPanel::getRecommendedHeight(int heightAtLeast) const
{
    int height = 0;

    for (auto &comp : paramComponents)
        height += comp->getHeight();

    return juce::jmax(height, heightAtLeast);
}

void YsfxParametersPanel::paint(juce::Graphics &g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void YsfxParametersPanel::resized()
{
    auto area = getLocalBounds();

    for (auto *comp : paramComponents)
        comp->setBounds(area.removeFromTop(comp->getHeight()));
}

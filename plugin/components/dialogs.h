#pragma once
#include <juce_gui_basics/juce_gui_basics.h>


class ExclusionFilter : public juce::TextEditor::InputFilter
{
    public:
        ExclusionFilter(juce::String excludedChar) : m_excludedChar(excludedChar) {}

        juce::String filterNewText(juce::TextEditor&, const juce::String& text) override
        {
            return text.removeCharacters(m_excludedChar);
        }

    private:
        juce::String m_excludedChar;
};


juce::AlertWindow* show_async_text_input(juce::String title, juce::String message, std::function<void(juce::String, bool)> callback, std::optional<std::function<juce::String(juce::String)>> validator=std::nullopt, juce::Component* parent=nullptr);
juce::AlertWindow* show_option_window(juce::String title, juce::String message, std::vector<juce::String> buttons, std::function<void(int result)> callback, juce::Component* parent=nullptr);

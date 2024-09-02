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


static void show_async_text_input(juce::String title, juce::String message, std::function<void(juce::String, bool)> callback)
{
    auto* window = new juce::AlertWindow(title, message, juce::AlertWindow::NoIcon);
    window->addTextEditor("textField", "", "");
    juce::TextEditor* textEditor = window->getTextEditor("textField");

    auto inputFilter = std::make_unique<ExclusionFilter>("`");
    textEditor->setInputFilter(inputFilter.release(), true);

    auto finalize_success = [window, textEditor, callback]() { 
        if (textEditor->getText().isEmpty()) {
            window->setMessage("Please enter a preset name or press cancel.");
        } else {
            callback(textEditor->getText(), true); window->exitModalState(0);
        };
    };
    auto finalize_cancel = [window, textEditor, callback]() { callback(textEditor->getText(), false); window->exitModalState(0); };

    textEditor->onReturnKey = finalize_success;
    window->addButton("Ok", 1);
    window->getButton("Ok")->onClick = finalize_success;
    window->addButton("Cancel", 0);
    window->getButton("Cancel")->onClick = finalize_cancel;

    window->setAlwaysOnTop(true);
    window->enterModalState(true, nullptr, true);
    textEditor->setWantsKeyboardFocus(true);
    textEditor->grabKeyboardFocus();
}

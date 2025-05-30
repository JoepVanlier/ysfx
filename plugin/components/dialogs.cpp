#include "dialogs.h"


juce::AlertWindow* show_async_text_input(juce::String title, juce::String message, std::function<void(juce::String, bool)> callback, std::optional<std::function<juce::String(juce::String)>> validator, juce::Component* parent)
{
    auto* window = new juce::AlertWindow(title, message, juce::AlertWindow::NoIcon);
    window->addTextEditor("textField", "", "");
    juce::TextEditor* textEditor = window->getTextEditor("textField");

    auto inputFilter = std::make_unique<ExclusionFilter>("`");
    textEditor->setInputFilter(inputFilter.release(), true);

    auto finalize_success = [window, textEditor, callback, validator]() { 
        if (textEditor->getText().isEmpty()) {
            window->setMessage("Please enter a preset name or press cancel.");
        } else {
            if (validator) {
                juce::String warning = (*validator)(textEditor->getText());
                if (warning.isNotEmpty()) {
                    window->setMessage(warning);
                    return;
                }
            }

            callback(textEditor->getText(), true);
            window->exitModalState(0);
            window->setVisible(false);
        };
    };
    auto finalize_cancel = [window, textEditor, callback]() {
        callback(textEditor->getText(), false);
        window->exitModalState(0);
        window->setVisible(false);
    };

    textEditor->onReturnKey = finalize_success;
    window->addButton("Ok", 1);
    window->getButton("Ok")->onClick = finalize_success;
    window->addButton("Cancel", 0);
    window->getButton("Cancel")->onClick = finalize_cancel;

    window->setAlwaysOnTop(true);
    window->enterModalState(true, nullptr, false);
    textEditor->setWantsKeyboardFocus(true);
    textEditor->grabKeyboardFocus();

    if (parent) {
        window->setCentrePosition(parent->getScreenPosition() + juce::Point<int>{parent->getScreenBounds().getWidth() / 2, parent->getScreenBounds().getHeight() / 2});
    }

    return window;
}


juce::AlertWindow* show_option_window(juce::String title, juce::String message, std::vector<juce::String> buttons, std::function<void(int result)> callback, juce::Component* parent)
{
    auto* window = new juce::AlertWindow(title, message, juce::AlertWindow::NoIcon);
    window->setMessage(message);

    auto finalize = [callback, window](int value) {
        window->exitModalState(value);
        window->setVisible(false);
        callback(value);
    };

    int result = 1;
    for (auto label : buttons) {
        window->addButton(label, result);
        window->getButton(label)->onClick = [finalize, result]() { finalize(result); };
        result++;
    }

    window->setAlwaysOnTop(true);
    window->enterModalState(true, nullptr, false);
    window->setWantsKeyboardFocus(true);
    window->grabKeyboardFocus();
    window->setEscapeKeyCancels(true);

    if (parent) {
        window->setCentrePosition(parent->getScreenPosition() + juce::Point<int>{parent->getScreenBounds().getWidth() / 2, parent->getScreenBounds().getHeight() / 2});
    }

    return window;
}

#include "misc_buttons.h"

void ScalingButton::clicked (const juce::ModifierKeys& mods)
{
    if (mods.isPopupMenu())
    {
        if (onPopupClick) onPopupClick();
    }
    else
    {
        if (onClick) onClick();
    }
}


ScalingEditor :: ScalingEditor(float currentScale, std::function<void(float)> onAccept): onAccept(std::move(onAccept))
{
    addAndMakeVisible(editor);
    editor.setText(juce::String(currentScale, 2), juce::dontSendNotification);
    editor.setSelectAllWhenFocused(true);

    editor.onReturnKey = [this]
    {
        if (editor.getText().getFloatValue()) {
            commit();
        }
    };

    editor.onEscapeKey = [this]
    {
        dismiss();
    };

    setSize(100, 30);
}



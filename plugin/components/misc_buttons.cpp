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


ViewToggleButton::ViewToggleButton()
{
    setClickingTogglesState(true);
    setToggleState(true, juce::dontSendNotification);
    setTitle("View");
    setDescription("Select UI or parameter list view");
    setWantsKeyboardFocus(true);

    setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}


void ViewToggleButton::paintButton(juce::Graphics& g, bool isMouseOver, bool isButtonDown)
{
    auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    constexpr float radius = 6.0f;

    const bool ctrlSelected = getToggleState();

    g.setColour(juce::Colour(this->findColour(TextButton::buttonColourId)));
    g.fillRoundedRectangle (bounds, radius);

    auto left  = bounds.removeFromLeft(bounds.getWidth() * 0.5f);
    auto right = bounds;

    g.setColour(juce::Colour(this->findColour(juce::PopupMenu::highlightedBackgroundColourId)));
    g.fillRoundedRectangle(ctrlSelected ? left : right, radius);

    juce::Colour bc(this->findColour(TextButton::buttonOnColourId).withMultipliedSaturation(0.3f));
    g.setColour(bc.contrasting().withAlpha(true ? 0.6f : 0.4f));

    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced (0.5f), radius, 1.0f);

    g.drawLine(getWidth() * 0.5f, 5.0f, getWidth() * 0.5f, getHeight() - 5.0f, 1.0f);
    g.setFont(getLookAndFeel().getTextButtonFont(*this, getHeight()));

    g.setColour(this->findColour(ctrlSelected ? TextButton::textColourOnId : TextButton::textColourOffId));
    g.drawText("UI", left.toNearestInt(), juce::Justification::centred);

    g.setColour(this->findColour(ctrlSelected ? TextButton::textColourOffId : TextButton::textColourOnId));
    g.drawText("List", right.toNearestInt(), juce::Justification::centred);

    if (isMouseOver)
    {
        g.setColour(juce::Colours::white.withAlpha (isButtonDown ? 0.06f : 0.035f));
        g.fillRoundedRectangle(bounds, radius);
    }
}


void ViewToggleButton::clicked()
{
    juce::Button::clicked();

    if (auto* handler = getAccessibilityHandler())
        handler->notifyAccessibilityEvent(juce::AccessibilityEvent::titleChanged);
}


void ViewToggleButton::mouseUp (const juce::MouseEvent& e)
{
    const bool wantsUI = e.position.x < getWidth() * 0.5f;

    if (wantsUI != getToggleState())
        setToggleState(wantsUI, juce::sendNotificationSync);
    else
        clicked();
}


std::unique_ptr<juce::AccessibilityHandler> ViewToggleButton::createAccessibilityHandler()
{
    struct Handler : juce::AccessibilityHandler
    {
        ViewToggleButton& button;

        Handler (ViewToggleButton& b): AccessibilityHandler(b, juce::AccessibilityRole::toggleButton), button(b) {}

        juce::String getTitle() const override
        {
            return "View " + juce::String(button.getToggleState() ? "UI" : "List");
        }
    };

    return std::make_unique<Handler>(*this);
}

#include "lookandfeel.h"


class ScalingButton : public juce::TextButton
{
    public:
        using juce::TextButton::TextButton;

        std::function<void()> onClick;
        std::function<void()> onPopupClick;

        void clicked (const juce::ModifierKeys& mods) override;
};

class ScalingEditor : public juce::Component
{
    public:
        ScalingEditor(float currentScale, std::function<void(float)> onAccept);

        void visibilityChanged() override
        {
            if (isVisible())
            {
                juce::MessageManager::callAsync([this]
                {
                    editor.grabKeyboardFocus();
                    editor.selectAll();
                });
            }
        }

        void resized() override
        {
            editor.setBounds(getLocalBounds());
        }

    private:
        void commit()
        {
            if (onAccept)
                onAccept(editor.getText().getFloatValue());

            dismiss();
        }

        void dismiss()
        {
            if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
                box->dismiss();
        }

        juce::TextEditor editor;
        std::function<void(float)> onAccept;
};


class ViewToggleButton : public juce::TextButton
{
public:
    ViewToggleButton();

    void clicked() override;
    void paintButton(juce::Graphics& g, bool isMouseOver, bool isButtonDown) override;
    void mouseUp(const juce::MouseEvent& e);

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;
};

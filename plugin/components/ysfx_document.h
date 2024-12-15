#pragma once
#include "modal_textinputbox.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>


class YSFXCodeDocument : public juce::CodeDocument {
    public:
        YSFXCodeDocument();
        ~YSFXCodeDocument() {};
        void reset(void);
        void loadFile(juce::File file);
        bool saveFile(juce::File file=juce::File{});
        void checkFileForModifications();

        bool loaded();
        juce::File getPath();

    private:
        juce::File m_file{};
        juce::Time m_changeTime{0};
        bool m_reloadDialogGuard{false};

        bool m_fileChooserActive{false};
        std::unique_ptr<juce::FileChooser> m_fileChooser;
        std::unique_ptr<juce::AlertWindow> m_alertWindow;
};

class YSFXCodeEditor : public juce::CodeEditorComponent 
{
    public:
        YSFXCodeEditor(juce::CodeDocument& doc, juce::CodeTokeniser* tokenizer, std::function<bool(const juce::KeyPress&)> keyPressCallback): CodeEditorComponent(doc, tokenizer), m_keyPressCallback{keyPressCallback} {}
        
        bool keyPressed(const juce::KeyPress &key) override 
        {
            if (!m_keyPressCallback(key)) {
                return juce::CodeEditorComponent::keyPressed(key);
            } else {
                return true;
            };
        }

    private:
        std::function<bool(const juce::KeyPress&)> m_keyPressCallback;
};

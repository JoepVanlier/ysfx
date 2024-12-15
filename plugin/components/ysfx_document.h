// Copyright 2021 Jean Pierre Cimalando
// Copyright 2024 Joep Vanlier
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
// Modifications by Joep Vanlier, 2024
//
// SPDX-License-Identifier: Apache-2.0
//

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
        juce::String getName(void);

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


class CodeEditor : public juce::CodeEditorComponent
{
    public:
        CodeEditor(
            juce::CodeDocument& doc, juce::CodeTokeniser* tokenizer, std::function<bool(const juce::KeyPress&)> keyPressCallback, std::function<bool(int x, int y)> dblClickCallback
        ) : CodeEditorComponent(doc, tokenizer), m_keyPressCallback{keyPressCallback}, m_dblClickCallback{dblClickCallback} {};
        
        bool keyPressed(const juce::KeyPress &key) override 
        {
            if (!m_keyPressCallback(key)) {
                return juce::CodeEditorComponent::keyPressed(key);
            } else {
                return true;
            };
        }

        void mouseDoubleClick(const juce::MouseEvent &e) override
        {
            if (!m_dblClickCallback(e.x, e.y)) {
                return juce::CodeEditorComponent::mouseDoubleClick(e);
            };
        }

        juce::String getLineAt(int x, int y) const {
            juce::CodeDocument::Position position(getPositionAt(x, y));
            auto& doc = getDocument();
            juce::CodeDocument::Position start(doc, 0);
            juce::CodeDocument::Position stop(doc, 0);
            doc.findLineContaining(position, start, stop);
            return doc.getTextBetween(start, stop);
        };

        int search(juce::String text, bool reverse=false)
        {
            if (text.isEmpty()) return 0;

            auto currentPosition = juce::CodeDocument::Position(getDocument(), getCaretPosition());

            auto chunk = [this, currentPosition](bool before) {
                if (before) {
                    return getDocument().getTextBetween(juce::CodeDocument::Position(getDocument(), 0), currentPosition.movedBy(-1));
                } else {
                    return getDocument().getTextBetween(currentPosition, juce::CodeDocument::Position(getDocument(), getDocument().getNumCharacters()));
                }
            };

            int position = reverse ? chunk(true).lastIndexOfIgnoreCase(text) : chunk(false).indexOfIgnoreCase(text);

            juce::CodeDocument::Position searchPosition;
            if (position == -1) {
                // We didn't find it! Start from the other end!
                position = reverse ? chunk(false).lastIndexOfIgnoreCase(text) : chunk(true).indexOfIgnoreCase(text);

                if (position == -1) {
                    grabKeyboardFocus();
                    return 0;
                }
                searchPosition = juce::CodeDocument::Position(getDocument(), reverse ? currentPosition.getPosition() + position : position);
            } else {
                // Found it!
                searchPosition = juce::CodeDocument::Position(getDocument(), reverse ? position : currentPosition.getPosition() + position);
            }

            auto pos = juce::CodeDocument::Position(getDocument(), searchPosition.getPosition());
            grabKeyboardFocus();
            moveCaretTo(pos, false);
            moveCaretTo(pos.movedBy(text.length()), true);
            return 1;
        }

    private:
        std::function<bool(const juce::KeyPress&)> m_keyPressCallback;
        std::function<bool(int x, int y)> m_dblClickCallback;
};


class YSFXCodeEditor
{
    public:
        YSFXCodeEditor(juce::CodeTokeniser* tokenizer, std::function<bool(const juce::KeyPress&)> keyPressCallback, std::function<bool(int x, int y)> dblClickCallback) {
            m_document = std::make_unique<YSFXCodeDocument>();
            m_editor = std::make_unique<CodeEditor>(*m_document, tokenizer, keyPressCallback, dblClickCallback);
            m_editor->setVisible(false);
        };
        ~YSFXCodeEditor() {
            // Make sure we kill the editor first since it may be referencing the document!
            m_editor.reset();
            m_document.reset();
        }

        void setColourScheme(juce::CodeEditorComponent::ColourScheme colourScheme) { m_editor->setColourScheme(colourScheme); };
        void checkFileForModifications() { m_document->checkFileForModifications(); };
        void reset() { m_document->reset(); };
        void setReadOnly(bool readOnly) { m_editor->setReadOnly(readOnly); };

        juce::File getPath() { return m_document->getPath(); };
        juce::String getName() { return m_document->getName(); };
        void loadFile(juce::File file) { m_document->loadFile(file); };
        bool saveFile(juce::File file = juce::File{}) { return m_document->saveFile(file); };
        int search(juce::String text, bool reverse = false) { return m_editor->search(text, reverse); };

        bool hasFocus() {
            juce::Component *focus = m_editor->getCurrentlyFocusedComponent();
            return focus == m_editor.get();
        };

        juce::String getLineAt(int x, int y) const { return m_editor->getLineAt(x, y); };
        CodeEditor* getVisibleComponent() { return m_editor.get(); };

        void setVisible(bool visible) { m_editor->setVisible(visible); };
        template <typename T>
        void setBounds(T&& arg) { m_editor->setBounds(std::forward<T>(arg)); };

    private:
        std::unique_ptr<CodeEditor> m_editor;
        std::unique_ptr<YSFXCodeDocument> m_document;
};


class YSFXTabbedButtonBar : public juce::TabbedButtonBar
{
    public:
        YSFXTabbedButtonBar(TabbedButtonBar::Orientation orientation, std::function<void(int idx)> changeCallback): TabbedButtonBar(orientation), m_changeCallback(changeCallback) {};

    private:
        void currentTabChanged(int newCurrentTabIndex, const juce::String &newCurrentTabName) override
        {
            (void) newCurrentTabName;
            if (m_emitChange) m_changeCallback(newCurrentTabIndex);
        }

        std::function<void(int idx)> m_changeCallback = [](int idx){ (void) idx; };
        bool m_emitChange{true};

        friend class ScopedUpdateBlocker;
};


class ScopedUpdateBlocker
{
    public:
        ScopedUpdateBlocker(YSFXTabbedButtonBar& buttonBar): m_bar(buttonBar) { m_bar.m_emitChange = false; };
        ~ScopedUpdateBlocker() { m_bar.m_emitChange = true; };
    
    private:
        YSFXTabbedButtonBar& m_bar;
};

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
#include "ide_view.h"
#include "utility/functional_timer.h"
#include "tokenizer.h"
#include <algorithm>

class YSFXCodeEditor : public juce::CodeEditorComponent 
{
    public:
        YSFXCodeEditor(juce::CodeDocument& document, juce::CodeTokeniser* codeTokeniser, std::function<bool(const juce::KeyPress&)> keyPressCallback): CodeEditorComponent(document, codeTokeniser), m_keyPressCallback{keyPressCallback} {};
        
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

struct YsfxIDEView::Impl {
    YsfxIDEView *m_self = nullptr;
    ysfx_u m_fx;
    juce::Time m_changeTime;
    bool m_reloadDialogGuard = false;
    std::unique_ptr<juce::CodeDocument> m_document;
    std::unique_ptr<JSFXTokenizer> m_tokenizer;
    std::unique_ptr<YSFXCodeEditor> m_editor;
    std::unique_ptr<juce::TextButton> m_btnSave;
    std::unique_ptr<juce::TextButton> m_btnUpdate;
    std::unique_ptr<juce::Label> m_lblVariablesHeading;
    std::unique_ptr<juce::Viewport> m_vpVariables;
    std::unique_ptr<juce::Component> m_compVariables;
    std::unique_ptr<juce::Label> m_lblStatus;
    std::unique_ptr<juce::TextEditor> m_searchEditor;
    std::unique_ptr<juce::Timer> m_relayoutTimer;
    std::unique_ptr<juce::Timer> m_fileCheckTimer;
    std::unique_ptr<juce::FileChooser> m_fileChooser;
    bool m_fileChooserActive{false};

    struct VariableUI {
        ysfx_real *m_var = nullptr;
        juce::String m_name;
        std::unique_ptr<juce::Label> m_lblName;
        std::unique_ptr<juce::Label> m_lblValue;
    };
    juce::Array<VariableUI> m_vars;
    std::unique_ptr<juce::Timer> m_varsUpdateTimer;

    juce::File m_file{};

    bool m_forceUpdate{false};

    //==========================================================================
    void setupNewFx();
    void saveCurrentFile();
    void saveFile(juce::File filename);
    void saveAs();
    void search(juce::String text, bool reverse);
    void checkFileForModifications();

    //==========================================================================
    void createUI();
    void connectUI();
    void relayoutUI();
    void relayoutUILater();
};

YsfxIDEView::YsfxIDEView()
    : m_impl(new Impl)
{
    m_impl->m_self = this;

    m_impl->m_document.reset(new juce::CodeDocument);
    m_impl->m_tokenizer.reset(new JSFXTokenizer());

    m_impl->createUI();
    m_impl->connectUI();
    m_impl->relayoutUILater();

    m_impl->setupNewFx();
    this->setVisible(false);
}

YsfxIDEView::~YsfxIDEView()
{
}

void YsfxIDEView::setColourScheme(std::map<std::string, std::array<uint8_t, 3>> colormap)
{
    m_impl->m_tokenizer->setColours(colormap);
    m_impl->m_editor->setColourScheme(m_impl->m_tokenizer->getDefaultColourScheme());
}

void YsfxIDEView::setEffect(ysfx_t *fx, juce::Time timeStamp)
{
    if (m_impl->m_fx.get() == fx)
        return;

    m_impl->m_fx.reset(fx);
    if (fx)
        ysfx_add_ref(fx);

    m_impl->m_changeTime = timeStamp;
    m_impl->setupNewFx();
    m_impl->m_btnSave->setEnabled(true);
}

void YsfxIDEView::setStatusText(const juce::String &text)
{
    m_impl->m_lblStatus->setText(text, juce::dontSendNotification);
    m_impl->m_lblStatus->setTooltip(text);
}

void YsfxIDEView::resized()
{
    m_impl->relayoutUILater();
}

void YsfxIDEView::focusOnCodeEditor()
{
    m_impl->m_forceUpdate = true;
}

void YsfxIDEView::focusOfChildComponentChanged(FocusChangeType cause)
{
    (void)cause;

    juce::Component *focus = getCurrentlyFocusedComponent();

    if (focus == m_impl->m_editor.get()) {
        juce::Timer *timer = FunctionalTimer::create([this]() { m_impl->checkFileForModifications(); });
        m_impl->m_fileCheckTimer.reset(timer);
        timer->startTimer(100);
    }
    else {
        m_impl->m_fileCheckTimer.reset();
    }
}

void YsfxIDEView::Impl::setupNewFx()
{
    ysfx_t *fx = m_fx.get();

    m_vars.clear();
    m_varsUpdateTimer.reset();

    if (!fx) {
        //
        m_document->replaceAllContent(juce::String{});
        m_editor->setReadOnly(true);
    }
    else {
        juce::File file{juce::CharPointer_UTF8{ysfx_get_file_path(fx)}};
        if (file != juce::File{}) m_file = file;

        {
            juce::MemoryBlock memBlock;
            if (m_file.loadFileAsData(memBlock)) {
                juce::String newContent = memBlock.toString();
                memBlock = {};
                if (newContent != m_document->getAllContent()) {
                    m_document->replaceAllContent(newContent);
                    m_editor->moveCaretToTop(false);
                }
            }
        }

        m_vars.ensureStorageAllocated(64);

        ysfx_enum_vars(fx, +[](const char *name, ysfx_real *var, void *userdata) -> int {
            Impl &impl = *(Impl *)userdata;
            Impl::VariableUI ui;
            ui.m_var = var;
            ui.m_name = juce::CharPointer_UTF8{name};
            ui.m_lblName.reset(new juce::Label(juce::String{}, ui.m_name));
            ui.m_lblName->setTooltip(ui.m_name);
            ui.m_lblName->setMinimumHorizontalScale(1.0f);
            impl.m_compVariables->addAndMakeVisible(*ui.m_lblName);
            ui.m_lblValue.reset(new juce::Label(juce::String{}, "0"));
            impl.m_compVariables->addAndMakeVisible(*ui.m_lblValue);
            impl.m_vars.add(std::move(ui));
            return 1;
        }, this);

        if (!m_vars.isEmpty()) {
            std::sort(
                m_vars.begin(), m_vars.end(),
                [](const VariableUI &a, const VariableUI &b) -> bool {
                    return a.m_name.compareNatural(b.m_name) < 0;
                });

            m_varsUpdateTimer.reset(FunctionalTimer::create([this]() {
                if (m_self->isShowing() && m_btnUpdate && (m_btnUpdate->getToggleState() || m_forceUpdate)) {
                    for (int i = 0; i < m_vars.size(); ++i) {
                        VariableUI &ui = m_vars.getReference(i);
                        ui.m_lblValue->setText(juce::String(*ui.m_var), juce::dontSendNotification);
                        m_forceUpdate = false;
                    }
                };
            }));

            m_varsUpdateTimer->startTimer(100);
        }

        m_editor->setReadOnly(false);

        relayoutUILater();
    }
}

void YsfxIDEView::Impl::saveAs()
{
    if (m_fileChooserActive) return;

    juce::File initialPath;
    if (m_file != juce::File{}) {
        initialPath = m_file.getParentDirectory();
    }

    m_fileChooser.reset(new juce::FileChooser(TRANS("Choose filename to save JSFX to"), initialPath));
    m_fileChooser->launchAsync(
        juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser &chooser) {
            juce::File chosenFile = chooser.getResult();
            if (chosenFile != juce::File()) {
                if (chosenFile.exists()) {
                    juce::AlertWindow::showAsync(
                        juce::MessageBoxOptions{}
                        .withParentComponent(m_self)
                        .withIconType(juce::MessageBoxIconType::QuestionIcon)
                        .withTitle(TRANS("Overwrite?"))
                        .withButton(TRANS("Yes"))
                        .withButton(TRANS("No"))
                        .withMessage(TRANS("File already exists! Overwrite?")),
                        [this, chosenFile](int result) {
                            if (result == 1) {
                                this->saveFile(chosenFile);
                                m_self->onFileSaved(chosenFile);
                            };
                        }
                    );
                } else {
                    saveFile(chosenFile);
                    m_self->onFileSaved(chosenFile);
                }
            }
            m_fileChooserActive = false;
        }
    );
}

void YsfxIDEView::Impl::saveFile(juce::File file)
{
    const juce::String content = m_document->getAllContent();

    bool success = file.replaceWithData(content.toRawUTF8(), content.getNumBytesAsUTF8());
    if (!success) {
        juce::AlertWindow::showAsync(
            juce::MessageBoxOptions{}
            .withParentComponent(m_self)
            .withIconType(juce::MessageBoxIconType::WarningIcon)
            .withTitle(TRANS("Error"))
            .withButton(TRANS("OK"))
            .withMessage(TRANS("Could not save the JSFX document.")),
            nullptr);
        return;
    }

    m_changeTime = juce::Time::getCurrentTime();

    if (m_self->onFileSaved)
        m_self->onFileSaved(file);
}

void YsfxIDEView::Impl::saveCurrentFile()
{
    ysfx_t *fx = m_fx.get();
    if (!fx)
        return;

    if (m_file.existsAsFile()) {
        m_btnSave->setEnabled(false);
        saveFile(m_file);
    } else {
        saveAs();
    }
}

void YsfxIDEView::Impl::checkFileForModifications()
{
    ysfx_t *fx = m_fx.get();
    if (!fx)
        return;

    if (m_file == juce::File{})
        return;

    juce::Time newMtime = m_file.getLastModificationTime();
    if (newMtime == juce::Time{})
        return;

    if (m_changeTime == juce::Time{} || newMtime > m_changeTime) {
        m_changeTime = newMtime;

        if (!m_reloadDialogGuard) {
            m_reloadDialogGuard = true;

            auto callback = [this](int result) {
                m_reloadDialogGuard = false;
                if (result != 0) {
                    if (m_self->onReloadRequested)
                        m_self->onReloadRequested(m_file);
                }
            };

            juce::AlertWindow::showAsync(
                juce::MessageBoxOptions{}
                .withAssociatedComponent(m_self)
                .withIconType(juce::MessageBoxIconType::QuestionIcon)
                .withTitle(TRANS("Reload?"))
                .withButton(TRANS("Yes"))
                .withButton(TRANS("No"))
                .withMessage(TRANS("The file has been modified outside this editor. Reload it?")),
                callback);
        }
    }
}

void YsfxIDEView::Impl::search(juce::String text, bool reverse=false)
{
    if (text.isNotEmpty())
    {
        auto currentPosition = juce::CodeDocument::Position(*m_document, m_editor->getCaretPosition());
        
        auto chunk = [this, currentPosition](bool before) {
            if (before) {
                return m_document->getTextBetween(juce::CodeDocument::Position(*m_document, 0), currentPosition.movedBy(-1));
            } else {
                return m_document->getTextBetween(currentPosition, juce::CodeDocument::Position(*m_document, m_document->getNumCharacters()));
            }
        };

        int position = reverse ? chunk(true).lastIndexOfIgnoreCase(text) : chunk(false).indexOfIgnoreCase(text);
        juce::CodeDocument::Position searchPosition;
        if (position == -1) {
            // We didn't find it! Start from the other end!
            position = reverse ? chunk(false).lastIndexOfIgnoreCase(text) : chunk(true).indexOfIgnoreCase(text);

            if (position == -1) {
                // Not found at all -> stop
                if (text.compare(m_document->getTextBetween(currentPosition.movedBy(- text.length()), currentPosition)) != 0) {
                    m_lblStatus->setText(TRANS("Didn't find search string ") + text, juce::NotificationType::dontSendNotification);
                } else {
                    m_lblStatus->setText(TRANS("Didn't find other copies of search string ") + text, juce::NotificationType::dontSendNotification);
                }
                m_editor->grabKeyboardFocus();
                return;
            }
            searchPosition = juce::CodeDocument::Position(*m_document, reverse ? currentPosition.getPosition() + position : position);
        } else {
            // Found it!
            searchPosition = juce::CodeDocument::Position(*m_document, reverse ? position : currentPosition.getPosition() + position);
        }

        auto pos = juce::CodeDocument::Position(*m_document, searchPosition.getPosition());
        m_editor->grabKeyboardFocus();
        m_editor->moveCaretTo(pos, false);
        m_editor->moveCaretTo(pos.movedBy(text.length()), true);
        m_lblStatus->setText(TRANS("Found ") + text + TRANS(". (SHIFT +) CTRL/CMD + G to repeat search (backwards)."), juce::NotificationType::dontSendNotification);
    }
}

void YsfxIDEView::Impl::createUI()
{
    auto keyPressCallback = [this](const juce::KeyPress& key) -> bool {
        if (key.getModifiers().isCommandDown()) {
            if (key.isKeyCurrentlyDown('f')) {
                m_lblStatus->setText("", juce::NotificationType::dontSendNotification);
                m_searchEditor->setVisible(true);
                m_lblStatus->setVisible(false);
                m_searchEditor->setText("", juce::NotificationType::dontSendNotification);
                m_searchEditor->setWantsKeyboardFocus(true);
                m_searchEditor->grabKeyboardFocus();
                m_searchEditor->setEscapeAndReturnKeysConsumed(true);
                m_searchEditor->onReturnKey = [this]() {
                    search(m_searchEditor->getText());
                    m_searchEditor->setWantsKeyboardFocus(false);
                    m_searchEditor->setVisible(false);
                    m_lblStatus->setVisible(true);
                };
                m_searchEditor->onFocusLost = [this]() {
                    m_searchEditor->setWantsKeyboardFocus(false);
                    m_searchEditor->setVisible(false);
                    m_lblStatus->setVisible(true);
                };

                return true;
            }

            if (key.isKeyCurrentlyDown('s')) {
                saveCurrentFile();
                return true;
            }

            if (key.isKeyCurrentlyDown('g')) {
                m_lblStatus->setText("", juce::NotificationType::dontSendNotification);
                search(m_searchEditor->getText(), key.getModifiers().isShiftDown());
                return true;
            }
        }

        return false;
    };

    m_editor.reset(new YSFXCodeEditor(*m_document, m_tokenizer.get(), keyPressCallback));
    m_self->addAndMakeVisible(*m_editor);
    m_btnSave.reset(new juce::TextButton(TRANS("Save")));
    m_btnSave->addShortcut(juce::KeyPress('s', juce::ModifierKeys::ctrlModifier, 0));
    m_self->addAndMakeVisible(*m_btnSave);
    m_btnUpdate.reset(new juce::TextButton(TRANS("Watch (off)")));
    m_btnUpdate->setTooltip("Enable this to continuously update variables (note this has a big performance impact currently).");
    m_btnUpdate->setClickingTogglesState(true);
    m_btnUpdate->setToggleState(false, juce::NotificationType::dontSendNotification);
    m_self->addAndMakeVisible(*m_btnUpdate);
    m_lblVariablesHeading.reset(new juce::Label(juce::String{}, TRANS("Variables")));
    m_self->addAndMakeVisible(*m_lblVariablesHeading);
    m_vpVariables.reset(new juce::Viewport);
    m_vpVariables->setScrollBarsShown(true, false);
    m_self->addAndMakeVisible(*m_vpVariables);
    m_compVariables.reset(new juce::Component);
    m_vpVariables->setViewedComponent(m_compVariables.get(), false);
    m_lblStatus.reset(new juce::Label);
    m_lblStatus->setMinimumHorizontalScale(1.0f);
    m_searchEditor.reset(new juce::TextEditor);
    m_self->addAndMakeVisible(*m_searchEditor);
    m_self->addAndMakeVisible(*m_lblStatus);
    m_searchEditor->setVisible(false);
}

void YsfxIDEView::Impl::connectUI()
{
    m_btnSave->onClick = [this]() { saveCurrentFile(); };
    m_btnUpdate->onClick = [this]() { m_btnUpdate->setButtonText(m_btnUpdate->getToggleState() ? TRANS("Watch (on)") : TRANS("Watch (off)")); };
}

void YsfxIDEView::Impl::relayoutUI()
{
    juce::Rectangle<int> temp;
    const juce::Rectangle<int> bounds = m_self->getLocalBounds();

    temp = bounds;
    const juce::Rectangle<int> debugArea = temp.removeFromRight(300);
    const juce::Rectangle<int> topRow = temp.removeFromTop(50);
    const juce::Rectangle<int> statusArea = temp.removeFromBottom(20);
    const juce::Rectangle<int> editArea = temp;

    ///
    temp = topRow.reduced(10, 10);
    m_btnSave->setBounds(temp.removeFromLeft(100));
    m_btnUpdate->setBounds(temp.removeFromLeft(100));
    temp.removeFromLeft(10);

    ///
    temp = debugArea;
    m_lblVariablesHeading->setBounds(temp.removeFromTop(50).reduced(10, 10));
    m_vpVariables->setBounds(temp.reduced(10, 10));

    const int varRowHeight = 20;
    for (int i = 0; i < m_vars.size(); ++i) {
        VariableUI &var = m_vars.getReference(i);
        juce::Rectangle<int> varRow = juce::Rectangle<int>{}
            .withWidth(m_vpVariables->getWidth())
            .withHeight(varRowHeight)
            .withY(i * varRowHeight);
        juce::Rectangle<int> varTemp = varRow;
        var.m_lblValue->setBounds(varTemp.removeFromRight(100));
        var.m_lblName->setBounds(varTemp);
    }
    m_compVariables->setSize(m_vpVariables->getWidth(), m_vars.size() * varRowHeight);

    m_lblStatus->setBounds(statusArea);
    m_searchEditor->setBounds(statusArea);

    m_editor->setBounds(editArea);

    if (m_relayoutTimer)
        m_relayoutTimer->stopTimer();
}

void YsfxIDEView::Impl::relayoutUILater()
{
    if (!m_relayoutTimer)
        m_relayoutTimer.reset(FunctionalTimer::create([this]() { relayoutUI(); }));
    m_relayoutTimer->startTimer(0);
}

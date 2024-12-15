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

#include "lookandfeel.h"
#include "ide_view.h"
#include "utility/functional_timer.h"
#include "tokenizer.h"
#include <algorithm>
#include "ysfx_document.h"


struct YsfxIDEView::Impl {
    YsfxIDEView *m_self = nullptr;
    ysfx_u m_fx;

    std::vector<std::shared_ptr<YSFXCodeEditor>> m_editors;
    std::unique_ptr<JSFXTokenizer> m_tokenizer;
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

    std::unique_ptr<YSFXTabbedButtonBar> m_tabs;

    bool m_fileChooserActive{false};

    struct VariableUI {
        ysfx_real *m_var = nullptr;
        juce::String m_name;
        std::unique_ptr<juce::Label> m_lblName;
        std::unique_ptr<juce::Label> m_lblValue;
    };
    juce::Array<VariableUI> m_vars;
    std::unique_ptr<juce::Timer> m_varsUpdateTimer;

    bool m_forceUpdate{false};
    int m_currentEditorIndex{0};

    //==========================================================================
    void setupNewFx();
    void saveCurrentFile();
    void saveAs();
    std::shared_ptr<YSFXCodeEditor> addEditor();
    void openDocument(juce::File file);
    void setCurrentEditor(int idx);
    std::shared_ptr<YSFXCodeEditor> getCurrentEditor();

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
    m_impl->m_editors[0]->setColourScheme(m_impl->m_tokenizer->getDefaultColourScheme());
}

void YsfxIDEView::setEffect(ysfx_t *fx, juce::Time timeStamp)
{
    if (m_impl->m_fx.get() == fx)
        return;

    m_impl->m_fx.reset(fx);
    if (fx)
        ysfx_add_ref(fx);

    (void) timeStamp;
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

std::shared_ptr<YSFXCodeEditor> YsfxIDEView::Impl::getCurrentEditor()
{
    if (m_currentEditorIndex >= m_editors.size()) {
        setCurrentEditor(0);
    }

    return m_editors[m_currentEditorIndex];
}

void YsfxIDEView::focusOfChildComponentChanged(FocusChangeType cause)
{
    (void)cause;

    if (m_impl->getCurrentEditor()->hasFocus()) {
        juce::Timer *timer = FunctionalTimer::create([this]() { 
            m_impl->getCurrentEditor()->checkFileForModifications();
        });
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
        getCurrentEditor()->reset();
        getCurrentEditor()->setReadOnly(true);
    }
    else {
        juce::File file{juce::CharPointer_UTF8{ysfx_get_file_path(fx)}};
        m_editors[0]->loadFile(file);

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

        m_editors[0]->setReadOnly(false);

        relayoutUILater();
    }
}

void YsfxIDEView::Impl::saveAs()
{
    if (m_fileChooserActive) return;
    if (m_currentEditorIndex >= m_editors.size()) return;

    auto editor = m_editors[m_currentEditorIndex];
    juce::File initialPath = editor->getPath().getParentDirectory();

    m_fileChooser.reset(new juce::FileChooser(TRANS("Choose filename to save JSFX to"), initialPath));
    m_fileChooser->launchAsync(
        juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles,
        [this, editor](const juce::FileChooser &chooser) {
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
                        [this, chosenFile, editor](int result) {
                            if (result == 1) {
                                editor->saveFile(chosenFile);
                                if (m_self->onFileSaved) m_self->onFileSaved(chosenFile);
                            };
                        }
                    );
                } else {
                    m_editors[0]->saveFile(chosenFile);
                    if (m_self->onFileSaved) m_self->onFileSaved(chosenFile);
                }
            }
            m_fileChooserActive = false;
        }
    );
}

void YsfxIDEView::Impl::saveCurrentFile()
{
    ysfx_t *fx = m_fx.get();
    if (!fx)
        return;

    if (m_currentEditorIndex >= m_editors.size()) return;

    if (getCurrentEditor()->getPath().existsAsFile()) {
        getCurrentEditor()->saveFile();
        if (m_self->onFileSaved)
            m_self->onFileSaved(m_editors[0]->getPath());
    } else {
        saveAs();
    }
    m_btnSave->setEnabled(false);
}

void YsfxIDEView::Impl::openDocument(juce::File file)
{
    int idx = 0;
    auto fn = file.getFileName();
    for (const auto& editor : m_editors) {
        if (fn.compareIgnoreCase(editor->getName()) == 0) {
            setCurrentEditor(idx);
            return;
        };
        idx += 1;
    }

    auto editor = addEditor();
    editor->loadFile(file);
    setCurrentEditor(m_editors.size() - 1);
}

void YsfxIDEView::Impl::setCurrentEditor(int editorIndex)
{
    if (editorIndex >= m_editors.size()) return;

    m_editors[m_currentEditorIndex]->setVisible(false);
    m_currentEditorIndex = editorIndex;
    m_editors[m_currentEditorIndex]->setVisible(true);

    relayoutUILater();
}

std::shared_ptr<YSFXCodeEditor> YsfxIDEView::Impl::addEditor()
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
                    auto searchResult = getCurrentEditor()->search(m_searchEditor->getText());

                    if (searchResult) {
                        m_lblStatus->setText(TRANS("Found ") + m_searchEditor->getText() + TRANS(". (SHIFT +) CTRL/CMD + G to repeat search (backwards)."), juce::NotificationType::dontSendNotification);
                    } else {
                        m_lblStatus->setText(TRANS("Didn't find search string ") + m_searchEditor->getText(), juce::NotificationType::dontSendNotification);
                    }

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
                int searchResult = getCurrentEditor()->search(m_searchEditor->getText(), key.getModifiers().isShiftDown());

                if (searchResult) {
                    m_lblStatus->setText(TRANS("Found ") + m_searchEditor->getText() + TRANS(". (SHIFT +) CTRL/CMD + G to repeat search (backwards)."), juce::NotificationType::dontSendNotification);
                } else {
                    m_lblStatus->setText(TRANS("Didn't find search string ") + m_searchEditor->getText(), juce::NotificationType::dontSendNotification);
                }

                return true;
            }
        }

        return false;
    };

    auto dblClickCallback = [this](int x, int y) -> bool {
        if (m_currentEditorIndex >= m_editors.size()) return false;

        auto line = getCurrentEditor()->getLineAt(x, y);
        if (line.startsWithIgnoreCase("import ")) {
            // Import statement!
            auto potentialFilename = line.substring(7).trimEnd();
            if (this->m_fx) {
                char *pathString = ysfx_resolve_path_and_allocate(this->m_fx.get(), potentialFilename.toStdString().c_str(), getCurrentEditor()->getPath().getFullPathName().toStdString().c_str());
                if (pathString) {
                    auto path = juce::File(juce::CharPointer_UTF8(pathString));
                    openDocument(path);
                    ysfx_free_resolved_path(pathString);
                    return true;
                };
            }
        }
        return false;
    };

    m_editors.push_back(std::make_shared<YSFXCodeEditor>(m_tokenizer.get(), keyPressCallback, dblClickCallback));
    m_self->addAndMakeVisible(m_editors.back()->getVisibleComponent());
    return m_editors.back();
}

void YsfxIDEView::Impl::createUI()
{
    addEditor();
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
    m_tabs.reset(new YSFXTabbedButtonBar(juce::TabbedButtonBar::Orientation::TabsAtBottom, [this](int newCurrentTabIndex) { setCurrentEditor(newCurrentTabIndex); }));
    m_self->addAndMakeVisible(*m_tabs);
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

    if (m_editors.size() > 1) {
        const juce::Rectangle<int> tabRow = temp.removeFromTop(30);
        m_tabs->setBounds(tabRow);
        auto updateBlock = ScopedUpdateBlocker(*m_tabs);
        m_tabs->clearTabs();
        int idx = 0;
        for (const auto m : m_editors) {
            m_tabs->addTab(m->getName(), m_self->getLookAndFeel().findColour(m_btnSave->buttonColourId), idx);
            ++idx;
        }
        m_tabs->setCurrentTabIndex(m_currentEditorIndex, false);
    }

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

    getCurrentEditor()->setBounds(editArea);

    if (m_relayoutTimer)
        m_relayoutTimer->stopTimer();
}

void YsfxIDEView::Impl::relayoutUILater()
{
    if (!m_relayoutTimer)
        m_relayoutTimer.reset(FunctionalTimer::create([this]() { relayoutUI(); }));
    m_relayoutTimer->startTimer(0);
}

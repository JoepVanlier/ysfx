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
// SPDX-License-Identifier: Apache-2.0
//

#include "lookandfeel.h"
#include "rpl_view.h"
#include "info.h"
#include "bank_io.h"
#include "utility/functional_timer.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <algorithm>
#include "components/modal_textinputbox.h"

class BankItemsListBoxModel final : public juce::ListBox, public juce::ListBoxModel, public juce::DragAndDropTarget
{
    public:
        BankItemsListBoxModel() : ListBox() {
            setName("BankItemsListBoxModel");
            setModel(this);
            setMultipleSelectionEnabled(true);
        }

        void setItems(std::vector<juce::String> items) {
           m_items = items;
        }

        void setDropCallback(std::function<void(std::vector<int>, juce::WeakReference<juce::Component>)> dropCallback) {
            m_dropCallback = dropCallback;
        }

        void setDeleteCallback(std::function<void(std::vector<int>)> deleteCallback) {
            m_deleteCallback = deleteCallback;
        }

        void setDoubleClickCallback(std::function<void(int)> dblClickCallback) {
            m_dblClickCallback = dblClickCallback;
        }

        void setRenameCallback(std::function<void(int)> renameCallback) {
            m_renameCallback = renameCallback;
        }

    private:
        std::unique_ptr<juce::AlertWindow> m_editDialog;
        std::unique_ptr<juce::PopupMenu> m_itemMenu;
        std::vector<juce::String> m_items;
        std::function<void(int)> m_renameCallback;
        std::function<void(int)> m_dblClickCallback;
        std::function<void(std::vector<int>, juce::WeakReference<juce::Component>)> m_dropCallback;
        std::function<void(std::vector<int>)> m_deleteCallback;

        int getNumRows() override
        {
            return static_cast<int>(m_items.size());
        }

        void paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override
        {
            if (rowIsSelected)
                g.fillAll (juce::Colours::lightblue);

            g.setColour(juce::LookAndFeel::getDefaultLookAndFeel().findColour(juce::Label::textColourId));
            g.setFont((float) height * 0.7f);
            g.drawText(m_items[rowNumber], 5, 0, width, height, juce::Justification::centredLeft, true);
        }

        juce::var getDragSourceDescription(const juce::SparseSet<int>& selectedRows)
        {
            juce::Array<juce::var> juceArray;
            for (int i = 0; i < selectedRows.size(); ++i)
                juceArray.add(selectedRows[i]);
            
            return juceArray;
        }

        bool isInterestedInDragSource (const SourceDetails& dragSourceDetails) override
        {
            if (!dragSourceDetails.sourceComponent)
                return false;
            
            if (dragSourceDetails.sourceComponent == this)
                return false;

            if (dragSourceDetails.sourceComponent.get()->getName() != "BankItemsListBoxModel")
                return false;

            return true;
        }

        void itemDropped (const SourceDetails& dragSourceDetails) override
        {
            if (!dragSourceDetails.sourceComponent)
                return;
            
            if (dragSourceDetails.sourceComponent == this)
                return;

            if (dragSourceDetails.sourceComponent.get()->getName() != "BankItemsListBoxModel")
                return;

            juce::Array<juce::var> *payload = dragSourceDetails.description.getArray();
            std::vector<int> elements{(*payload).begin(), (*payload).end()};

            if (!elements.empty()) {
                m_dropCallback(elements, dragSourceDetails.sourceComponent);
            }
        }

        void deleteKeyPressed(int arg) override
        {
            (void) arg;

            std::vector<int> elements;
            auto selection = getSelectedRows();
            for (int i = 0; i < selection.size(); ++i)
                elements.push_back(selection[i]);
            
            if (!elements.empty()) {
                m_deleteCallback(elements);
            }
        }

        void listBoxItemDoubleClicked(int row, const juce::MouseEvent &) override
        {
            m_dblClickCallback(row);
        }

        void listBoxItemClicked(int row, const juce::MouseEvent& evnt) override
        {
            if (evnt.mods.isRightButtonDown() && m_renameCallback) {
                m_itemMenu.reset(new juce::PopupMenu);

                juce::PopupMenu::Options presetOptions = juce::PopupMenu::Options{}
                    .withTargetComponent(getComponentForRowNumber(row));
                
                m_itemMenu->addItem(1, "Rename");
                
                m_itemMenu->showMenuAsync(presetOptions, [this, row](int index) {
                    if (index == 1) {
                        m_renameCallback(row);
                    }
                });
            }
        }
};

class LoadedBank : public juce::Component, public juce::DragAndDropContainer {
    private:
        juce::Time m_lastLoad{0};
        juce::File m_file;
        ysfx_bank_shared m_bank;

        std::unique_ptr<juce::AlertWindow> m_editDialog;
        std::unique_ptr<BankItemsListBoxModel> m_listBox;
        std::unique_ptr<juce::Label> m_label;
        std::unique_ptr<juce::TextButton> m_btnLoadFile;
        std::unique_ptr<juce::FileChooser> m_fileChooser;

        std::function<void(void)> m_bankUpdatedCallback;
        std::function<void(std::string)> m_loadPresetCallback;
        
    public:
        void resized() override
        {
            juce::Rectangle<int> temp;
            const juce::Rectangle<int> bounds = getLocalBounds();
    
            temp = bounds;
            auto labelSpace = temp.removeFromTop(30);
            if (m_btnLoadFile) {
                m_btnLoadFile->setButtonText(TRANS("Browse"));
                m_btnLoadFile->setBounds(labelSpace.removeFromRight(80).withTrimmedTop(3).withTrimmedBottom(3));
            }
            m_label->setBounds(labelSpace);
            m_listBox->setBounds(temp);
        }

        void setBankUpdatedCallback(std::function<void(void)> bankUpdatedCallback)
        {
            m_bankUpdatedCallback = bankUpdatedCallback;
        }

        void setLoadPresetCallback(std::function<void(std::string)> loadPresetCallback)
        {
            m_loadPresetCallback = loadPresetCallback;
        }

        void setLabelTooltip(juce::String tooltip) {
            m_label->setTooltip(tooltip);
        }

        void chooseFileAndLoad() {
            juce::File initialPath;
            if (m_file != juce::File{}) {
                initialPath = m_file.getParentDirectory();
            }
            m_fileChooser.reset(new juce::FileChooser(TRANS("Open bank..."), initialPath));
            m_fileChooser->launchAsync(
                juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
                [this](const juce::FileChooser &chooser) {
                    juce::File result = chooser.getResult();
                    if (result != juce::File()) {
                        if (this) {
                            this->setFile(result);
                        }
                    }
                }
            );
        }

        ysfx_bank_shared getBank()
        {
            return m_bank;
        }

        void transferPresets(std::vector<int> indices, juce::WeakReference<juce::Component> ref)
        {
            if (!m_bank) return;
            
            ref->getName();
            LoadedBank* loadedBank = dynamic_cast<LoadedBank*>(ref->getParentComponent());

            ysfx_bank_shared src_bank = loadedBank->getBank();
            if (!src_bank) return;

            transferPresetRecursive(indices, src_bank);
        }

        void deletePresets(std::vector<int> indices)
        {
            if (!m_bank) return;

            // Grab the names first.
            std::vector<std::string> names;
            for (uint32_t idx : indices)
            {
                if (idx < m_bank->preset_count) {
                    names.push_back(std::string(m_bank->presets[idx].name));
                }
            }

            if (names.empty()) return;

            juce::AlertWindow::showAsync
            (
                juce::MessageBoxOptions()
                .withTitle("Are you certain?")
                .withMessage(
                    TRANS("Are you certain you want to delete ") + 
                    ((names.size() > 1) ? TRANS("several presets") : names[0])
                    + "\n" + TRANS("This operation cannot be undone!")
                )
                .withButton("Yes")
                .withButton("No")
                .withParentComponent(this)
                .withIconType(juce::MessageBoxIconType::NoIcon),
                [this, names](int result){
                    if (result == 1) {
                        for (auto name : names)
                        {
                            m_bank.reset(ysfx_delete_preset_from_bank(m_bank.get(), name.c_str()));
                        }

                        this->m_listBox->deselectAllRows();
                        save_bank(m_file.getFullPathName().toStdString().c_str(), m_bank.get());
                        if (m_bankUpdatedCallback) m_bankUpdatedCallback();
                    }
                }
            );
        }

        void renamePreset(int row) {
            if (!m_bank) return;
            if (row > static_cast<int>(m_bank->preset_count)) return;

            std::string currentPreset{m_bank->presets[row].name};
            
            m_editDialog.reset(
                show_async_text_input(
                    "Enter new name",
                    "",
                    [this, currentPreset](juce::String presetName, bool wantRename) {
                        if (wantRename) {
                            m_bank.reset(ysfx_rename_preset_from_bank(m_bank.get(), currentPreset.c_str(), presetName.toStdString().c_str()));
                            this->m_listBox->deselectAllRows();
                            save_bank(m_file.getFullPathName().toStdString().c_str(), m_bank.get());
                            if (m_bankUpdatedCallback) m_bankUpdatedCallback();
                        }
                    },
                    [this](juce::String presetName) {
                        if (ysfx_preset_exists(m_bank.get(), presetName.toStdString().c_str())) {
                            return juce::String("Preset with that name already exists.\nChoose a different name or click cancel.");
                        } else {
                            return juce::String("");
                        }
                    }
                )
            );
        }

        void createUI(bool withLoad)
        {
            m_listBox.reset(new BankItemsListBoxModel());
            m_label.reset(new juce::Label);
            m_label->setText(TRANS("No RPL loaded"), juce::dontSendNotification);
            if (withLoad) {
                m_btnLoadFile.reset(new juce::TextButton());
                m_btnLoadFile->onClick = [this]() { chooseFileAndLoad(); };
                addAndMakeVisible(*m_btnLoadFile);
            }
            m_listBox->setOutlineThickness(1);
            m_listBox->setDropCallback([this](std::vector<int> indices, juce::WeakReference<juce::Component> ref) { this->transferPresets(indices, ref); });
            m_listBox->setDeleteCallback([this](std::vector<int> indices) { this->deletePresets(indices); });
            m_listBox->setRenameCallback([this](int row) { this->renamePreset(row); });
            m_listBox->setDoubleClickCallback([this](int idx) { if (m_loadPresetCallback) m_loadPresetCallback(std::string{m_bank->presets[idx].name}); });
            addAndMakeVisible(*m_listBox);
            addAndMakeVisible(*m_label);
        }

        void resetLoadTime()
        {
            m_lastLoad = juce::Time(0);
        }

        void tryRead()
        {
            if (m_file == juce::File{}) {
                m_listBox->setItems({});
                m_listBox->updateContent();
                repaint();
                return;
            }

            juce::Time newTime = m_file.getLastModificationTime();
            if (newTime <= m_lastLoad) {
                return;
            }
            m_lastLoad = newTime;

            ysfx_bank_t* bank = load_bank(m_file.getFullPathName().toStdString().c_str());
            if (!bank)
                return;
            
            m_bank = make_ysfx_bank_shared(bank);

            std::vector<juce::String> names;
            for (uint32_t i = 0; i < m_bank->preset_count; ++i) {
                names.push_back(juce::String::fromUTF8(m_bank->presets[i].name));
            }
            m_listBox->setItems(names);
            m_listBox->updateContent();           
            m_label->setText(m_file.getFileName() + juce::String(" (") + juce::String(bank->name) + juce::String(")"), juce::dontSendNotification);
            repaint();
        }

        void setFile(juce::File file)
        {
            if (m_file != file) {
                m_lastLoad = juce::Time(0);  // Force a reload
                m_file = file;
                tryRead();
            }
        }

    private:
        void transferPresetRecursive(std::vector<int> indices, ysfx_bank_shared src_bank)
        {
            uint32_t idx = indices.back();
            indices.pop_back();

            auto copy_lambda = [this, indices, src_bank, idx](int result){
                if (result == 1) {
                    m_bank.reset(ysfx_add_preset_to_bank(m_bank.get(), src_bank->presets[idx].name, src_bank->presets[idx].state));
                }

                if (indices.empty()) {
                    save_bank(m_file.getFullPathName().toStdString().c_str(), m_bank.get());
                    if (m_bankUpdatedCallback) m_bankUpdatedCallback();
                } else {
                    this->transferPresetRecursive(indices, src_bank);
                }
            };

            if (idx < src_bank->preset_count) {
                if (ysfx_preset_exists(m_bank.get(), src_bank->presets[idx].name)) {
                    // Ask for overwrite
                    juce::AlertWindow::showAsync
                    (
                        juce::MessageBoxOptions()
                        .withTitle("Are you certain?")
                        .withMessage(TRANS("Are you certain you want to overwrite the preset named ") + juce::String(src_bank->presets[idx].name) + "?")
                        .withButton("Yes")
                        .withButton("No")
                        .withParentComponent(this)
                        .withIconType(juce::MessageBoxIconType::NoIcon),
                        copy_lambda
                    );
                } else {
                    copy_lambda(1);  // No need to ask
                }
            }
        }
};

struct YsfxRPLView::Impl {
    YsfxRPLView *m_self = nullptr;

    ysfx_u m_fx;

    LoadedBank m_left;
    LoadedBank m_right;

    std::unique_ptr<juce::Timer> m_relayoutTimer;
    std::unique_ptr<juce::Timer> m_fileCheckTimer;
    std::function<void(void)> m_bankUpdateCallback;
    std::function<void(std::string)> m_loadPresetCallback;

    //==========================================================================
    void createUI();
    void relayoutUI();
    void relayoutUILater();
    void checkFileForModifications();
    void setupNewFx();
};

YsfxRPLView::YsfxRPLView()
    : m_impl(new Impl)
{
    m_impl->m_self = this;

    m_impl->createUI();
    m_impl->relayoutUILater();

    m_impl->setupNewFx();
    this->setVisible(false);
}

YsfxRPLView::~YsfxRPLView()
{
}

void YsfxRPLView::setEffect(ysfx_t *fx)
{
    if (m_impl->m_fx.get() == fx)
        return;

    m_impl->m_fx.reset(fx);
    if (fx)
        ysfx_add_ref(fx);

    m_impl->setupNewFx();
}

void YsfxRPLView::setBankUpdateCallback(std::function<void(void)> bankUpdateCallback) {
     m_impl->m_bankUpdateCallback = bankUpdateCallback;
}

void YsfxRPLView::setLoadPresetCallback(std::function<void(std::string)> loadPresetCallback) {
    m_impl->m_loadPresetCallback = loadPresetCallback;
}

void YsfxRPLView::Impl::createUI()
{
    m_left.createUI(false);
    m_left.setLabelTooltip("Location of the currently loaded presets");
    m_self->addAndMakeVisible(m_left);
    m_left.setBankUpdatedCallback([this](void) { if (m_bankUpdateCallback) m_bankUpdateCallback(); });
    m_left.setLoadPresetCallback([this](std::string name) { if (m_loadPresetCallback) this->m_loadPresetCallback(name); });

    m_right.createUI(true);
    m_right.setLabelTooltip("Click to select preset file to import from");
    m_self->addAndMakeVisible(m_right);

    juce::Timer *timer = FunctionalTimer::create([this]() { checkFileForModifications(); });
    m_fileCheckTimer.reset(timer);
    timer->startTimer(100);
}

void YsfxRPLView::Impl::setupNewFx()
{
    m_left.resetLoadTime();
    m_right.resetLoadTime();
}

void YsfxRPLView::Impl::checkFileForModifications()
{
    ysfx_t *fx = m_fx.get();
    if (!fx)
        return;

    // Check if we have a customized bank
    const char *bankpath = juce::CharPointer_UTF8{ysfx_get_bank_path(fx)};
    juce::File customBankPath = getCustomBankLocation(fx);

    if (customBankPath.existsAsFile()) {
        m_left.setFile(customBankPath);
    } else {
        m_left.setFile(juce::File{bankpath});
    }

    m_left.tryRead();
    m_right.tryRead();
}

void YsfxRPLView::resized()
{
    m_impl->relayoutUILater();
}

void YsfxRPLView::focusOnPresetViewer()
{
}

void YsfxRPLView::Impl::relayoutUI()
{
    juce::Rectangle<int> temp;
    const juce::Rectangle<int> bounds = m_self->getLocalBounds();
    
    temp = bounds;
    temp.removeFromRight(bounds.getWidth() / 2);
    m_left.setBounds(temp);

    temp = bounds;
    temp.removeFromLeft(bounds.getWidth() / 2);
    m_right.setBounds(temp);

    if (m_relayoutTimer)
        m_relayoutTimer->stopTimer();
}


void YsfxRPLView::Impl::relayoutUILater()
{
    if (!m_relayoutTimer)
        m_relayoutTimer.reset(FunctionalTimer::create([this]() { relayoutUI(); }));
    m_relayoutTimer->startTimer(0);
}

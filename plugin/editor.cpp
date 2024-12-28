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

#include "editor.h"
#include "lookandfeel.h"
#include "processor.h"
#include "parameter.h"
#include "info.h"
#include "components/parameters_panel.h"
#include "components/graphics_view.h"
#include "components/ide_view.h"
#include "components/rpl_view.h"
#include "components/searchable_popup.h"
#include "components/modal_textinputbox.h"
#include "components/divider.h"
#include "utility/functional_timer.h"
#include "ysfx.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include "json.hpp"
#include <iostream>
#include <cmath>

struct YsfxEditor::Impl {
    YsfxEditor *m_self = nullptr;
    YsfxProcessor *m_proc = nullptr;
    YsfxInfo::Ptr m_info;
    YsfxCurrentPresetInfo::Ptr m_currentPresetInfo;
    ysfx_bank_shared m_bank;
    std::unique_ptr<juce::AlertWindow> m_editDialog;
    std::unique_ptr<juce::AlertWindow> m_modalAlert;
    std::unique_ptr<juce::Timer> m_infoTimer;
    std::unique_ptr<juce::Timer> m_relayoutTimer;
    std::unique_ptr<juce::FileChooser> m_fileChooser;
    std::unique_ptr<juce::PopupMenu> m_recentFilesPopup;
    std::unique_ptr<juce::PopupMenu> m_recentFilesOptsPopup;
    std::unique_ptr<juce::PopupMenu> m_recentFilesOptsSubmenuPopup;
    std::unique_ptr<juce::PopupMenu> m_presetsPopup;
    std::unique_ptr<juce::PopupMenu> m_presetsOptsPopup;
    std::unique_ptr<juce::PropertiesFile> m_pluginProperties;
    bool m_fileChooserActive = false;
    bool m_mustResizeToGfx = true;
    bool m_maintainState = false;
    float m_currentScaling{1.0f};
    uint64_t m_sliderVisible[ysfx_max_slider_groups]{0};
    bool m_visibleSlidersChanged{false};
    bool m_showUndo{false};
    bool m_showRedo{false};

    //==========================================================================
    void updateInfo();
    void grabInfoAndUpdate();
    void chooseFileAndLoad();
    void loadFile(const juce::File &file, bool keepState);
    void popupRecentFiles();
    void popupRecentOpts();
    void popupPresets();
    void popupPresetOptions();
    void switchEditor(bool showGfx);
    void openCodeEditor();
    void openPresetWindow();
    void quickAlertBox(bool confirmationRequired, std::function<void()> callbackOnSuccess, juce::Component* parent);
    static juce::File getAppDataDirectory();
    static juce::File getDefaultEffectsDirectory();
    juce::RecentlyOpenedFilesList loadRecentFiles();
    void saveRecentFiles(const juce::RecentlyOpenedFilesList &recent);
    void clearRecentFiles();
    void setScale(float newScaling);
    void loadScaling();
    void saveScaling();
    void resetScaling(const juce::File &jsfxFilePath);
    juce::String getJsfxName();

    //==========================================================================
    class SubWindow : public juce::DocumentWindow {
    public:
        using juce::DocumentWindow::DocumentWindow;

    protected:
        void closeButtonPressed() override { setVisible(false); }
    };

    //==========================================================================
    std::unique_ptr<juce::TextButton> m_btnLoadFile;
    std::unique_ptr<juce::TextButton> m_btnRecentFiles;
    std::unique_ptr<juce::TextButton> m_btnRecentFilesOpts;
    std::unique_ptr<juce::TextButton> m_btnEditCode;
    std::unique_ptr<juce::TextButton> m_btnLoadPreset;
    std::unique_ptr<juce::TextButton> m_btnPresetOpts;
    std::unique_ptr<juce::TextButton> m_btnSwitchEditor;
    std::unique_ptr<juce::TextButton> m_btnReload;
    std::unique_ptr<juce::TextButton> m_btnUndo;
    std::unique_ptr<juce::TextButton> m_btnRedo;
    std::unique_ptr<juce::TextButton> m_btnGfxScaling;

    std::unique_ptr<juce::Label> m_lblFilePath;
    std::unique_ptr<juce::Label> m_lblIO;
    std::unique_ptr<juce::Viewport> m_centerViewPort;
    std::unique_ptr<juce::Viewport> m_topViewPort;
    std::unique_ptr<Divider> m_divider; 
    std::unique_ptr<YsfxParametersPanel> m_parametersPanel;
    std::unique_ptr<YsfxParametersPanel> m_miniParametersPanel;
    std::unique_ptr<YsfxGraphicsView> m_graphicsView;
    std::unique_ptr<YsfxIDEView> m_ideView;
    std::unique_ptr<SubWindow> m_codeWindow;
    std::unique_ptr<YsfxRPLView> m_rplView;
    std::unique_ptr<SubWindow> m_presetWindow;
    std::unique_ptr<juce::TooltipWindow> m_tooltipWindow;

    //==========================================================================
    void createUI();
    void connectUI();
    void relayoutUI();
    void relayoutUILater();
    void initializeProperties();
    juce::String getLabel() const;
};

static const int defaultEditorWidth = 700;
static const int defaultEditorHeight = 50;

YsfxEditor::YsfxEditor(YsfxProcessor &proc)
    : juce::AudioProcessorEditor(proc),
      m_impl(new Impl)
{
    m_impl->m_self = this;
    m_impl->m_proc = &proc;
    m_impl->m_info = proc.getCurrentInfo();
    m_impl->m_currentPresetInfo = proc.getCurrentPresetInfo();
    m_impl->m_bank = proc.getCurrentBank();

    static YsfxLookAndFeel lnf;
    setLookAndFeel(&lnf);
    juce::LookAndFeel::setDefaultLookAndFeel(&lnf);

    setOpaque(true);
    setSize(defaultEditorWidth, defaultEditorHeight);
    setResizable(true, true);
    m_impl->createUI();
    m_impl->connectUI();
    m_impl->relayoutUILater();
    m_impl->initializeProperties();
    m_impl->updateInfo();

    readTheme();
}

void writeThemeFile(juce::File file, std::map<std::string, std::array<uint8_t, 3>> colors, std::map<std::string, float> params)
{
    juce::FileOutputStream stream(file);
    stream.setPosition(0);
    stream.truncate();
    nlohmann::json json_colors{colors};
    nlohmann::json json_params{params};

    nlohmann::json json_file;
    json_file["version"] = 1;
    json_file["colors"] = json_colors;
    json_file["params"] = json_params;

    stream.writeString(juce::String{json_file.dump(4)});
}

void YsfxEditor::readTheme()
{
    if (!m_impl) return;

    juce::File dir = m_impl->getAppDataDirectory();
    if (dir == juce::File{})
        return;
        
    juce::File file = dir.getChildFile("theme.json");
    dir.createDirectory();

    if (!file.existsAsFile()) {
        try {
            writeThemeFile(file, getDefaultColors(), getDefaultParams());
            setColors(getLookAndFeel(), getDefaultColors());
            setParams(getLookAndFeel(), getDefaultParams());
            m_impl->m_ideView->setColourScheme(getDefaultColors());
        } catch (const nlohmann::json::exception& e) {
            std::cerr << "Failed to write theme: " << e.what() << std::endl;
        }
    } else {
        juce::FileInputStream stream(file);
        juce::String text = stream.readEntireStreamAsString();

        try {
            auto jsonFile = nlohmann::json::parse(text.toStdString());
            // Fallback for version 1 files (upconvert the file)
            if (!jsonFile.contains("version")) {
                auto readTheme = jsonFile[0].get<std::map<std::string, std::array<uint8_t, 3>>>();
                readTheme = fillMissingColors(readTheme);
                writeThemeFile(file, readTheme, getDefaultParams());

                // Reread it!
                stream.setPosition(0);
                text = stream.readEntireStreamAsString();
                jsonFile = nlohmann::json::parse(text.toStdString());
            }
            
            auto readTheme = jsonFile.at("colors")[0].get<std::map<std::string, std::array<uint8_t, 3>>>();
            auto readParams = jsonFile.at("params")[0].get<std::map<std::string, float>>();
            readTheme = fillMissingColors(readTheme);
            readParams = fillMissingParams(readParams);

            setColors(getLookAndFeel(), readTheme);
            setParams(getLookAndFeel(), readParams);
            m_impl->m_ideView->setColourScheme(readTheme);
            writeThemeFile(file, readTheme, readParams);
        } catch (const nlohmann::json::exception& e) {
            std::cerr << "Failed to read theme: " << e.what() << std::endl;
        }
    }
}

void YsfxEditor::paint (juce::Graphics& g)
{
    // Redraw only parts that aren't covered already.
    const juce::Rectangle<int> bounds = getLocalBounds();
    g.setOpacity(1.0f);
    g.setColour(juce::Colour(0, 0, 0));
    g.fillRect(juce::Rectangle<int>(0, 0, m_headerSize, bounds.getHeight() - m_headerSize));
    g.fillRect(juce::Rectangle<int>(bounds.getWidth() - 20, m_headerSize, 20, bounds.getHeight() - m_headerSize));
    g.fillRect(juce::Rectangle<int>(0, bounds.getHeight() - 20, bounds.getWidth(), 20));

    g.setColour(juce::Colour(32, 32, 32));
    g.setColour(this->findColour(juce::DocumentWindow::backgroundColourId));
    g.fillRect(juce::Rectangle<int>(0, 0, bounds.getWidth(), m_headerSize));
}

YsfxEditor::~YsfxEditor()
{
    if (m_impl) {
        m_impl->saveScaling();
    }
}

//------------------------------------------------------------------------------
bool YsfxEditor::isInterestedInFileDrag(const juce::StringArray &files)
{
    (void)files;

    YsfxInfo::Ptr info = m_impl->m_info;
    ysfx_t *fx = info->effect.get();

    return !ysfx_is_compiled(fx);
}

void YsfxEditor::filesDropped(const juce::StringArray &files, int x, int y)
{
    (void)x;
    (void)y;
    
    // We only allow jsfx drops if no JSFX was loaded.
    YsfxInfo::Ptr info = m_impl->m_info;
    ysfx_t *fx = info->effect.get();

    if (!ysfx_is_compiled(fx)) {
        if (files.size() == 1) {
            juce::File file{files[0]};
            if (file.existsAsFile()) {
                m_impl->loadFile(files[0], false);
            }   
        }
    }
}

void YsfxEditor::resized()
{
    m_impl->relayoutUILater();
}

juce::String YsfxEditor::Impl::getJsfxName()
{
    YsfxInfo::Ptr info = m_info;
    return info->m_name;
}

juce::String YsfxEditor::Impl::getLabel() const
{
    if (!m_info) return juce::String("");

    juce::String label{m_info->m_name};

    if (m_currentPresetInfo) {
        if (m_currentPresetInfo->m_lastChosenPreset.isNotEmpty()) {
            label += "\n" + m_currentPresetInfo->m_lastChosenPreset;
        };
    };

    return label;
}

void YsfxEditor::Impl::grabInfoAndUpdate()
{
    YsfxInfo::Ptr info = m_proc->getCurrentInfo();
    YsfxCurrentPresetInfo::Ptr presetInfo = m_proc->getCurrentPresetInfo();
    ysfx_bank_shared bank = m_proc->getCurrentBank();

    if (std::abs(m_graphicsView->getTotalScaling() - m_currentScaling) > 1e-6) {
        relayoutUILater();
        m_currentScaling = m_graphicsView->getTotalScaling();
    }

    if (m_currentPresetInfo != presetInfo) {
        m_currentPresetInfo = presetInfo;
    }

    if ((m_showUndo != m_proc->canUndo()) || (m_showRedo != m_proc->canRedo())) {
        m_showUndo = m_proc->canUndo();
        m_showRedo = m_proc->canRedo();
        relayoutUI();
    }
    
    if (m_info != info) {
        m_info = info;
        updateInfo();
        m_btnLoadFile->setButtonText(TRANS("Load"));
        m_btnRecentFiles->setVisible(true);
    }
    if (m_bank != bank) {
        m_bank = bank;
        updateInfo();
    }

    for (uint8_t i = 0; i < ysfx_max_slider_groups; i++) {
        ysfx_t *fx = m_info.get()->effect.get();

        if (fx) {
            uint64_t newValue = ysfx_get_slider_visibility(fx, i);
            if (newValue != m_sliderVisible[i]) {
                m_sliderVisible[i] = newValue;
                m_visibleSlidersChanged = true;
            }
            if (m_visibleSlidersChanged) relayoutUILater();
        }
    }
    
    m_lblFilePath->setText(getLabel(), juce::dontSendNotification);
    
    if ((m_proc->retryLoad() == RetryState::mustRetry) && !m_fileChooserActive) {
        chooseFileAndLoad();
        m_btnLoadFile->setButtonText(TRANS("Locate"));
        m_btnRecentFiles->setVisible(false);
    }
}

void YsfxEditor::Impl::updateInfo()
{
    YsfxInfo *info = m_info.get();
    ysfx_t *fx = info->effect.get();

    juce::File filePath{juce::CharPointer_UTF8{ysfx_get_file_path(fx)}};

    if (filePath != juce::File{}) {
        m_lblFilePath->setTooltip(filePath.getFullPathName());
        m_self->getTopLevelComponent()->setName(juce::String(ysfx_get_name(fx)) + " (ysfx)");
    }
    else {
        m_lblFilePath->setText(TRANS("No file"), juce::dontSendNotification);
        m_lblFilePath->setTooltip(juce::String{});
    }

    juce::String ioText;
    uint32_t numInputs = ysfx_get_num_inputs(fx);
    uint32_t numOutputs = ysfx_get_num_inputs(fx);
    if (numInputs != 0 && numOutputs != 0)
        ioText = juce::String(numInputs) + " in " + juce::String(numOutputs) + " out";
    else if (numInputs != 0)
        ioText = juce::String(numInputs) + " in";
    else if (numOutputs != 0)
        ioText = juce::String(numOutputs) + " out";
    else
        ioText = "MIDI";
    m_lblIO->setText(ioText, juce::dontSendNotification);

    m_presetsPopup.reset();

    juce::Array<YsfxParameter *> params;
    params.ensureStorageAllocated(ysfx_max_sliders);
    for (uint32_t i = 0; i < ysfx_max_sliders; ++i) {
        if (ysfx_slider_exists(fx, i))
            params.add(m_proc->getYsfxParameter((int)i));
    }
    m_parametersPanel->setParametersDisplayed(params);

    juce::Array<YsfxParameter *> params2;
    params2.ensureStorageAllocated(ysfx_max_sliders);
    for (uint32_t i = 0; i < ysfx_max_sliders; ++i) {
        if (ysfx_slider_exists(fx, i) && ysfx_slider_is_initially_visible(fx, i))
            params2.add(m_proc->getYsfxParameter((int)i));
    }
    m_miniParametersPanel->setParametersDisplayed(params2);
    
    m_graphicsView->setEffect(fx);
    m_ideView->setEffect(fx, info->timeStamp, info->mainFile);

    if (!info->errors.isEmpty())
        m_ideView->setStatusText(info->errors.getReference(0));
    else if (!info->warnings.isEmpty())
        m_ideView->setStatusText(info->warnings.getReference(0));
    else
        m_ideView->setStatusText(TRANS("Compiled OK"));

    m_rplView->setEffect(fx);
    m_rplView->setBankUpdateCallback(
        [this](void){
            m_proc->reloadBank();
        }
    );
    m_rplView->setLoadPresetCallback(
        [this](std::string preset) {
            YsfxInfo::Ptr ysfx_info = m_info;
            ysfx_bank_shared bank = m_bank;
            if (!bank) return;

            auto index = ysfx_preset_exists(bank.get(), preset.c_str());
            if (index > 0) {
                m_proc->loadJsfxPreset(ysfx_info, bank, (uint32_t)(index - 1), PresetLoadMode::load, true);
            }
        }
    );

    // We always just want the sliders the user meant to expose
    if (!m_maintainState) switchEditor(true);

    juce::File file{juce::CharPointer_UTF8{ysfx_get_file_path(fx)}};
    m_mustResizeToGfx = true;

    loadScaling();
    relayoutUILater();
}

void YsfxEditor::Impl::quickAlertBox(bool confirmationRequired, std::function<void()> callbackOnSuccess, juce::Component* parent)
{
    if (confirmationRequired) {
        m_modalAlert.reset(
            show_option_window(
                "Are you certain?",
                "Are you certain you want to (re)load the plugin?\n\nNote that you will lose your current preset.",
                std::vector<juce::String>{"Yes", "No"},
                [callbackOnSuccess](int result){
                    if (result == 1) callbackOnSuccess();
                },
                parent
            )
        );
    } else {
        callbackOnSuccess();
    }
}

void YsfxEditor::Impl::chooseFileAndLoad()
{
    if (m_fileChooserActive)
        return;

    YsfxInfo::Ptr info = m_info;
    ysfx_t *fx = info->effect.get();

    juce::File initialPath;
    juce::File prevFilePath{juce::CharPointer_UTF8{ysfx_get_file_path(fx)}};
    if (prevFilePath != juce::File{}) {
        initialPath = prevFilePath.getParentDirectory();
    } else {
        if (m_pluginProperties->containsKey("load_path")) {
            initialPath = m_pluginProperties->getValue("load_path");
        }
        if (!initialPath.isDirectory()) {
            initialPath = getDefaultEffectsDirectory();
        }
    }

    bool normalLoad = (m_proc->retryLoad() == RetryState::ok);

    if (normalLoad) {
        m_fileChooser.reset(new juce::FileChooser(TRANS("Open jsfx..."), initialPath));
    } else {
        juce::File fullpath{m_proc->lastLoadPath()};
        m_fileChooser.reset(new juce::FileChooser(TRANS("JSFX missing! Please locate jsfx named ") + fullpath.getFileNameWithoutExtension(), fullpath.getParentDirectory(), fullpath.getFileName()));
    }
    
    bool mustAskConfirmation = normalLoad && ysfx_is_compiled(fx);
    m_fileChooserActive = true;
    m_fileChooser->launchAsync(
        juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
        [this, normalLoad, mustAskConfirmation](const juce::FileChooser &chooser) {
            juce::File result = chooser.getResult();
            if (result != juce::File()) {
                quickAlertBox(
                    mustAskConfirmation,
                    [this, normalLoad, result]() {
                        if (normalLoad) saveScaling();
                        loadFile(result, false);
                    },
                    this->m_self
                );
            }
            m_fileChooserActive = false;
        }
    );
}

void YsfxEditor::Impl::saveScaling()
{
    if (m_pluginProperties) {
        juce::String filename_without_ext = getJsfxName();
        if (filename_without_ext.isNotEmpty()) {
            auto getKey = [filename_without_ext](juce::String name) {return filename_without_ext + name;};
            {
                juce::ScopedLock lock{m_pluginProperties->getLock()};
                m_pluginProperties->setValue(getKey("_width"), m_self->getWidth());
                m_pluginProperties->setValue(getKey("_height"), m_self->getHeight());
                m_pluginProperties->setNeedsToBeSaved(true);
                m_pluginProperties->setValue(getKey("_divider"), m_divider->m_position);
            }
        }
    }
}

void YsfxEditor::Impl::resetScaling(const juce::File &jsfxFilePath)
{
    if (m_pluginProperties) {
        juce::String filename_without_ext = jsfxFilePath.getFileNameWithoutExtension();
        auto getKey = [filename_without_ext](juce::String name) {return filename_without_ext + name;};
        {
            juce::ScopedLock lock{m_pluginProperties->getLock()};
            m_pluginProperties->removeValue(getKey("_width"));
            m_pluginProperties->removeValue(getKey("_height"));
            m_pluginProperties->needsToBeSaved();
        }
    }
}

void YsfxEditor::Impl::loadScaling()
{
    if (m_pluginProperties) {
        juce::String filename_without_ext = getJsfxName();
        if (filename_without_ext.isNotEmpty()) {
            auto getKey = [filename_without_ext](juce::String name) {return filename_without_ext + name;};
            
            juce::String key = getKey("_scaling_factor");
            if (m_pluginProperties->containsKey(key)) {
                float scalingFactor = m_pluginProperties->getValue(key).getFloatValue();
                setScale(scalingFactor);
            } else {
                setScale(1.0f);
            }

            int width = m_pluginProperties->getValue(getKey("_width")).getIntValue();
            int height = m_pluginProperties->getValue(getKey("_height")).getIntValue();
            if (width && height) {
                m_self->setSize(width, height);
                m_mustResizeToGfx = false;
            }

            key = getKey("_divider");
            if (m_pluginProperties->containsKey(key)) {
                m_divider->setPosition((int) m_pluginProperties->getValue(key).getFloatValue());
            }
        }
    }
}

void YsfxEditor::Impl::loadFile(const juce::File &file, bool keepState)
{
    m_maintainState = keepState;

    {
        juce::ScopedLock lock{m_pluginProperties->getLock()};
        m_pluginProperties->setValue("load_path", file.getParentDirectory().getFullPathName());
        m_pluginProperties->save();
    }

    m_proc->loadJsfxFile(file.getFullPathName(), nullptr, true, keepState);
    relayoutUILater();

    juce::RecentlyOpenedFilesList recent = loadRecentFiles();
    recent.addFile(file);
    saveRecentFiles(recent);
}

void YsfxEditor::Impl::popupRecentFiles()
{
    m_recentFilesPopup.reset(new juce::PopupMenu);
    juce::RecentlyOpenedFilesList recent = loadRecentFiles();
    recent.createPopupMenuItems(*m_recentFilesPopup, 100, false, true);

    if (m_recentFilesPopup->getNumItems() == 0)
        return;

    juce::PopupMenu::Options popupOptions = juce::PopupMenu::Options{}
        .withTargetComponent(*m_btnRecentFiles);
    
    m_recentFilesPopup->showMenuAsync(popupOptions, [this, recent](int index) {
        if (index != 0) {
            juce::File selectedFile = recent.getFile(index - 100);
            quickAlertBox(
                ysfx_is_compiled(m_info->effect.get()),
                [this, selectedFile]() {
                    saveScaling();
                    loadFile(selectedFile, false);
                },
                this->m_self
            );
        }
    });
}

void YsfxEditor::Impl::popupRecentOpts()
{
    m_recentFilesOptsPopup.reset(new juce::PopupMenu);
    m_recentFilesOptsSubmenuPopup.reset(new juce::PopupMenu);

    juce::PopupMenu::Options popupOptions = juce::PopupMenu::Options{}
        .withTargetComponent(*m_btnRecentFilesOpts);

    juce::RecentlyOpenedFilesList recent = loadRecentFiles();
    recent.createPopupMenuItems(*m_recentFilesOptsSubmenuPopup, 100, false, true);

    m_recentFilesOptsPopup->addItem(1000, TRANS("Clear all items"), true, false);
    m_recentFilesOptsPopup->addSeparator();
    YsfxInfo::Ptr info = m_info;
    m_recentFilesOptsPopup->addSubMenu("Remove from recent", *m_recentFilesOptsSubmenuPopup, true);
    
    m_recentFilesOptsPopup->showMenuAsync(popupOptions, [this](int index) {
        if (index == 1000) {
            clearRecentFiles();
        } else if (index != 0) {
            juce::RecentlyOpenedFilesList recent_files = loadRecentFiles();
            juce::File file = recent_files.getFile(index - 100);
            recent_files.removeFile(file);
            saveRecentFiles(recent_files);
        }
    });
}

void YsfxEditor::Impl::popupPresetOptions()
{
    m_presetsOptsPopup.reset(new juce::PopupMenu);

    YsfxInfo::Ptr info = m_info;
    ysfx_bank_shared bank = m_bank;
    YsfxCurrentPresetInfo::Ptr presetInfo = m_currentPresetInfo;

    if (info->m_name.isNotEmpty()) {
        m_presetsOptsPopup->addItem(1, "Save preset", true, false);
        m_presetsOptsPopup->addItem(2, "Rename preset", presetInfo->m_lastChosenPreset.isNotEmpty(), false);
        m_presetsOptsPopup->addSeparator();
        m_presetsOptsPopup->addItem(3, "Next preset", true, false);
        m_presetsOptsPopup->addItem(4, "Previous preset", true, false);
        m_presetsOptsPopup->addSeparator();        
        m_presetsOptsPopup->addItem(5, "Delete preset", presetInfo->m_lastChosenPreset.isNotEmpty(), false);
        m_presetsOptsPopup->addSeparator();
        m_presetsOptsPopup->addItem(6, "Preset manager", true, false);
    }

    juce::PopupMenu::Options popupOptions = juce::PopupMenu::Options{}
        .withTargetComponent(*m_btnPresetOpts);

    m_presetsOptsPopup->showMenuAsync(
        popupOptions, [this, info](int index) 
        {
            switch (index) {
                case 1:
                    // Save
                    m_editDialog.reset(
                        show_async_text_input(
                            "Enter preset name",
                            "",
                            [this](juce::String presetName, bool wantSave) {
                                std::string preset = presetName.toStdString();
                                if (wantSave) {
                                    if (m_proc->presetExists(preset.c_str())) {
                                        this->m_modalAlert.reset(
                                            show_option_window(
                                                "Overwrite?",
                                                "Preset with that name already exists.\nAre you sure you want to overwrite the preset?",
                                                std::vector<juce::String>{"Yes", "No"},
                                                [this, preset](int result){
                                                    if (result == 1) m_proc->saveCurrentPreset(preset.c_str());
                                                },
                                                this->m_self
                                            )
                                        );
                                    } else {
                                        m_proc->saveCurrentPreset(preset.c_str());
                                    }
                                }
                            },
                            std::nullopt,
                            this->m_self
                        )
                    );
                    return;
                case 2:
                    // Rename
                    m_editDialog.reset(
                        show_async_text_input(
                            "Enter new name",
                            "",
                            [this](juce::String presetName, bool wantRename) {
                                std::string preset = presetName.toStdString();
                                if (wantRename) {
                                    m_proc->renameCurrentPreset(preset.c_str());
                                }
                            },
                            [this](juce::String presetName) {
                                if (m_proc->presetExists(presetName.toStdString().c_str())) {
                                    return juce::String("Preset with that name already exists.\nChoose a different name or click cancel.");
                                } else {
                                    return juce::String("");
                                }
                            },
                            this->m_self
                        )
                    );
                    break;
                case 3:
                    m_proc->cyclePreset(1);
                    break;
                case 4:
                    m_proc->cyclePreset(-1);
                    break;                    
                case 5:
                    // Delete
                    this->m_modalAlert.reset(
                        show_option_window(
                            "Delete?",
                            "Are you sure you want to delete the preset named " + m_currentPresetInfo->m_lastChosenPreset + "?",
                            std::vector<juce::String>{"Yes", "No"},
                            [this](int result) {
                                if (result == 1) m_proc->deleteCurrentPreset();
                            },
                            this->m_self
                        )
                    );
                    break;
                case 6:
                    this->openPresetWindow();
                    break;
                default:
                    break;
            }
        }
    );
}

void YsfxEditor::Impl::popupPresets()
{
    YsfxInfo::Ptr info = m_info;
    ysfx_bank_shared bank = m_bank;
    YsfxCurrentPresetInfo::Ptr presetInfo = m_currentPresetInfo;

    m_presetsPopup.reset(new juce::PopupMenu);
    if (!bank) {
        m_presetsPopup->addItem(32767, TRANS("No presets"), false);
    } else {
        for (uint32_t i = 0; i < bank->preset_count; ++i) {
            bool wasLastChosen = presetInfo->m_lastChosenPreset.compare(bank->presets[i].name) == 0;
            m_presetsPopup->addItem((int)(i + 1), juce::String::fromUTF8(bank->presets[i].name), true, wasLastChosen);
        }
    }
   
    juce::PopupMenu::Options quickSearchOptions = PopupMenuQuickSearchOptions{}
        .withTargetComponent(*m_btnLoadPreset);

    showPopupMenuWithQuickSearch(*m_presetsPopup, quickSearchOptions, [this, info, bank](int index) {
            if ((index > 0) && (index < 32767)) {
                m_proc->loadJsfxPreset(info, bank, (uint32_t)(index - 1), PresetLoadMode::load, true);
            }
        }
    );
}

void YsfxEditor::Impl::switchEditor(bool showGfx)
{
    juce::String text = showGfx ? TRANS("Graphics") : TRANS("Sliders");
    m_btnSwitchEditor->setButtonText(text);
    m_btnSwitchEditor->setToggleState(showGfx, juce::dontSendNotification);

    relayoutUILater();
}

void YsfxEditor::Impl::openCodeEditor()
{
    if (!m_codeWindow) {
        m_codeWindow.reset(new SubWindow(TRANS("Edit"), m_self->findColour(juce::DocumentWindow::backgroundColourId), juce::DocumentWindow::allButtons));
        m_codeWindow->setResizable(true, false);
        m_codeWindow->setContentNonOwned(m_ideView.get(), true);
    }

    m_codeWindow->setVisible(true);
    m_codeWindow->toFront(true);
    m_codeWindow->setAlwaysOnTop(true);
    m_ideView->focusOnCodeEditor();
}

void YsfxEditor::Impl::openPresetWindow()
{
    if (!m_presetWindow) {
        m_presetWindow.reset(new SubWindow(TRANS("Preset Manager"), m_self->findColour(juce::DocumentWindow::backgroundColourId), juce::DocumentWindow::allButtons));
        m_presetWindow->setResizable(true, false);
        m_presetWindow->setContentNonOwned(m_rplView.get(), true);
    }

    m_presetWindow->setVisible(true);
    m_presetWindow->toFront(true);
}

juce::RecentlyOpenedFilesList YsfxEditor::Impl::loadRecentFiles()
{
    juce::RecentlyOpenedFilesList recent;

    juce::File dir = getAppDataDirectory();
    if (dir == juce::File{})
        return recent;

    juce::File file = dir.getChildFile("PluginRecentFiles.dat");
    juce::FileInputStream stream(file);
    juce::String text = stream.readEntireStreamAsString();
    recent.restoreFromString(text);
    return recent;
}

void YsfxEditor::Impl::saveRecentFiles(const juce::RecentlyOpenedFilesList &recent)
{
    juce::File dir = getAppDataDirectory();
    if (dir == juce::File{})
        return;

    juce::File file = dir.getChildFile("PluginRecentFiles.dat");
    dir.createDirectory();
    juce::FileOutputStream stream(file);
    stream.setPosition(0);
    stream.truncate();
    juce::String text = recent.toString();
    stream.write(text.toRawUTF8(), text.getNumBytesAsUTF8());
}

void YsfxEditor::Impl::clearRecentFiles()
{
    juce::File dir = getAppDataDirectory();
    if (dir == juce::File{})
        return;

    juce::File file = dir.getChildFile("PluginRecentFiles.dat");
    file.deleteFile();
}

juce::File YsfxEditor::Impl::getAppDataDirectory()
{
    juce::File dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    if (dir == juce::File{})
        return juce::File{};

    return dir.getChildFile("ysfx_saike_mod");
}

juce::File YsfxEditor::Impl::getDefaultEffectsDirectory()
{
#if !JUCE_MAC
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("REAPER/Effects");
#else
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Application Support/REAPER/Effects");
#endif
}

void YsfxEditor::Impl::initializeProperties()
{
    juce::PropertiesFile::Options options;

    options.applicationName = "ysfx_saike_mod";
    options.storageFormat = juce::PropertiesFile::StorageFormat::storeAsXML;
    options.filenameSuffix = ".prefs";
    options.osxLibrarySubFolder = "Application Support";
    #if JUCE_LINUX
    options.folderName = "~/.config";
    #else
    options.folderName = "";
    #endif
    
    m_pluginProperties.reset(new juce::PropertiesFile{options});
}

void YsfxEditor::Impl::createUI()
{
    m_btnLoadFile.reset(new juce::TextButton(TRANS("Load")));
    m_self->addAndMakeVisible(*m_btnLoadFile);
    m_btnRecentFiles.reset(new juce::TextButton(TRANS("Recent")));
    m_self->addAndMakeVisible(*m_btnRecentFiles);
    m_btnReload.reset(new juce::TextButton(TRANS("Reload")));
    m_self->addAndMakeVisible(*m_btnReload);
    m_btnUndo.reset(new juce::TextButton(TRANS("U")));
    m_self->addAndMakeVisible(*m_btnUndo);
    m_btnRedo.reset(new juce::TextButton(TRANS("R")));
    m_self->addAndMakeVisible(*m_btnRedo);
    m_btnEditCode.reset(new juce::TextButton(TRANS("Edit")));
    m_self->addAndMakeVisible(*m_btnEditCode);
    m_btnGfxScaling.reset(new juce::TextButton(TRANS("x1")));
    m_self->addAndMakeVisible(*m_btnGfxScaling);
    m_btnGfxScaling->setTooltip("Render JSFX UI at lower resolution and upscale the result. Ths is intended for JSFX that do not implement scaling themselves. For JSFX that do, it is better to simply resize the plugin.");
    m_btnLoadPreset.reset(new juce::TextButton(TRANS("Preset")));
    m_self->addAndMakeVisible(*m_btnLoadPreset);
    m_btnPresetOpts.reset(new juce::TextButton(TRANS(juce::CharPointer_UTF8("\xe2\x96\xBC"))));
    m_self->addAndMakeVisible(*m_btnPresetOpts);
    m_btnRecentFilesOpts.reset(new juce::TextButton(TRANS(juce::CharPointer_UTF8("\xe2\x96\xBC"))));
    m_self->addAndMakeVisible(*m_btnRecentFilesOpts);
    m_btnSwitchEditor.reset(new juce::TextButton(TRANS("Sliders")));
    m_btnSwitchEditor->setClickingTogglesState(true);
    m_self->addAndMakeVisible(*m_btnSwitchEditor);
    m_lblFilePath.reset(new juce::Label);
    m_lblFilePath->setMinimumHorizontalScale(1.0f);
    m_lblFilePath->setJustificationType(juce::Justification::centred);
    m_self->addAndMakeVisible(*m_lblFilePath);
    m_lblIO.reset(new juce::Label);
    m_lblIO->setMinimumHorizontalScale(1.0f);
    m_lblIO->setJustificationType(juce::Justification::horizontallyCentred);
    m_lblIO->setColour(juce::Label::outlineColourId, m_self->findColour(juce::ComboBox::outlineColourId));
    m_self->addAndMakeVisible(*m_lblIO);
    m_centerViewPort.reset(new juce::Viewport);
    m_centerViewPort->setScrollBarsShown(true, false);
    m_self->addAndMakeVisible(*m_centerViewPort);
    m_topViewPort.reset(new juce::Viewport);
    m_topViewPort->setScrollBarsShown(true, false);
    m_self->addAndMakeVisible(*m_topViewPort);

    m_divider.reset(new Divider(m_self));
    m_topViewPort->addAndMakeVisible(m_divider.get());

    m_parametersPanel.reset(new YsfxParametersPanel);
    m_miniParametersPanel.reset(new YsfxParametersPanel);
    m_graphicsView.reset(new YsfxGraphicsView);
    m_ideView.reset(new YsfxIDEView);
    m_ideView->setSize(1000, 600);
    m_tooltipWindow.reset(new juce::TooltipWindow);

    m_rplView.reset(new YsfxRPLView);
    m_rplView->setSize(1000, 600);
}

void YsfxEditor::Impl::setScale(float newScaling)
{
    newScaling = ((newScaling < 1.0f) || (newScaling > 2.1f)) ? 1.0f : newScaling;
    m_graphicsView->setScaling(newScaling);
    m_btnGfxScaling->setButtonText(TRANS(juce::String::formatted("%.1f", newScaling)));
}

void YsfxEditor::Impl::connectUI()
{
    m_btnLoadFile->onClick = [this]() { chooseFileAndLoad(); };
    m_btnRecentFiles->onClick = [this]() { popupRecentFiles(); };
    m_btnRecentFilesOpts->onClick = [this]() { popupRecentOpts(); };
    m_btnSwitchEditor->onClick = [this]() { switchEditor(m_btnSwitchEditor->getToggleState()); };
    m_btnEditCode->onClick = [this]() { openCodeEditor(); };
    m_btnLoadPreset->onClick = [this]() { popupPresets(); };
    m_btnPresetOpts->onClick = [this]() { popupPresetOptions(); };
    m_btnReload->onClick = [this] {
        YsfxInfo::Ptr info = m_info;
        ysfx_t *fx = info->effect.get();
        juce::File file{juce::CharPointer_UTF8{ysfx_get_file_path(fx)}};
        quickAlertBox(
            ysfx_is_compiled(fx),
            [this, file]() {
                resetScaling(file);
                loadFile(file, false);
            },
            this->m_self
        );
    };
    m_btnUndo->onClick = [this] {
        this->m_proc->popUndoState();
    };
    m_btnRedo->onClick = [this] {
        this->m_proc->redoState();
    };

    m_btnGfxScaling->onClick = [this] {
        if (m_graphicsView) {
            float newScaling = (m_graphicsView->getScaling() + 0.5f);
            setScale(newScaling);
            m_mustResizeToGfx = true;
            relayoutUILater();

            juce::String key = getJsfxName() + juce::String("_scaling_factor");
            {
                juce::ScopedLock lock{m_pluginProperties->getLock()};
                m_pluginProperties->setValue(key, juce::String::formatted("%.3f", newScaling));
                m_pluginProperties->save();
            }
        }
    };

    m_ideView->onFileSaved = [this](const juce::File &file) { loadFile(file, true); };

    m_infoTimer.reset(FunctionalTimer::create([this]() { grabInfoAndUpdate(); }));
    m_infoTimer->startTimer(100);
}

void YsfxEditor::Impl::relayoutUI()
{
    YsfxInfo *info = m_info.get();
    ysfx_t *fx = info->effect.get();

    uint32_t gfxDim[2] = {};
    ysfx_get_gfx_dim(fx, gfxDim);

    int parameterHeight = m_miniParametersPanel->getRecommendedHeight(0);
    int sideTrim{0};
    int bottomTrim{0};

    if (m_mustResizeToGfx) {
        float scaling_factor = 1.0f;
        if (m_graphicsView) {
            scaling_factor = m_graphicsView->getTotalScaling();
        }

        int w = juce::jmax(defaultEditorWidth, (int)(gfxDim[0] * scaling_factor) + 2 * sideTrim);
        int h = juce::jmax(defaultEditorHeight, (int)(gfxDim[1] * scaling_factor) + m_self->m_headerSize + 2 * bottomTrim);

        m_divider->resetDragged();
        m_self->setSize(w, h + parameterHeight);
        m_mustResizeToGfx = false;
    }

    juce::Rectangle<int> temp;
    const juce::Rectangle<int> bounds = m_self->getLocalBounds();

    temp = bounds;
    const juce::Rectangle<int> topRow = temp.removeFromTop(m_self->m_headerSize);
    const juce::Rectangle<int> centerArea = temp.withTrimmedLeft(sideTrim).withTrimmedRight(sideTrim).withTrimmedBottom(bottomTrim);

    int width = 70;
    int spacing = 8;

    temp = topRow.reduced(10, 10);
    m_btnSwitchEditor->setBounds(temp.removeFromRight(80));
    temp.removeFromRight(spacing);
    m_btnPresetOpts->setBounds(temp.removeFromRight(25));
    temp.removeFromRight(0);
    m_btnLoadPreset->setBounds(temp.removeFromRight(width));
    temp.removeFromRight(spacing);
    m_btnEditCode->setBounds(temp.removeFromRight(60));
    temp.removeFromRight(spacing);
    m_btnGfxScaling->setBounds(temp.removeFromRight(40));
    temp.removeFromRight(spacing);

    int defaultLeftButtonWidth = 20 + 10 + 3 * (width + spacing);
    auto labelText = m_lblFilePath->getText();
    auto lines = juce::StringArray::fromTokens(labelText, "\n", "");

    if (m_visibleSlidersChanged) {
        juce::Array<YsfxParameter *> params2;
        params2.ensureStorageAllocated(ysfx_max_sliders);
        for (auto group = 0; group < ysfx_max_slider_groups; ++group) {
            auto group_offset = group << 6;
            for (auto idx = 0; idx < 64; idx++) {
                if (m_sliderVisible[group] & ((uint64_t)1 << idx)) {
                    params2.add(m_proc->getYsfxParameter(group_offset + idx));
                }
            }
        }
        m_visibleSlidersChanged = false;
        m_miniParametersPanel->setParametersDisplayed(params2);

        // Resize the divider if the number of parameters changed.
        int new_parameterHeight = m_miniParametersPanel->getRecommendedHeight(0);
        if (new_parameterHeight > parameterHeight) {
            if (m_divider->m_position == parameterHeight) {
                m_divider->m_position = new_parameterHeight;
                parameterHeight = new_parameterHeight;
            }
        }
    }

    int max_text_width = 0;
    for (auto line : lines) {
        int current_text_width = static_cast<int>(m_lblFilePath->getFont().getStringWidthFloat(line));
        if (current_text_width > max_text_width) max_text_width = current_text_width;
    }

    int roomNeeded = max_text_width + defaultLeftButtonWidth;
    int ioWidth = std::min(80, temp.getWidth() - roomNeeded);
    if (ioWidth > 0) {
        m_lblIO->setBounds(temp.removeFromRight(ioWidth));
        temp.removeFromRight(spacing);
        m_lblIO->setVisible(true);
    } else {
        m_lblIO->setVisible(false);
    }

    m_btnLoadFile->setBounds(temp.removeFromLeft(width));
    temp.removeFromLeft(spacing);
    m_btnRecentFiles->setBounds(temp.removeFromLeft(width));
    m_btnRecentFilesOpts->setBounds(temp.removeFromLeft(25));
    temp.removeFromLeft(spacing);

    int buttonWidth = width + spacing + std::min(0, ioWidth);
    if (buttonWidth > 0) {
        m_btnReload->setBounds(temp.removeFromLeft(buttonWidth));
        temp.removeFromLeft(spacing);
        m_btnReload->setVisible(true);

        if (m_showUndo || m_showRedo) {
            m_btnUndo->setBounds(temp.removeFromLeft(25));
            temp.removeFromLeft(spacing);
            m_btnUndo->setVisible(true);
            m_btnUndo->setEnabled(m_showUndo);

            m_btnRedo->setBounds(temp.removeFromLeft(25));
            temp.removeFromLeft(spacing);
            m_btnRedo->setVisible(true);
            m_btnRedo->setEnabled(m_showRedo);
        } else {
            m_btnUndo->setVisible(false);
            m_btnRedo->setVisible(false);
        }
    } else {
        m_btnReload->setVisible(false);
    }

    temp.expand(0, 10);
    m_lblFilePath->setBounds(temp);

    int nonParameterSpace = (int) (m_self->m_headerSize + 2 * bottomTrim + gfxDim[1] * m_graphicsView->getTotalScaling());

    juce::Component *viewed;
    if (m_btnSwitchEditor->getToggleState() && (fx && ysfx_has_section(fx, ysfx_section_gfx))) {
        int maxParamArea = m_self->getHeight();
        maxParamArea -= nonParameterSpace;
        auto recommended = m_miniParametersPanel->getRecommendedHeight(0);
        m_divider->setSizes(std::min(parameterHeight, std::max(200, maxParamArea)), recommended > 0 ? 5 : 0, recommended);

        const juce::Rectangle<int> paramArea = centerArea.withHeight(m_divider->m_position);
        const juce::Rectangle<int> gfxArea = centerArea.withTrimmedTop(m_divider->m_position);

        if (parameterHeight) {
            viewed = m_miniParametersPanel.get();
            viewed->setSize(paramArea.getWidth(), m_miniParametersPanel->getRecommendedHeight(0));
            
            m_topViewPort->setBounds(paramArea);
            m_divider->setBounds(m_topViewPort->getX(), m_topViewPort->getHeight() - 4, m_topViewPort->getWidth(), 4);

            m_topViewPort->setViewedComponent(viewed, false);
            m_topViewPort->setVisible(true);
            m_divider->setVisible(true);
            m_divider->toFront(false);
        } else {
            m_topViewPort->setViewedComponent(nullptr, false);
            m_topViewPort->setVisible(false);
        }

        viewed = m_graphicsView.get();
        viewed->setSize(gfxArea.getWidth(), gfxArea.getHeight());
        m_centerViewPort->setViewedComponent(viewed, false);
        m_centerViewPort->setBounds(gfxArea);
    } else {
        m_divider->setVisible(false);
        m_topViewPort->setViewedComponent(nullptr, false);
        m_topViewPort->setVisible(false);
        // Choose whether we only want the "visible" sliders or all of them
        viewed = m_btnSwitchEditor->getToggleState() ? m_miniParametersPanel.get() : m_parametersPanel.get();
        viewed->setSize(centerArea.getWidth(), m_parametersPanel->getRecommendedHeight(centerArea.getHeight()));
        m_centerViewPort->setViewedComponent(viewed, false);
        m_centerViewPort->setBounds(centerArea);
    }

    if (m_relayoutTimer)
        m_relayoutTimer->stopTimer();
}

void YsfxEditor::Impl::relayoutUILater()
{
    if (!m_relayoutTimer)
        m_relayoutTimer.reset(FunctionalTimer::create([this]() { relayoutUI(); }));
    m_relayoutTimer->startTimer(1);
}

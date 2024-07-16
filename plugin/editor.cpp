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
#include "utility/functional_timer.h"
#include "ysfx.h"
#include <juce_gui_extra/juce_gui_extra.h>

struct YsfxEditor::Impl {
    YsfxEditor *m_self = nullptr;
    YsfxProcessor *m_proc = nullptr;
    YsfxInfo::Ptr m_info;
    std::unique_ptr<juce::Timer> m_infoTimer;
    std::unique_ptr<juce::Timer> m_relayoutTimer;
    std::unique_ptr<juce::FileChooser> m_fileChooser;
    std::unique_ptr<juce::PopupMenu> m_recentFilesPopup;
    std::unique_ptr<juce::PopupMenu> m_presetsPopup;
    std::unique_ptr<juce::PropertiesFile> m_pluginProperties;
    bool m_fileChooserActive = false;
    bool m_mustResizeToGfx = true;

    //==========================================================================
    void updateInfo();
    void grabInfoAndUpdate();
    void chooseFileAndLoad();
    void loadFile(const juce::File &file);
    void popupRecentFiles();
    void popupPresets();
    void switchEditor(bool showGfx);
    void openCodeEditor();
    static juce::File getAppDataDirectory();
    static juce::File getDefaultEffectsDirectory();
    juce::RecentlyOpenedFilesList loadRecentFiles();
    void saveRecentFiles(const juce::RecentlyOpenedFilesList &recent);
    void clearRecentFiles();
    void setScale(float newScaling);
    void loadScaling();
    void saveScaling();
    void resetScaling(juce::File &jsfxFilePath);
    juce::String getJsfxName();

    //==========================================================================
    class CodeWindow : public juce::DocumentWindow {
    public:
        using juce::DocumentWindow::DocumentWindow;

    protected:
        void closeButtonPressed() override { setVisible(false); }
    };

    //==========================================================================
    std::unique_ptr<juce::TextButton> m_btnLoadFile;
    std::unique_ptr<juce::TextButton> m_btnRecentFiles;
    std::unique_ptr<juce::TextButton> m_btnEditCode;
    std::unique_ptr<juce::TextButton> m_btnLoadPreset;
    std::unique_ptr<juce::TextButton> m_btnSwitchEditor;
    std::unique_ptr<juce::TextButton> m_btnReload;
    std::unique_ptr<juce::TextButton> m_btnGfxScaling;

    std::unique_ptr<juce::Label> m_lblFilePath;
    std::unique_ptr<juce::Label> m_lblIO;
    std::unique_ptr<juce::Viewport> m_centerViewPort;
    std::unique_ptr<YsfxParametersPanel> m_parametersPanel;
    std::unique_ptr<YsfxGraphicsView> m_graphicsView;
    std::unique_ptr<YsfxIDEView> m_ideView;
    std::unique_ptr<CodeWindow> m_codeWindow;
    std::unique_ptr<juce::TooltipWindow> m_tooltipWindow;

    //==========================================================================
    void createUI();
    void connectUI();
    void relayoutUI();
    void relayoutUILater();
    void initializeProperties();
};

static const int defaultEditorWidth = 700;
static const int defaultEditorHeight = 600;

YsfxEditor::YsfxEditor(YsfxProcessor &proc)
    : juce::AudioProcessorEditor(proc),
      m_impl(new Impl)
{
    m_impl->m_self = this;
    m_impl->m_proc = &proc;
    m_impl->m_info = proc.getCurrentInfo();

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
}

void YsfxEditor::paint (juce::Graphics& g)
{
    // Redraw only parts that aren't covered already.
    const juce::Rectangle<int> bounds = getLocalBounds();
    g.setColour(juce::Colours::black);
    g.setOpacity(1.0f);
    g.fillRect(juce::Rectangle<int>(0, 0, bounds.getWidth(), 70));
    g.fillRect(juce::Rectangle<int>(0, 0, 20, bounds.getHeight()));
    g.fillRect(juce::Rectangle<int>(bounds.getWidth() - 20, 0, 20, bounds.getHeight()));
    g.fillRect(juce::Rectangle<int>(0, bounds.getHeight() - 20, bounds.getWidth(), 20));
}

YsfxEditor::~YsfxEditor()
{
    if (m_impl) {
        m_impl->saveScaling();
    }
}

void YsfxEditor::resized()
{
    m_impl->relayoutUILater();
}

juce::String YsfxEditor::Impl::getJsfxName()
{
    YsfxInfo::Ptr info = m_info;
    ysfx_t *fx = info->effect.get();
    if (!fx) return juce::String("");

    juce::File file{juce::CharPointer_UTF8{ysfx_get_file_path(fx)}};
    return file.getFileNameWithoutExtension();
}

void YsfxEditor::Impl::grabInfoAndUpdate()
{
    YsfxInfo::Ptr info = m_proc->getCurrentInfo();
    if (m_info != info) {
        m_info = info;
        updateInfo();
    }
}

void YsfxEditor::Impl::updateInfo()
{
    YsfxInfo *info = m_info.get();
    ysfx_t *fx = info->effect.get();

    juce::File filePath{juce::CharPointer_UTF8{ysfx_get_file_path(fx)}};

    if (filePath != juce::File{}) {
        m_lblFilePath->setText(filePath.getFileNameWithoutExtension(), juce::dontSendNotification);
        m_lblFilePath->setTooltip(filePath.getFullPathName());
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

    m_graphicsView->setEffect(fx);
    m_ideView->setEffect(fx, info->timeStamp);

    if (!info->errors.isEmpty())
        m_ideView->setStatusText(info->errors.getReference(0));
    else if (!info->warnings.isEmpty())
        m_ideView->setStatusText(info->warnings.getReference(0));
    else
        m_ideView->setStatusText(TRANS("Compiled OK"));

    bool hasGfx = ysfx_has_section(fx, ysfx_section_gfx);
    switchEditor(hasGfx);

    juce::File file{juce::CharPointer_UTF8{ysfx_get_file_path(fx)}};
    m_mustResizeToGfx = true;
    loadScaling();
    
    relayoutUILater();
}

void YsfxEditor::Impl::chooseFileAndLoad()
{
    if (m_fileChooserActive)
        return;

    YsfxInfo *info = m_info.get();
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

    m_fileChooser.reset(new juce::FileChooser(TRANS("Open jsfx..."), initialPath));
    m_fileChooserActive = true;

    m_fileChooser->launchAsync(
        juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser &chooser) {
            juce::File result = chooser.getResult();
            if (result != juce::File()) {
                saveScaling();
                loadFile(result);
            }
            m_fileChooserActive = false;
        });
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
                m_pluginProperties->needsToBeSaved();
            }
        }
    }
}

void YsfxEditor::Impl::resetScaling(juce::File &jsfxFilePath)
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
        }
    }
}

void YsfxEditor::Impl::loadFile(const juce::File &file)
{
    {
        juce::ScopedLock lock{m_pluginProperties->getLock()};
        m_pluginProperties->setValue("load_path", file.getParentDirectory().getFullPathName());
        m_pluginProperties->save();
    }

    m_proc->loadJsfxFile(file.getFullPathName(), nullptr, true);
    relayoutUILater();

    juce::RecentlyOpenedFilesList recent = loadRecentFiles();
    recent.addFile(file);
    saveRecentFiles(recent);
}

void YsfxEditor::Impl::popupRecentFiles()
{
    m_recentFilesPopup.reset(new juce::PopupMenu);

    m_recentFilesPopup->addItem(1000, TRANS("Clear items"));
    m_recentFilesPopup->addSeparator();

    juce::RecentlyOpenedFilesList recent = loadRecentFiles();
    recent.createPopupMenuItems(*m_recentFilesPopup, 100, false, true);

    if (m_recentFilesPopup->getNumItems() == 0)
        return;

    juce::PopupMenu::Options popupOptions = juce::PopupMenu::Options{}
        .withParentComponent(m_self)
        .withTargetComponent(*m_btnRecentFiles);
    m_recentFilesPopup->showMenuAsync(popupOptions, [this, recent](int index) {
        if (index == 1000)
            clearRecentFiles();
        else if (index != 0) {
            saveScaling();
            loadFile(recent.getFile(index - 100));
        }
    });
}

void YsfxEditor::Impl::popupPresets()
{
    m_presetsPopup.reset(new juce::PopupMenu);

    YsfxInfo::Ptr info = m_info;
    ysfx_bank_t *bank = info->bank.get();
    if (!bank)
        m_presetsPopup->addItem(0, TRANS("No presets"), false);
    else {
        for (uint32_t i = 0; i < bank->preset_count; ++i)
            m_presetsPopup->addItem((int)(i + 1), bank->presets[i].name);
    }

    juce::PopupMenu::Options popupOptions = juce::PopupMenu::Options{}
        .withParentComponent(m_self)
        .withTargetComponent(*m_btnLoadPreset);
    m_presetsPopup->showMenuAsync(popupOptions, [this, info](int index) {
        if (index > 0)
            m_proc->loadJsfxPreset(info, (uint32_t)(index - 1), true);
    });
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
    m_codeWindow->setVisible(true);
    m_codeWindow->toFront(true);
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

    return dir.getChildFile("ysfx");
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

    options.applicationName = JucePlugin_Name;
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
    m_btnEditCode.reset(new juce::TextButton(TRANS("Edit")));
    m_self->addAndMakeVisible(*m_btnEditCode);
    m_btnGfxScaling.reset(new juce::TextButton(TRANS("x1")));
    m_self->addAndMakeVisible(*m_btnGfxScaling);
    m_btnGfxScaling->setTooltip("Render JSFX UI at lower resolution and upscale the result. Ths is intended for JSFX that do not implement scaling themselves. For JSFX that do, it is better to simply resize the plugin.");
    m_btnLoadPreset.reset(new juce::TextButton(TRANS("Preset")));
    m_self->addAndMakeVisible(*m_btnLoadPreset);
    m_btnSwitchEditor.reset(new juce::TextButton(TRANS("Sliders")));
    m_btnSwitchEditor->setClickingTogglesState(true);
    m_self->addAndMakeVisible(*m_btnSwitchEditor);
    m_lblFilePath.reset(new juce::Label);
    m_lblFilePath->setMinimumHorizontalScale(1.0f);
    m_lblFilePath->setJustificationType(juce::Justification::horizontallyCentred);
    m_self->addAndMakeVisible(*m_lblFilePath);
    m_lblIO.reset(new juce::Label);
    m_lblIO->setMinimumHorizontalScale(1.0f);
    m_lblIO->setJustificationType(juce::Justification::horizontallyCentred);
    m_lblIO->setColour(juce::Label::outlineColourId, m_self->findColour(juce::ComboBox::outlineColourId));
    m_self->addAndMakeVisible(*m_lblIO);
    m_centerViewPort.reset(new juce::Viewport);
    m_centerViewPort->setScrollBarsShown(true, false);
    m_self->addAndMakeVisible(*m_centerViewPort);
    m_parametersPanel.reset(new YsfxParametersPanel);
    m_graphicsView.reset(new YsfxGraphicsView);
    m_ideView.reset(new YsfxIDEView);
    m_ideView->setVisible(true);
    m_ideView->setSize(1000, 600);
    m_codeWindow.reset(new CodeWindow(TRANS("Edit"), m_self->findColour(juce::DocumentWindow::backgroundColourId), juce::DocumentWindow::allButtons));
    m_codeWindow->setResizable(true, false);
    m_codeWindow->setContentNonOwned(m_ideView.get(), true);
    m_tooltipWindow.reset(new juce::TooltipWindow);
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
    m_btnSwitchEditor->onClick = [this]() { switchEditor(m_btnSwitchEditor->getToggleState()); };
    m_btnEditCode->onClick = [this]() { openCodeEditor(); };
    m_btnLoadPreset->onClick = [this]() { popupPresets(); };
    m_btnReload->onClick = [this] {
        YsfxInfo::Ptr info = m_info;
        ysfx_t *fx = info->effect.get();
        if (!fx) return;

        juce::File file{juce::CharPointer_UTF8{ysfx_get_file_path(fx)}};
        resetScaling(file);
        loadFile(file);
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

    m_ideView->onFileSaved = [this](const juce::File &file) { loadFile(file); };
    m_ideView->onReloadRequested = [this](const juce::File &file) { loadFile(file); };

    m_infoTimer.reset(FunctionalTimer::create([this]() { grabInfoAndUpdate(); }));
    m_infoTimer->startTimer(100);
}

void YsfxEditor::Impl::relayoutUI()
{
    YsfxInfo *info = m_info.get();
    ysfx_t *fx = info->effect.get();

    uint32_t gfxDim[2] = {};
    ysfx_get_gfx_dim(fx, gfxDim);

    if (m_mustResizeToGfx) {
        float scaling_factor = 1.0f;
        if (m_graphicsView) {
            scaling_factor = m_graphicsView->getTotalScaling();
        }

        int w = juce::jmax(defaultEditorWidth, (int)(gfxDim[0] * scaling_factor) + 22);
        int h = juce::jmax(defaultEditorHeight, (int)(gfxDim[1] * scaling_factor) + 50 + 20);

        m_self->setSize(w, h);
        m_mustResizeToGfx = false;
    }

    juce::Rectangle<int> temp;
    const juce::Rectangle<int> bounds = m_self->getLocalBounds();

    temp = bounds;
    const juce::Rectangle<int> topRow = temp.removeFromTop(50);
    const juce::Rectangle<int> centerArea = temp.withTrimmedLeft(10).withTrimmedRight(10).withTrimmedBottom(10);

    int width = 70;
    int spacing = 8;
    temp = topRow.reduced(10, 10);
    m_btnLoadFile->setBounds(temp.removeFromLeft(width));
    temp.removeFromLeft(spacing);
    m_btnRecentFiles->setBounds(temp.removeFromLeft(width));
    temp.removeFromLeft(spacing);
    m_btnReload->setBounds(temp.removeFromLeft(width));
    temp.removeFromLeft(spacing);

    m_btnSwitchEditor->setBounds(temp.removeFromRight(80));
    temp.removeFromRight(spacing);
    m_btnLoadPreset->setBounds(temp.removeFromRight(width));
    temp.removeFromRight(spacing);
    m_btnEditCode->setBounds(temp.removeFromRight(60));
    temp.removeFromRight(spacing);
    m_btnGfxScaling->setBounds(temp.removeFromRight(40));
    temp.removeFromRight(spacing);

    m_lblIO->setBounds(temp.removeFromRight(80));
    temp.removeFromRight(spacing);
    m_lblFilePath->setBounds(temp);

    m_centerViewPort->setBounds(centerArea);

    juce::Component *viewed;
    if (m_btnSwitchEditor->getToggleState()) {
        viewed = m_graphicsView.get();
        viewed->setSize(centerArea.getWidth(), centerArea.getHeight());
    }
    else {
        viewed = m_parametersPanel.get();
        viewed->setSize(centerArea.getWidth(), m_parametersPanel->getRecommendedHeight(m_centerViewPort->getHeight()));
    }

    m_centerViewPort->setViewedComponent(viewed, false);

    if (m_relayoutTimer)
        m_relayoutTimer->stopTimer();
}

void YsfxEditor::Impl::relayoutUILater()
{
    if (!m_relayoutTimer)
        m_relayoutTimer.reset(FunctionalTimer::create([this]() { relayoutUI(); }));
    m_relayoutTimer->startTimer(1);
}

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

#include "ysfx_document.h"
#include "modal_textinputbox.h"


YSFXCodeDocument::YSFXCodeDocument() : CodeDocument()
{
    reset();
};

void YSFXCodeDocument::reset()
{
    replaceAllContent(juce::String{});
}

void YSFXCodeDocument::loadFile(juce::File file)
{
    bool clearUndo = false;
    if (file != juce::File{}) {
        if (m_file != file) {
            clearUndo = true;
            m_file = file;
        }
    }
    if (!m_file.existsAsFile()) return;

    {
        m_changeTime = m_file.getLastModificationTime();
        juce::MemoryBlock memBlock;
        if (m_file.loadFileAsData(memBlock)) {
            juce::String newContent = memBlock.toString();
            memBlock = {};
            if (newContent != getAllContent()) {
                replaceAllContent(newContent);
                if (clearUndo) {
                    clearUndoHistory();
                    setSavePoint();
                }
            }
        }
    }
}

bool YSFXCodeDocument::loaded(void) {
    return bool(m_file != juce::File{});
}

juce::File YSFXCodeDocument::getPath(void) {
    return m_file;
}

juce::String YSFXCodeDocument::getName(void) {
    if (!m_file.existsAsFile()) return juce::String("Untitled");

    return m_file.getFileName();
}

void YSFXCodeDocument::checkFileForModifications()
{
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
                    loadFile(m_file);
                }
            };

            m_alertWindow.reset(show_option_window(TRANS("Reload?"), TRANS("The file ") + m_file.getFileNameWithoutExtension() + TRANS(" has been modified outside this editor. Reload it?"), std::vector<juce::String>{"Yes", "No"}, callback));
        }
    }
}

bool YSFXCodeDocument::saveFile(juce::File path)
{
    const juce::String content = getAllContent();

    if (path == juce::File{}) {
        return saveFile(m_file);
    }

    bool success = path.replaceWithData(content.toRawUTF8(), content.getNumBytesAsUTF8());
    if (!success) {
        m_alertWindow.reset(show_option_window(TRANS("Error"), TRANS("Could not save ") + path.getFileNameWithoutExtension() + TRANS("."), std::vector<juce::String>{"OK"}, [](int result){ (void) result; }));
        return false;
    }
    m_file = path;

    m_changeTime = juce::Time::getCurrentTime();
    return true;
}

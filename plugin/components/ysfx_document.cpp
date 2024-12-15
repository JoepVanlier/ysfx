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
    if (file != juce::File{}) m_file = file;
    if (!m_file.existsAsFile()) return;

    {
        m_changeTime = m_file.getLastModificationTime();
        juce::MemoryBlock memBlock;
        if (m_file.loadFileAsData(memBlock)) {
            juce::String newContent = memBlock.toString();
            memBlock = {};
            if (newContent != getAllContent()) {
                replaceAllContent(newContent);
                // m_editor->moveCaretToTop(false);
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
        saveFile(m_file);
    }

    bool success = m_file.replaceWithData(content.toRawUTF8(), content.getNumBytesAsUTF8());
    if (!success) {
        m_alertWindow.reset(show_option_window(TRANS("Error"), TRANS("Could not save ") + m_file.getFileNameWithoutExtension() + TRANS("."), std::vector<juce::String>{"OK"}, [](int result){ (void) result; }));
        return false;
    }

    m_changeTime = juce::Time::getCurrentTime();
    return true;
}

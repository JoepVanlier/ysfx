#include "tokenizer.h"
#include "tokenizer_functions.h"


JSFXTokenizer::JSFXTokenizer() {

}

void JSFXTokenizer::setColours(std::map<std::string, std::array<uint8_t, 3>> colormap)
{
    std::vector<std::string> ideColors{
        "error", "comment", "builtin_variable", "builtin_function", "builtin_core_function",
        "builtin_section", "operator", "identifier", "integer", "float", "string", "bracket", 
        "punctuation", "preprocessor_text", "string_hash", "not_supported"
    };

    for (auto const& key : ideColors)
        m_colourScheme.set(key, juce::Colour(colormap[key][0], colormap[key][1], colormap[key][2]));
}

juce::CodeEditorComponent::ColourScheme JSFXTokenizer::getDefaultColourScheme()
{
    return m_colourScheme;
}

int JSFXTokenizer::readNextToken (juce::CodeDocument::Iterator& source)
{
    return JSFXTokenizerFunctions::readNextJSFXToken(source);
}

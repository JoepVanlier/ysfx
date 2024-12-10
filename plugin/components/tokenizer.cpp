#include "tokenizer.h"
#include "tokenizer_functions.h"


JSFXTokenizer::JSFXTokenizer() {

}

juce::CodeEditorComponent::ColourScheme JSFXTokenizer::getDefaultColourScheme()
{
    struct Type
    {
        const char* name;
        juce::uint32 colour;
    };

    const Type types[] =
    {
        { "error",                 0xffffcc00 },
        { "comment",               0xff6080c0 },
        { "builtin_variable",      0xffff8080 },
        { "builtin_function",      0xffffff30 },
        { "builtin_core_function", 0xff00c0ff },
        { "builtin_section",       0xff00ffff },
        { "operator",              0xff00ffff },
        { "identifier",            0xffc0c0c0 },
        { "integer",               0xff00ff00 },
        { "float",                 0xff00ff00 },
        { "string",                0xffffc0c0 },
        { "bracket",               0xffc0c0ff },
        { "punctuation",           0xff00ffff },
        { "preprocessor_text",     0xff20c0ff },
        { "string_hash",           0xffc0ff80 }
    };

    juce::CodeEditorComponent::ColourScheme cs;

    for (auto& t : types)
        cs.set(t.name, juce::Colour(t.colour));

    return cs;
}

int JSFXTokenizer::readNextToken (juce::CodeDocument::Iterator& source)
{
    return JSFXTokenizerFunctions::readNextJSFXToken(source);
}

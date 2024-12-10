#include <juce_gui_extra/juce_gui_extra.h>

class JSFXTokenizer : public juce::CPlusPlusCodeTokeniser
{
    public:
        JSFXTokenizer();
        juce::CodeEditorComponent::ColourScheme getDefaultColourScheme();

        /** The token values returned by this tokeniser. */
        enum TokenType
        {
            tokenType_error = 0,
            tokenType_comment,
            tokenType_builtin_variable,
            tokenType_builtin_function,
            tokenType_builtin_core_function,
            tokenType_builtin_section,
            tokenType_operator,
            tokenType_identifier,
            tokenType_integer,
            tokenType_float,
            tokenType_string,
            tokenType_bracket,
            tokenType_punctuation,
            tokenType_preprocessor,
            tokenType_string_hash,
        };

    private:
        int readNextToken(juce::CodeDocument::Iterator& source);
};

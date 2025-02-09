#include <juce_gui_extra/juce_gui_extra.h>


namespace JSFXTokenizerFunctions {
    static bool isIdentifierStart(const juce::juce_wchar c) noexcept
    {
        return juce::CharacterFunctions::isLetter(c) || c == '_' || c == '@';
    }

    static bool isSectionLike(juce::String::CharPointerType token, const int tokenLength) noexcept
    {
        static const char* const keywords2Char[] = { nullptr };
        static const char* const keywords3Char[] = { nullptr };
        static const char* const keywords4Char[] = { "@gfx", "desc", "tags", nullptr };
        static const char* const keywords5Char[] = { "@init", nullptr };
        static const char* const keywords6Char[] = { "@block", "import", "in_pin", nullptr };
        static const char* const keywords7Char[] = { "@sample", "@slider", "out_pin", "options", nullptr };
        static const char* const keywords8Char[] = { nullptr };
        static const char* const keywords9Char[] = { nullptr };
        static const char* const keywords10Char[] = { "@serialize", nullptr };
        static const char* const keywordsOther[] = { nullptr };

        const char* const* k;

        switch (tokenLength)
        {
            case 2:     k = keywords2Char; break;
            case 3:     k = keywords3Char; break;
            case 4:     k = keywords4Char; break;
            case 5:     k = keywords5Char; break;
            case 6:     k = keywords6Char; break;
            case 7:     k = keywords7Char; break;
            case 8:     k = keywords8Char; break;
            case 9:     k = keywords9Char; break;
            case 10:     k = keywords10Char; break;

            default:
                if (tokenLength < 2 || tokenLength > 16)
                    return false;

                k = keywordsOther;
                break;
        }

        for (int i = 0; k[i] != nullptr; ++i)
            if (token.compare(juce::CharPointer_ASCII(k[i])) == 0)
                return true;

        return false;
    }

    static bool isCoreFuncLike(juce::String::CharPointerType token, const int tokenLength) noexcept
    {
        static const char* const keywords2Char[] = { nullptr };
        static const char* const keywords3Char[] = { nullptr };
        static const char* const keywords4Char[] = { "loop", "this", nullptr };
        static const char* const keywords5Char[] = { "local", nullptr };
        static const char* const keywords6Char[] = { "global", nullptr };
        static const char* const keywords7Char[] = { "_global", nullptr };
        static const char* const keywords8Char[] = { "function", "instance", nullptr };
        static const char* const keywordsOther[] = { nullptr };

        const char* const* k;

        switch (tokenLength)
        {
            case 2:     k = keywords2Char; break;
            case 3:     k = keywords3Char; break;
            case 4:     k = keywords4Char; break;
            case 5:     k = keywords5Char; break;
            case 6:     k = keywords6Char; break;
            case 7:     k = keywords7Char; break;
            case 8:     k = keywords8Char; break;

            default:
                if (tokenLength < 2 || tokenLength > 16)
                    return false;

                k = keywordsOther;
                break;
        }

        for (int i = 0; k[i] != nullptr; ++i)
            if (token.compare(juce::CharPointer_ASCII(k[i])) == 0)
                return true;

        return false;
    }

    static bool isBuiltinFunction(juce::String::CharPointerType token, const int tokenLength) noexcept
    {
        static const char* const keywords2Char[] = { nullptr };
        static const char* const keywords3Char[] = { "abs", "cos", "exp", "fft", "log", "max", "min", "pow", "sin", "spl", "sqr", "tan", nullptr };
        static const char* const keywords4Char[] = { "acos", "asin", "atan", "ceil", "ifft", "mdct", "rand", "sign", "sqrt", nullptr };
        static const char* const keywords5Char[] = { "atan2", "floor", "log10", "match", nullptr };
        static const char* const keywords6Char[] = { "matchi", "memcpy", "memset", "slider", "strcat", "strcmp", "strcpy", "strlen", nullptr };
        static const char* const keywords7Char[] = { "_memtop", "gfx_arc", "gfx_set", "invsqrt", "midisyx", "sprintf", "stricmp", "strncat", "strncmp", "strncpy", nullptr };
        static const char* const keywords8Char[] = { "fft_real", "file_mem", "file_var", "freembuf", "gfx_blit", "gfx_line", "gfx_rect", "midirecv", "midisend", "strnicmp", nullptr };
        static const char* const keywords9Char[] = { "file_open", "file_riff", "file_text", "ifft_real", "stack_pop", nullptr };
        static const char* const keywords10Char[] = { "atomic_add", "atomic_get", "atomic_set", "convolve_c", "file_avail", "file_close", "gfx_blurto", "gfx_circle", "gfx_lineto", "gfx_printf", "gfx_rectto", "stack_exch", "stack_peek", "stack_push", nullptr };
        static const char* const keywordsOther[] = { "atomic_exch", "atomic_setifequal", "fft_permute", "file_rewind", "file_string", "gfx_blitext", "gfx_deltablit", "gfx_drawchar", "gfx_drawnumber", "gfx_drawstr", "gfx_getchar", "gfx_getfont", "gfx_getimgdim", "gfx_getpixel", "gfx_gradrect", "gfx_loadimg", "gfx_measurestr", "gfx_muladdrect", "gfx_roundrect", "gfx_setcursor", "gfx_setfont", "gfx_setimgdim", "gfx_setpixel", "gfx_showmenu", "gfx_transformblit", "gfx_triangle", "ifft_permute", "mem_get_values", "mem_insert_shuffle", "mem_multiply_sum", "mem_set_values", "midirecv_buf", "midirecv_str", "midisend_buf", "midisend_str", "slider_automate", "slider_next_chg", "slider_show", "sliderchange", "str_getchar", "str_setchar", "strcpy_from", "strcpy_fromslider", "strcpy_substr", nullptr };
        const char* const* k;

        switch (tokenLength)
        {
            case 2:     k = keywords2Char; break;
            case 3:     k = keywords3Char; break;
            case 4:     k = keywords4Char; break;
            case 5:     k = keywords5Char; break;
            case 6:     k = keywords6Char; break;
            case 7:     k = keywords7Char; break;
            case 8:     k = keywords8Char; break;
            case 9:     k = keywords9Char; break;
            case 10:     k = keywords10Char; break;

            default:
                if (tokenLength < 2 || tokenLength > 16)
                    return false;

                k = keywordsOther;
                break;
        }

        for (int i = 0; k[i] != nullptr; ++i)
            if (token.compare(juce::CharPointer_ASCII(k[i])) == 0)
                return true;

        return false;
    }

    static bool isBuiltinVar(juce::String::CharPointerType token, const int tokenLength) noexcept
    {
        static const char* const keywords2Char[] = { nullptr };
        static const char* const keywords3Char[] = { nullptr };
        static const char* const keywords4Char[] = { "reg0", "reg1", "reg2", "reg3", "reg4", "reg5", "reg6", "reg7", "reg8", "reg9", "spl0", "spl1", "spl2", "spl3", "spl4", "spl5", "spl6", "spl7", "spl8", "spl9", nullptr };
        static const char* const keywords5Char[] = { "gfx_a", "gfx_b", "gfx_g", "gfx_h", "gfx_r", "gfx_w", "gfx_x", "gfx_y", "reg10", "reg11", "reg12", "reg13", "reg14", "reg15", "reg16", "reg17", "reg18", "reg19", "reg20", "reg21", "reg22", "reg23", "reg24", "reg25", "reg26", "reg27", "reg28", "reg29", "reg30", "reg31", "reg32", "reg33", "reg34", "reg35", "reg36", "reg37", "reg38", "reg39", "reg40", "reg41", "reg42", "reg43", "reg44", "reg45", "reg46", "reg47", "reg48", "reg49", "reg50", "reg51", "reg52", "reg53", "reg54", "reg55", "reg56", "reg57", "reg58", "reg59", "reg60", "reg61", "reg62", "reg63", "reg64", "reg65", "reg66", "reg67", "reg68", "reg69", "reg70", "reg71", "reg72", "reg73", "reg74", "reg75", "reg76", "reg77", "reg78", "reg79", "reg80", "reg81", "reg82", "reg83", "reg84", "reg85", "reg86", "reg87", "reg88", "reg89", "reg90", "reg91", "reg92", "reg93", "reg94", "reg95", "reg96", "reg97", "reg98", "reg99", "spl10", "spl11", "spl12", "spl13", "spl14", "spl15", "spl16", "spl17", "spl18", "spl19", "spl20", "spl21", "spl22", "spl23", "spl24", "spl25", "spl26", "spl27", "spl28", "spl29", "spl30", "spl31", "spl32", "spl33", "spl34", "spl35", "spl36", "spl37", "spl38", "spl39", "spl40", "spl41", "spl42", "spl43", "spl44", "spl45", "spl46", "spl47", "spl48", "spl49", "spl50", "spl51", "spl52", "spl53", "spl54", "spl55", "spl56", "spl57", "spl58", "spl59", "spl60", "spl61", "spl62", "spl63", "tempo", nullptr };
        static const char* const keywords6Char[] = { "ts_num", nullptr };
        static const char* const keywords7Char[] = { "mouse_x", "mouse_y", "slider0", "slider1", "slider2", "slider3", "slider4", "slider5", "slider6", "slider7", "slider8", "slider9", "trigger", nullptr };
        static const char* const keywords8Char[] = { "gfx_dest", "gfx_mode", "midi_bus", "pdc_midi", "slider10", "slider11", "slider12", "slider13", "slider14", "slider15", "slider16", "slider17", "slider18", "slider19", "slider20", "slider21", "slider22", "slider23", "slider24", "slider25", "slider26", "slider27", "slider28", "slider29", "slider30", "slider31", "slider32", "slider33", "slider34", "slider35", "slider36", "slider37", "slider38", "slider39", "slider40", "slider41", "slider42", "slider43", "slider44", "slider45", "slider46", "slider47", "slider48", "slider49", "slider50", "slider51", "slider52", "slider53", "slider54", "slider55", "slider56", "slider57", "slider58", "slider59", "slider60", "slider61", "slider62", "slider63", "slider64", "slider65", "slider66", "slider67", "slider68", "slider69", "slider70", "slider71", "slider72", "slider73", "slider74", "slider75", "slider76", "slider77", "slider78", "slider79", "slider80", "slider81", "slider82", "slider83", "slider84", "slider85", "slider86", "slider87", "slider88", "slider89", "slider90", "slider91", "slider92", "slider93", "slider94", "slider95", "slider96", "slider97", "slider98", "slider99", "ts_denom", nullptr };
        static const char* const keywords9Char[] = { "gfx_clear", "gfx_texth", "gfx_textw", "mouse_cap", "pdc_delay", "slider100", "slider101", "slider102", "slider103", "slider104", "slider105", "slider106", "slider107", "slider108", "slider109", "slider110", "slider111", "slider112", "slider113", "slider114", "slider115", "slider116", "slider117", "slider118", "slider119", "slider120", "slider121", "slider122", "slider123", "slider124", "slider125", "slider126", "slider127", "slider128", "slider129", "slider130", "slider131", "slider132", "slider133", "slider134", "slider135", "slider136", "slider137", "slider138", "slider139", "slider140", "slider141", "slider142", "slider143", "slider144", "slider145", "slider146", "slider147", "slider148", "slider149", "slider150", "slider151", "slider152", "slider153", "slider154", "slider155", "slider156", "slider157", "slider158", "slider159", "slider160", "slider161", "slider162", "slider163", "slider164", "slider165", "slider166", "slider167", "slider168", "slider169", "slider170", "slider171", "slider172", "slider173", "slider174", "slider175", "slider176", "slider177", "slider178", "slider179", "slider180", "slider181", "slider182", "slider183", "slider184", "slider185", "slider186", "slider187", "slider188", "slider189", "slider190", "slider191", "slider192", "slider193", "slider194", "slider195", "slider196", "slider197", "slider198", "slider199", "slider200", "slider201", "slider202", "slider203", "slider204", "slider205", "slider206", "slider207", "slider208", "slider209", "slider210", "slider211", "slider212", "slider213", "slider214", "slider215", "slider216", "slider217", "slider218", "slider219", "slider220", "slider221", "slider222", "slider223", "slider224", "slider225", "slider226", "slider227", "slider228", "slider229", "slider230", "slider231", "slider232", "slider233", "slider234", "slider235", "slider236", "slider237", "slider238", "slider239", "slider240", "slider241", "slider242", "slider243", "slider244", "slider245", "slider246", "slider247", "slider248", "slider249", "slider250", "slider251", "slider252", "slider253", "slider254", "slider255", nullptr };
        static const char* const keywords10Char[] = { "ext_noinit", "pdc_bot_ch", "pdc_top_ch", "play_state", nullptr };
        static const char* const keywordsOther[] = { "beat_position", "ext_midi_bus", "ext_nodenorm", "ext_tail_size", "gfx_ext_flags", "gfx_ext_retina", "mouse_hwheel", "mouse_wheel", "play_position", "samplesblock", "sratenum_ch", nullptr };
        const char* const* k;

        switch (tokenLength)
        {
            case 2:     k = keywords2Char; break;
            case 3:     k = keywords3Char; break;
            case 4:     k = keywords4Char; break;
            case 5:     k = keywords5Char; break;
            case 6:     k = keywords6Char; break;
            case 7:     k = keywords7Char; break;
            case 8:     k = keywords8Char; break;
            case 9:     k = keywords9Char; break;
            case 10:     k = keywords10Char; break;

            default:
                if (tokenLength < 2 || tokenLength > 16)
                    return false;

                k = keywordsOther;
                break;
        }

        for (int i = 0; k[i] != nullptr; ++i)
            if (token.compare(juce::CharPointer_ASCII(k[i])) == 0)
                return true;

        return false;
    }


    template <typename Iterator>
    static int parseIdentifier (Iterator& source) noexcept
    {
        int tokenLength = 0;
        juce::String::CharPointerType::CharType possibleIdentifier[100] = {};
        juce::String::CharPointerType possible (possibleIdentifier);

        while (juce::CppTokeniserFunctions::isIdentifierBody(source.peekNextChar()))
        {
            auto c = source.nextChar();

            if (tokenLength < 20)
                possible.write(c);

            ++tokenLength;
        }

        if (tokenLength > 1 && tokenLength <= 16)
        {
            possible.writeNull();

            if (JSFXTokenizerFunctions::isSectionLike(juce::String::CharPointerType(possibleIdentifier), tokenLength))
                return JSFXTokenizer::tokenType_builtin_section;

            if (JSFXTokenizerFunctions::isBuiltinVar(juce::String::CharPointerType(possibleIdentifier), tokenLength))
                return JSFXTokenizer::tokenType_builtin_variable;

            if (JSFXTokenizerFunctions::isCoreFuncLike(juce::String::CharPointerType(possibleIdentifier), tokenLength))
                return JSFXTokenizer::tokenType_builtin_core_function;

            if (JSFXTokenizerFunctions::isBuiltinFunction(juce::String::CharPointerType(possibleIdentifier), tokenLength))
                return JSFXTokenizer::tokenType_builtin_function;
        }

        return JSFXTokenizer::tokenType_identifier;
    }

    template <typename Iterator>
    static int parseNumber(Iterator& source)
    {
        const Iterator original (source);

        if (juce::CppTokeniserFunctions::parseFloatLiteral(source))    return JSFXTokenizer::tokenType_float;
        source = original;

        if (juce::CppTokeniserFunctions::parseHexLiteral(source))      return JSFXTokenizer::tokenType_integer;
        source = original;

        if (juce::CppTokeniserFunctions::parseOctalLiteral(source))    return JSFXTokenizer::tokenType_integer;
        source = original;

        if (juce::CppTokeniserFunctions::parseDecimalLiteral(source))  return JSFXTokenizer::tokenType_integer;
        source = original;

        return JSFXTokenizer::tokenType_error;
    }

    template <typename Iterator>
    static int readNextJSFXToken (Iterator& source)
    {
        source.skipWhitespace();
        auto firstChar = source.peekNextChar();

        switch (firstChar)
        {
        case 0:
            break;

        case '0':   case '1':   case '2':   case '3':   case '4':
        case '5':   case '6':   case '7':   case '8':   case '9':
        case '.':
        {
            auto result = JSFXTokenizerFunctions::parseNumber(source);

            if (result == JSFXTokenizer::tokenType_error)
            {
                source.skip();

                if (firstChar == '.')
                    return JSFXTokenizer::tokenType_punctuation;
            }

            return result;
        }

        case ',':
        case ';':
        case ':':
            source.skip();
            return JSFXTokenizer::tokenType_punctuation;

        case '(':   case ')':
        case '{':   case '}':
        case '[':   case ']':
            source.skip();
            return JSFXTokenizer::tokenType_bracket;

        // TODO: Parse the header separately.
        // Strings cause a lot of issues right now, since in the header we can get random text which
        // can have an open ended string. So we explicitly leave multi-line strings out for now.
        case '"':
        case '\'':
            {
                int rollback = 0;
                auto c = source.nextChar();
                while (c != '\r' && c != '\n' && c != 0) {
                    rollback += 1;
                    c = source.nextChar();

                    // The line terminated within a line, so we can read it without trouble
                    if (c == firstChar) {
                        return JSFXTokenizer::tokenType_string;
                    }
                }
                for (auto i = 0; i < rollback; i++) source.previousChar();

                return JSFXTokenizer::tokenType_punctuation;
            }
        case '+':
            source.skip();
            juce::CppTokeniserFunctions::skipIfNextCharMatches(source, '=');
            return JSFXTokenizer::tokenType_operator;

        case '-':
        {
            source.skip();
            auto result = JSFXTokenizerFunctions::parseNumber(source);

            if (result == JSFXTokenizer::tokenType_error)
            {
                juce::CppTokeniserFunctions::skipIfNextCharMatches(source, '-', '=');
                return JSFXTokenizer::tokenType_operator;
            }

            return result;
        }

        case '*':   case '%':
        case '=':   case '!':
            source.skip();
            juce::CppTokeniserFunctions::skipIfNextCharMatches(source, '=');
            return JSFXTokenizer::tokenType_operator;

        case '/':
        {
            auto previousChar = source.peekPreviousChar();
            source.skip();
            auto nextChar = source.peekNextChar();

            if (nextChar == '/')
            {
                source.skipToEndOfLine();
                return JSFXTokenizer::tokenType_comment;
            }

            if (nextChar == '*')
            {
                // TODO: Parse the header separately.
                // This is a poor workaround for dealing with paths that have /* in them in the header.
                if (!juce::CharacterFunctions::isLetter(previousChar)) {
                    source.skip();
                    juce::CppTokeniserFunctions::skipComment(source);
                    return JSFXTokenizer::tokenType_comment;
                } else {
                    return JSFXTokenizer::tokenType_operator;
                }
            }

            if (nextChar == '=')
                source.skip();

            return JSFXTokenizer::tokenType_operator;
        }

        case '?':
        {
            source.skip();
            auto nextChar2 = source.peekNextChar();
            if (nextChar2 == '>') {
                source.skip();
                return JSFXTokenizer::tokenType_preprocessor;
            }
            return JSFXTokenizer::tokenType_operator;
        }

        case '~':
            source.skip();
            return JSFXTokenizer::tokenType_operator;

        case '<':
        {
            source.skip();
            auto nextChar3 = source.peekNextChar();
            if (nextChar3 == '?') {
                source.skip();
                return JSFXTokenizer::tokenType_preprocessor;
            }
            return JSFXTokenizer::tokenType_operator;
        }

        case '>': case '|': case '&': case '^':
            source.skip();
            juce::CppTokeniserFunctions::skipIfNextCharMatches(source, firstChar);
            juce::CppTokeniserFunctions::skipIfNextCharMatches(source, '=');
            return JSFXTokenizer::tokenType_operator;

        case '#':
            {
                source.skip();
                auto ch = source.peekNextChar();
                while (juce::CharacterFunctions::isLetterOrDigit(ch) || ch == '_') {
                    source.skip();
                    ch = source.peekNextChar();
                }
                return JSFXTokenizer::tokenType_string_hash;
            }

        default:
            if (JSFXTokenizerFunctions::isIdentifierStart(firstChar))
                return JSFXTokenizerFunctions::parseIdentifier(source);

            source.skip();
            break;
        }

        return JSFXTokenizer::tokenType_error;
    }
}

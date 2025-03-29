#pragma once
#include "ysfx.h"
#include <string>
#include <map>

bool ysfx_preprocess(ysfx::text_reader &reader, ysfx_parse_error *error, std::string& in_str, std::map<std::string, ysfx_real> preprocessor_values);

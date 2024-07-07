#pragma once
#include "ysfx.h"
#include <string>

bool ysfx_preprocess(ysfx::text_reader &reader, ysfx_parse_error *error, std::string& in_str);

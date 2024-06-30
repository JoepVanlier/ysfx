// Copyright 2021 Jean Pierre Cimalando
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
// SPDX-License-Identifier: Apache-2.0
//

#include <filesystem>
#include "../sources/ysfx.hpp"
#include "../sources/ysfx_config.hpp"
#include "../sources/ysfx_eel_utils.hpp"
#include "../sources/ysfx_api_eel.hpp"
#include "../sources/ysfx_preprocess.hpp"
#include <vector>
#include <functional>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <set>
#include <map>

namespace fs = std::filesystem;


struct {
    const char *input_file = nullptr;
} args;

template<typename ... Args>
void log(const std::string& format, Args ... args)
{
    int size_s = std::snprintf( nullptr, 0, format.c_str(), args ... ) + 1; // Extra space for '\0'
    if( size_s <= 0 ){ throw std::runtime_error( "Error during formatting." ); }
    auto size = static_cast<size_t>( size_s );
    std::unique_ptr<char[]> buf( new char[ size ] );
    std::snprintf( buf.get(), size, format.c_str(), args ... );
    printf(std::string( buf.get(), buf.get() + size - 1 ).c_str()); // We don't want the '\0' inside
}

void print_help()
{
    log("Usage: preproc_tool -f <filename.jsfx>\n");
    log("Files will be written to a directory named filename_processed.\n");
    log("Note that it _will_ overwrite existing files!.\n");
}

std::string resolve_import_path(const std::string &name, const std::string &origin)
{
    std::vector<std::string> dirs;

    // create the list of search directories
    {
        dirs.reserve(2);

        if (!origin.empty())
            dirs.push_back(ysfx::path_directory(origin.c_str()));
    }

    // the search should be case-insensitive
    static constexpr bool nocase = true;

    static auto *check_existence = +[](const std::string &dir, const std::string &file, std::string &result_path) -> int {
        if (nocase)
            return ysfx::case_resolve(dir.c_str(), file.c_str(), result_path);
        else {
            result_path = dir + file;
            return ysfx::exists(result_path.c_str());
        }
    };

    // search for the file in these directories directly
    for (const std::string &dir : dirs) {
        std::string resolved;
        if (check_existence(dir, name, resolved))
            return resolved;
    }

    // search for the file recursively
    for (const std::string &dir : dirs) {
        struct visit_data {
            const std::string *name = nullptr;
            std::string resolved;
        };
        visit_data vd;
        vd.name = &name;
        auto visit = [](const std::string &dir, void *data) -> bool {
            visit_data &vd = *(visit_data *)data;
            std::string resolved;
            if (check_existence(dir, *vd.name, resolved)) {
                vd.resolved = std::move(resolved);
                return false;
            }
            return true;
        };
        ysfx::visit_directories(dir.c_str(), +visit, &vd);
        if (!vd.resolved.empty())
            return vd.resolved;
    }

    return std::string{};
}


bool preprocess_jsfx(const char* filepath)
{
    std::string input_path = ysfx::path_file_name(filepath);
    size_t lastindex = input_path.find_last_of("."); 
    std::string output_path = input_path.substr(0, lastindex) + "_preprocessed";
    std::map<std::string, std::string> output_files; 

    ysfx_source_unit_u main{new ysfx_source_unit_t};
    {
        ysfx::file_uid main_uid;

        ysfx::FILE_u stream{ysfx::fopen_utf8(filepath, "rb")};
        if (!stream || !ysfx::get_stream_file_uid(stream.get(), main_uid)) {
            log("%s: cannot open file for reading", input_path.c_str());
            return false;
        }

        ysfx::stdio_text_reader raw_reader(stream.get());

        ysfx_parse_error error;
        std::string preprocessed;
        if (!ysfx_preprocess(raw_reader, &error, preprocessed)) {
            log("%s:%u: %s", input_path.c_str(), error.line + 1, error.message.c_str());
            return false;
        }

        ysfx::string_text_reader reader = ysfx::string_text_reader(preprocessed.c_str());
        output_files.insert({filepath, preprocessed});

        if (!ysfx_parse_toplevel(reader, main->toplevel, &error)) {
            log("%s:%u: %s", input_path.c_str(), error.line + 1, error.message.c_str());
            return false;
        }
        ysfx_parse_header(main->toplevel.header.get(), main->header);
    }

    log("Plugin: %s, Author: %s\n\n", main->header.desc.c_str(), main->header.author.c_str());
    log("Output path: %s\n\n", output_path.c_str());

    // Load imports
    static constexpr uint32_t max_import_level = 32;
    std::set<ysfx::file_uid> seen;
    std::vector<ysfx_source_unit_u> imports;

    std::function<bool(const std::string &, const std::string &, uint32_t)> do_next_import =
        [&output_files, &imports, &main, &seen, &do_next_import]
        (const std::string &name, const std::string &origin, uint32_t level) -> bool
        {
            if (level >= max_import_level) {
                log("%s: %s", ysfx::path_file_name(origin.c_str()).c_str(), "too many import levels");
                return false;
            }

            std::string imported_path = resolve_import_path(name, origin);
            if (imported_path.empty()) {
                log("%s: cannot find import: %s", ysfx::path_file_name(origin.c_str()).c_str(), name.c_str());
                return false;
            }

            ysfx::file_uid imported_uid;
            ysfx::FILE_u stream{ysfx::fopen_utf8(imported_path.c_str(), "rb")};
            if (!stream || !ysfx::get_stream_file_uid(stream.get(), imported_uid)) {
                log("%s: cannot open file for reading", ysfx::path_file_name(imported_path.c_str()).c_str());
                return false;
            }

            // this file was already visited, skip
            if (!seen.insert(imported_uid).second)
                return true;

            ysfx_source_unit_u unit{new ysfx_source_unit_t};
            ysfx::stdio_text_reader raw_reader(stream.get());

            // run the preprocessor first
            ysfx_parse_error error;
            std::string preprocessed;
            if (!ysfx_preprocess(raw_reader, &error, preprocessed)) {
                log("%s:%u: %s", ysfx::path_file_name(imported_path.c_str()).c_str(), error.line + 1, error.message.c_str());
                return false;
            }
            ysfx::string_text_reader reader = ysfx::string_text_reader(preprocessed.c_str());

            output_files.insert({imported_path, preprocessed});
            
            // then parse it
            if (!ysfx_parse_toplevel(reader, unit->toplevel, &error)) {
                log("%s:%u: %s", ysfx::path_file_name(imported_path.c_str()).c_str(), error.line + 1, error.message.c_str());
                return false;
            }
            ysfx_parse_header(unit->toplevel.header.get(), unit->header);

            // process the imported dependencies, *first*
            for (const std::string &name : unit->header.imports) {
                if (!do_next_import(name, imported_path.c_str(), level + 1))
                    return false;
            }

            // add it to the import sources, *second*
            imports.push_back(std::move(unit));

            return true;
        };

    for (const std::string &name : main->header.imports) {
        if (!do_next_import(name, filepath, 0))
            return false;
    }

    // Create output folder with processed JSFX
    fs::create_directories(output_path);

    log("Files:\n");

    for (auto it = output_files.begin(); it != output_files.end(); it++)
    {
        std::string relative_path = it->first;
        std::string processed_code = it->second;

        fs::path target_path = fs::path(output_path) / fs::path(relative_path.c_str());
        fs::create_directories(target_path.parent_path());

        std::ofstream file;
        file.open(target_path);
        file << processed_code << std::endl;
        file.close();

        log(" ./%s\n", ysfx::path_file_name(target_path.string().c_str()).c_str());  // Cursed
    }
    
    return true;
}

int main(int argc, char *argv[])
{
    if ((argc < 3) || (strncmp(argv[1], "-f", 2)))
    {
        print_help();
        return 0;
    }
    
    args.input_file = argv[2];

    if (!preprocess_jsfx(args.input_file))
        return 1;

    return 0;
}

// standard
#include <iostream>
#include <string>
#include <filesystem>

// internal
#include "common/ast.h"
#include "common/config.h"
#include "parser.h"

namespace program_errors {
    const std::string no_input = "[E1] input filepath is not given";
}

int main(int argc, char ** argv) {
    tiny::config.parse(argc, argv);
    auto input_filepath = tiny::config.input();
    if (!std::filesystem::exists(input_filepath)) {
        throw std::runtime_error(program_errors::no_input);
    }
    try {
        tiny::ASTPrettyPrinter printer{std::cout}; 
        auto ast_uptr = tinycpp::Parser::ParseFile(input_filepath);
        ast_uptr->print(printer);
    } catch (tiny::ParserError & parseError) {
        std::cerr << "[error] " << parseError.what() << " in \""<< parseError.location().file() << "\"" 
            << " at [" << parseError.location().line() 
            << ":" << parseError.location().col() 
            << "]\n";
    }
}
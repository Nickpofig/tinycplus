// standard
#include <iostream>
#include <string>
#include <filesystem>

// external
#include "common/ast.h"
#include "common/config.h"

// internal
#include "parser.h"
#include "transpiler.h"

namespace program_errors {
    const std::string no_input = "[E1] input filepath is not given";
}

int main(int argc, char ** argv) {
    const std::string keyColorful = "--colorful";
    tiny::config.parse(argc, argv);
    tiny::config.setDefaultIfMissing(keyColorful, "false");
    auto inputFilepath = tiny::config.input();
    bool isPrintColorful = tiny::config.get(keyColorful).compare("true") == 0;
    if (!std::filesystem::exists(inputFilepath)) {
        throw std::runtime_error(program_errors::no_input);
    }
    try {
        tinycpp::TranspilerASTVisiter transplier{std::cout, isPrintColorful};
        auto program = tinycpp::Parser::ParseFile(inputFilepath);
        transplier.visit(program.get());
    } catch (tiny::ParserError & parseError) {
        std::cerr << "[error] " << parseError.what() << " in \""<< parseError.location().file() << "\"" 
            << " at [" << parseError.location().line() 
            << ":" << parseError.location().col() 
            << "]\n";
    }
}
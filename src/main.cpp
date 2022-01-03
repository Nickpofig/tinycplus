// standard
#include <iostream>
#include <string>
#include <filesystem>

// external
#include "common/config.h"

// internal
#include "shared.h"
#include "parser.h"
#include "transpiler.h"
#include "typechecker.h"
#include "astparenter.h"

namespace program_errors {
    const std::string no_input = "[E1] input filepath is not given";
}

// declaration for extern Entry
tiny::Symbol tinycpp::symbols::Entry = tinycpp::symbols::NoEntry;

int main(int argc, char ** argv) {
    const std::string keyColorful = "--colorful";
    const std::string keyEntry = "--entry";
    tiny::config.parse(argc, argv);
    tiny::config.setDefaultIfMissing(keyColorful, "false");
    tiny::config.setDefaultIfMissing(keyEntry, tinycpp::symbols::NoEntry.name());
    auto inputFilepath = tiny::config.input();
    bool isPrintColorful = tiny::config.get(keyColorful).compare("true") == 0;
    tinycpp::symbols::Entry = tiny::Symbol{tiny::config.get(keyEntry)};
    if (!std::filesystem::exists(inputFilepath)) {
        throw std::runtime_error(program_errors::no_input);
    }
    try {
        tinycpp::TypesContext typesContext{};
        tinycpp::NamesContext namesContext{typesContext.getTypeVoid()};
        tinycpp::TypeChecker typechecker{typesContext, namesContext};
        tinycpp::ASTParenter astparenter{};
        tinycpp::Transpiler transpiler{namesContext, typesContext, std::cout, isPrintColorful};
        auto program = tinycpp::Parser::ParseFile(inputFilepath);
        astparenter.visit(program.get());
        typechecker.visit(program.get());
        transpiler.visit(program.get());
        transpiler.validateSelf();
    } catch (tiny::ParserError & parseError) {
        std::cerr << "\n[error] " << parseError.what() << " in \""<< parseError.location().file() << "\"" 
            << " at [" << parseError.location().line() 
            << ":" << parseError.location().col() 
            << "]\n";
    }
}
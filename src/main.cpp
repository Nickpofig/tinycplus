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

namespace program_errors {
    const std::string no_input = "[E1] input filepath is not given";
}

// declaration for extern Entry
tiny::Symbol tinycplus::symbols::Entry = tinycplus::symbols::NoEntry;

const std::string keyColorful = "--colorful";
const std::string keyEntry = "--entry";

void checkForHelpRequest(int argc, char** argv) {
    const std::string tab {"    "};
    for (int i = 1; i < argc; ++i) {
        std::string arg{argv[i]};
        if (arg == "--help" || arg == "-h") {
            std::cerr << "Help:" << std::endl
                << tab
                << "Program always expects one of the arguments to be a filepath to the TinyC+ source code."
                << std::endl;
            std::cerr << "Available arguments:" << std::endl;
            std::cerr << tab << keyColorful << " -> "
                << "when set to [true] prints TinyC output in color."
                << std::endl;
            std::cerr << tab << keyEntry << " -> "
                << "sets the entry point for TinyC output program."
                << std::endl;
            exit(EXIT_SUCCESS);
        }
    }
}

int main(int argc, char ** argv) {
    checkForHelpRequest(argc, argv);
    tiny::config.parse(argc, argv);
    tiny::config.setDefaultIfMissing(keyColorful, "false");
    tiny::config.setDefaultIfMissing(keyEntry, tinycplus::symbols::NoEntry.name());
    auto inputFilepath = tiny::config.input();
    bool isPrintColorful = tiny::config.get(keyColorful).compare("true") == 0;
    tinycplus::symbols::Entry = tiny::Symbol{tiny::config.get(keyEntry)};
    if (!std::filesystem::exists(inputFilepath)) {
        throw std::runtime_error(program_errors::no_input);
    }
    try {
        tinycplus::TypesContext typesContext{};
        tinycplus::NamesContext namesContext{typesContext.getTypeVoid()};
        tinycplus::TypeChecker typechecker{typesContext, namesContext};
        tinycplus::Transpiler transpiler{namesContext, typesContext, std::cout, isPrintColorful};
        auto program = tinycplus::Parser::ParseFile(inputFilepath);
        typechecker.visit(program.get());
        transpiler.visit(program.get());
        transpiler.validateSelf();
    } catch (tiny::ParserError & parseError) {
        std::cerr << "\n[error] " << parseError.what() << " in \""<< parseError.location().file() << "\"" 
            << " at [" << parseError.location().line() 
            << ":" << parseError.location().col() 
            << "]\n";
    } catch (std::exception & exception) {
        std::cerr << "\n[error] " << exception.what() << "\n";
    }
}
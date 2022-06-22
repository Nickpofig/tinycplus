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
#include "tinyc_to_cpp_converter.h"

namespace program_errors {
    const std::string no_input = "[E1] input filepath is not given";
}

// declaration for extern Entry
tiny::Symbol tinycplus::symbols::Entry = tinycplus::symbols::Main;

const std::string keyColorful = "--colorful";
const std::string keyEntry = "--entry";
const std::string keyTinyCtoCpp = "--tinyc-to-cpp"; 
const std::string keyParseOnly = "--parse-only";

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
            std::cerr << tab << keyTinyCtoCpp << " -> "
                << "asks program to treat input file as tinyC file and convert it to general C++ file."
                << std::endl;
            exit(EXIT_SUCCESS);
        }
    }
}

// #include <signal.h>
// void handle_os_signal(int code) {
//     std::cerr << "[OS] interrupt code: " << code << std::endl;
// }

void main(int argc, char ** argv) {
    checkForHelpRequest(argc, argv);
    tiny::config.parse(argc, argv);
    // flags check
    auto inputFilepath = tiny::config.input();
    bool isParseOnly = !tiny::config.setDefaultIfMissing(keyParseOnly, "");
    bool isPrintColorful = !tiny::config.setDefaultIfMissing(keyColorful, "");
    bool isConvertingTinycToCPP = !tiny::config.setDefaultIfMissing(keyTinyCtoCpp, "");
    // entry check
    tiny::config.setDefaultIfMissing(keyEntry, tinycplus::symbols::Main.name());
    tinycplus::symbols::Entry = tiny::Symbol{tiny::config.get(keyEntry)};
    // file check
    if (!std::filesystem::exists(inputFilepath)) {
        throw std::runtime_error(program_errors::no_input);
    }
    if (isConvertingTinycToCPP) {
        std::cout << "Input file: " << inputFilepath << std::endl;
        tinycToCpp::execute(inputFilepath);
        return;
    }
    try {
        tinycplus::TypesContext typesContext{};
        tinycplus::NamesContext namesContext{typesContext.getTypeVoid()};
        tinycplus::TypeChecker typechecker{typesContext, namesContext};
        tinycplus::Transpiler transpiler{namesContext, typesContext, std::cout, isPrintColorful};
        auto program = tinycplus::Parser::ParseFile(inputFilepath);
        if (isParseOnly) {
            tiny::ASTPrettyPrinter printer {std::cout};
            program->print(printer);
            return;
        }
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
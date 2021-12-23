#pragma once

// external
#include "common/helpers.h"
#include "common/ast.h"
#include "common/lexer.h"
#include "common/symbol.h"
#include "common/parser.h"

namespace tinycpp {

    using Lexer = tiny::Lexer;
    using Token = tiny::Token;
    using Symbol = tiny::Symbol;
    using ASTBase = tiny::ASTBase;
    using ASTPrettyPrinter = tiny::ASTPrettyPrinter;

    using ParserError = tiny::ParserError;
    using ParserBase = tiny::ParserBase;

    namespace symbols {
        static Symbol KwClass {"class"};
        static Symbol KwThis {"this"};

        bool static isParsebleKeyword(Symbol const & s) {
            return s == KwClass;
        }
    } // namespace symbols

} // namespace tinycpp
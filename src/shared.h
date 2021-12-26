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
        static Symbol KwClass {"class"}; // special data model representing an object.
        static Symbol KwThis {"this"}; // compulsory first argument of any method, representing reference to the target.
        static Symbol KwBase {"base"}; // cast of "this" to the base type of the current class.
        static Symbol KwVirtual {"virtual"}; // alllows virtual methods.
        static Symbol KwOverride {"override"}; // allows override of virtual methods.

        bool static isParsebleKeyword(Symbol const & s) {
            return s == KwClass || s == KwVirtual || s == KwOverride;
        }
    } // namespace symbols

} // namespace tinycpp
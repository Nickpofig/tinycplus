#pragma once

// external
#include "common/symbol.h"
#include "common/lexer.h"
#include "common/parser.h"
#include "common/ast.h"

namespace tinycpp {

    // Allies to standard types
    template<typename T> using uptr = std::unique_ptr<T>;

    // Allies to internal types
    using color = tiny::color;
    using Lexer = tiny::Lexer;
    using Token = tiny::Token;
    using Symbol = tiny::Symbol;
    using ASTBase = tiny::ASTBase;
    using ASTPrettyPrinter = tiny::ASTPrettyPrinter;

}; // namespace tinycpp
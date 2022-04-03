#pragma once

// external
#include "common/helpers.h"
#include "common/ast.h"
#include "common/lexer.h"
#include "common/symbol.h"
#include "common/parser.h"

namespace tinycplus {

    using Lexer = tiny::Lexer;
    using Token = tiny::Token;
    using Symbol = tiny::Symbol;
    using ASTBase = tiny::ASTBase;
    using ASTPrettyPrinter = tiny::ASTPrettyPrinter;

    using ParserError = tiny::ParserError;
    using ParserBase = tiny::ParserBase;


    class SymbolBuilder {
    private:
        std::stringstream buffer_;
        bool isClosed_;
    public:
        SymbolBuilder(bool isClosed) : isClosed_{isClosed} {
            buffer_ << "__";
        }
    public:
        SymbolBuilder & add(const Symbol & symbol) {
            buffer_ << symbol.name();
            return *this;
        }

        SymbolBuilder & add(const Symbol && symbol) {
            buffer_ << symbol.name();
            return *this;
        }

        template<typename T>
        SymbolBuilder & add(const T & item) {
            buffer_ << item;
            return *this;
        }

        template<typename T>
        SymbolBuilder & add(const T && item) {
            buffer_ << item;
            return *this;
        }

        Symbol end() {
            if (isClosed_) {
                buffer_ << "__";
            }
            return Symbol{buffer_.str()};
        }
    };

    namespace symbols {
        static Symbol KwClass {"class"}; // special data model representing an object.
        static Symbol KwVirtual {"virtual"}; // marks the method as virtual.
        static Symbol KwOverride {"override"}; // marks the method as override of base class virtual method.
        static Symbol KwAbstract {"abstract"}; // marks the method as abstract.
        static Symbol KwThis {"this"}; // compulsory first argument of any method, representing reference to the target.
        static Symbol KwBase {"base"}; // cast of "this" to the base type of the current class.
        static Symbol VTable {"__vtable__"}; // name for class field with vtable pointer type.
        static Symbol NoEntry{"__program__entry__"};
        extern Symbol Entry;

        bool static isParsebleKeyword(Symbol const & s) {
            return s == KwClass || s == KwVirtual || s == KwOverride || s == KwAbstract;
        }

        bool static isReservedName(Symbol const & s) {
            auto tinycppIndex = s.name().rfind("__", 0);
            if (tinycppIndex == 0) {
                return true;
            }
            if (isParsebleKeyword(s)) return true;
            if (s == KwThis || s == KwBase) return true;
            return false;
        }

        static tinycplus::SymbolBuilder startUserName() {
            return tinycplus::SymbolBuilder{false};
        };

        static tinycplus::SymbolBuilder startLanguageName() {
            return tinycplus::SymbolBuilder{true};
        }

    } // namespace symbols

} // namespace tinycplus
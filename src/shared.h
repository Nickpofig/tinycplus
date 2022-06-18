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


    enum class FunctionKind {
        None,
        Method,
        Constructor
    };


    struct PairHash
    {
        template <class T1, class T2>
        std::size_t operator() (const std::pair<T1, T2> &pair) const {
            return std::hash<T1>()(pair.first) ^ std::hash<T2>()(pair.second);
        }
    };

    class SymbolBuilder {
    private:
        std::stringstream buffer_;
        bool isClosed_;
    public:
        SymbolBuilder(bool isClosed) : isClosed_{isClosed} {
            buffer_ << "_";
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
            // if (isClosed_) {
            //     buffer_ << "__";
            // }
            return Symbol{buffer_.str()};
        }
    };

    namespace symbols {
        static Symbol KwClass {"class"}; // TinyC+ class -> special data model representing an object.
        static Symbol KwInterface {"interface"}; // TinyC+ interface -> language polymorphism entity.
        static Symbol KwVirtual {"virtual"}; // marks the method as virtual.
        static Symbol KwOverride {"override"}; // marks the method as override of base class virtual method.
        static Symbol KwAbstract {"abstract"}; // marks the method as abstract.
        static Symbol KwThis {"this"}; // compulsory first argument of any method, representing reference to the target.
        static Symbol KwBase {"base"}; // cast of "this" to the base type of the current class.
        static Symbol KwAccessPublic {"public"};
        static Symbol KwAccessPrivate {"private"};
        static Symbol KwAccessProtected {"protected"};
        static Symbol Main {"main"}; // main function name

        bool static isParsebleKeyword(Symbol const & s) {
            return s == KwClass
                || s == KwInterface
                || s == KwVirtual
                || s == KwOverride
                || s == KwAbstract;
        }

        bool static isReservedName(Symbol const & s) {
            auto tinycplusIndex = s.name().rfind("_", 0);
            if (tinycplusIndex == 0) {
                return true;
            }
            if (isParsebleKeyword(s)) return true;
            if (s == KwThis || s == KwBase) return true;
            return false;
        }

        static tinycplus::SymbolBuilder user() {
            return tinycplus::SymbolBuilder{false};
        };

        static tinycplus::SymbolBuilder system() {
            return tinycplus::SymbolBuilder{true};
        }

        static Symbol HiddenThis {"_this"};
        static Symbol ThisInterface {"_face"}; // instead of "this" for interface methods' first argument
        static Symbol VTable {"_vt"}; // name for class field with vtable pointer type.
        static Symbol NoEntry{"_program_entry"};
        static Symbol ObjectCast {"supercast"}; // method to cast any object into any other object type     .
        extern Symbol Entry;

        // ** virtual tables **

        static Symbol makeVTableStructName(Symbol className) {
            return system().add("VT_").add(className).end();
        }

        static Symbol makeVTableInitFuncName(Symbol className) {
            return system().add("VTinit_").add(className).end();
        }

        static Symbol makeVTableInstance(Symbol className) {
            return system().add("VTinst_").add(className).end();
        }

        // ** interfaces **

        static Symbol makeImplStructName(Symbol interfaceName) {
            return system().add("I_").add(interfaceName).end();
        }

        static Symbol makeImplInitFuncName(Symbol interfaceName, Symbol className) {
            return system()
                .add("Iinit_").add(interfaceName)
                .add("_").add(className).end();
        }

        static Symbol makeImplInstanceName(Symbol interfaceName, Symbol className) {
            return system()
                .add("Iinst_").add(interfaceName)
                .add("_").add(className).end();
        }

        static Symbol makeInterMakeFuncName(Symbol interfaceName, Symbol className) {
            return system()
                .add("Imake_").add(interfaceName)
                .add("_").add(className).end();
        }

        static Symbol makeGetInterImpl(Symbol className) {
            return system().add("Cgeti_").add(className).end();
        }

    } // namespace symbols

} // namespace tinycplus
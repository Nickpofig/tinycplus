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
        ClassMethod,
        InterfaceMethod,
        ClassConstructor
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
        static Symbol KwClassCast {"classcast"};
        static Symbol KwClass {"class"}; // TinyC+ class -> special data model representing an object.
        static Symbol KwInterface {"interface"}; // TinyC+ interface -> language polymorphism entity.
        static Symbol KwVirtual {"virtual"}; // marks the method as virtual.
        static Symbol KwOverride {"override"}; // marks the method as override of base class virtual method.
        static Symbol KwAbstract {"abstract"}; // marks the method as abstract.
        static Symbol KwAccessPublic {"public"};
        static Symbol KwAccessPrivate {"private"};
        static Symbol KwAccessProtected {"protected"};

        // RESERVED IDENTIFIERS
        static Symbol KwThis {"this"}; // compulsory first argument of any method, representing reference to the target.
        static Symbol KwBase {"base"}; // cast of "this" to the base type of the current class.
        static Symbol KwNull {"null"}; // null void pointer
        static Symbol KwObject {"object"}; // default object type

        static Symbol ClassMakeConstructorPrefix {"_Cmake_"};
        static Symbol ClassInitConstructorPrefix {"_Cinit_"};
        static Symbol ClassCastToClassPrefix {"_Ccastc_"};
        static Symbol ClassCastToClassFuncType {"_Ccastcfunc_"};
        static Symbol ClassGetImplPrefix {"_Cgeti_"};
        static Symbol ClassGetImplFuncType {"_Cgetifunc_"};
        static Symbol ClassMethodPrefix {"_Cfunc_"};
        static Symbol ClassMethodFuncTypePrefix {"_Cfuncptr_"}; // prefix for function pointer type of virtual table member.
        static Symbol ClassInterfaceImplInstPrefix {"_Cimpl_"};
        static Symbol ClassSetupFunctionPrefix {"_Csetup_"};

        static Symbol VirtualTableTypePrefix {"_VTtype_"};     // prefix of the virtual table struct
        static Symbol VirtualTableInstancePrefix {"_VTinst_"}; // prefix for global virtual table instance
        static Symbol VirtualTableGeneralStruct {"_VTany_"}; // prefix for global virtual table instance
        static Symbol VirtualTableCastToClassField {"_castc"}; // local to all vtable structs
        static Symbol VirtualTableGetImplField {"_geti"};      // local to all vtable structs

        static Symbol InterfaceViewStruct {"_Iview_"};
        static Symbol InterfaceImplTypePrefix {"_Iimpl_"};
        static Symbol InterfaceMethodFuncTypePrefix {"_Ifunc_"};
        static Symbol InterfaceCastFuncPerfix {"_Icast_"};

        static Symbol Main {"main"}; // main function name
        static Symbol VirtualTableAsField {"_vt"}; // name for class field with vtable pointer type.
        static Symbol InterfaceImplAsField {"impl"};
        static Symbol InterfaceTargetAsField = KwThis;
        static Symbol HiddenThis {"_this"}; // used in constructors as "this" of value type.

        // old: disabled or depricated
        static Symbol ThisInterface {"_face"}; // instead of "this" for interface methods' first argument
        static Symbol NoEntry{"_program_entry"};
        static Symbol ObjectCast {"supercast"}; // method to cast any object into any other object type     .
        extern Symbol Entry;


        bool static isParsebleKeyword(Symbol const & s) {
            return s == KwClass
                || s == KwInterface
                || s == KwVirtual
                || s == KwOverride
                || s == KwAbstract
                || s == KwAccessPublic
                || s == KwAccessPrivate
                || s == KwAccessProtected
                || s == KwClassCast
                ;
        }

        bool static isReservedName(Symbol const & s) {
            auto tinycplusIndex = s.name().rfind("_", 0);
            if (tinycplusIndex == 0) {
                return true;
            }
            if (isParsebleKeyword(s)) return true;
            if (s == KwThis || s == KwBase || s == KwNull || s == KwObject) return true;
            return false;
        }

        static tinycplus::SymbolBuilder start() {
            return tinycplus::SymbolBuilder{};
        };

        // // ** virtual tables **

        // static Symbol makeVTableStructName(Symbol className) {
        //     return system().add("VT_").add(className).end();
        // }

        // static Symbol makeVTableInitFuncName(Symbol className) {
        //     return system().add("VTinit_").add(className).end();
        // }

        // static Symbol makeVTableInstance(Symbol className) {
        //     return system().add("VTinst_").add(className).end();
        // }

        // // ** interfaces **

        static Symbol makeImplStructName(Symbol interfaceName) {
            return start().add(InterfaceImplTypePrefix).add(interfaceName).end();
        }

        static Symbol makeInterfaceMethodFuncType(Symbol interfaceName, Symbol methodName) {
            return symbols::start().add(symbols::InterfaceMethodFuncTypePrefix).add(interfaceName).add("_").add(methodName).end();
        }

        static Symbol makeClassMethodFuncType(Symbol className, Symbol methodName) {
            return symbols::start().add(symbols::ClassMethodFuncTypePrefix).add(className).add("_").add(methodName).end();
        }

        // static Symbol makeImplInitFuncName(Symbol interfaceName, Symbol className) {
        //     return system()
        //         .add("Iinit_").add(interfaceName)
        //         .add("_").add(className).end();
        // }

        // static Symbol makeImplInstanceName(Symbol interfaceName, Symbol className) {
        //     return system()
        //         .add("Iinst_").add(interfaceName)
        //         .add("_").add(className).end();
        // }

        // static Symbol makeInterMakeFuncName(Symbol interfaceName, Symbol className) {
        //     return system()
        //         .add("Imake_").add(interfaceName)
        //         .add("_").add(className).end();
        // }

        // static Symbol makeGetInterImpl(Symbol className) {
        //     return system().add("Cgeti_").add(className).end();
        // }

    } // namespace symbols

} // namespace tinycplus
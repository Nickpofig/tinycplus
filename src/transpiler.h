#pragma once

// standard
#include <iostream>
#include <vector>

// internal
#include "ast.h"
#include "types.h"
#include "contexts.h"

namespace tinycpp {

    class Transpiler : public ASTVisitor {
    private: // persistant data
        NamesContext & names_;
        TypesContext & types_;
        ASTPrettyPrinter printer_;
        bool isPrintColorful_ = false;
        std::unordered_map<Symbol, int> definitions;
    private: // temporary data
        int inheritanceDepth = 0;
        bool programEntryWasDefined_ = false;
    public:
        Transpiler(NamesContext & names, TypesContext & types, std::ostream & output, bool isColorful)
            :names_{names}
            ,types_{types}
            ,printer_{output}
            ,isPrintColorful_{isColorful}
        { }
    public:
        void validateSelf() {
            if (!programEntryWasDefined_ && symbols::Entry != symbols::NoEntry) {
                throw std::runtime_error(STR("Entry function " << symbols::Entry << " was not defined!"));
            }
        }
    private:
        void registerDeclaration(Symbol realName, Symbol name, int definitionsLimit = 0) {
            auto result = definitions.find(realName);
            if (result == definitions.end()) {
                definitions.insert(std::make_pair(realName, definitionsLimit));
            } else {
                auto & limit = definitions.at(realName);
                if (--limit < 0) {
                    throw std::runtime_error{STR("Multiple redefinitions of " << name)};
                }
            }
        }
        void validatedName(Symbol const & name) {
            if (symbols::isReservedName(name)) {
                throw std::runtime_error{STR("Name " << name << " contains or is a reserved TinyC+ name!")};
            }
        }
        inline void print(Symbol const & symbol, tiny::color color) {
            if (isPrintColorful_) printer_ << color;
            printer_ << symbol.name();
        }
        void printSpace() { printer_ << " "; }
        void printSymbol(Symbol const & name) { print(name, printer_.symbol);}
        void printIdentifier(Symbol const & name) { print(name, printer_.identifier); }
        void printType(Symbol const & name) { print(name, printer_.type); }
        void printType(Type * type) {
            if (isPrintColorful_) printer_ << printer_.type;
            printer_ << type->toString();
        }
        void printKeyword(Symbol const & name) { print(name, printer_.keyword); }
        template<typename T>
        void printNumber(T value) {
            if (isPrintColorful_) printer_ << printer_.numberLiteral;
            printer_ << value;
        }
        void printComment(std::string const & text, bool newline = true) {
            if (isPrintColorful_) printer_ << printer_.comment;
            printer_ << "// " << text;
            if (newline) printer_.newline();
        }
        void printVTableInitFunctionDeclaration(Type::Class * classType) {
            auto vtableType = classType->getVirtualTable();
            if (vtableType == nullptr) return;
            auto returnType = types_.getTypeVoid();
            auto functionName = vtableType->getGlobalInitFunctionName();
            if (!names_.addGlobalVariable(functionName, returnType)) return;
            // * return type
            printType(returnType);
            printSpace();
            // * name
            printIdentifier(functionName);
            // * arguments
            printSymbol(Symbol::ParOpen);
            printSymbol(Symbol::ParClose);
            printSpace();
            // * body start
            printSymbol(Symbol::CurlyOpen);
            printer_.indent();
            printer_.newline();
            {
                std::vector<std::pair<Symbol, Type::Complex::Member>>  vtableMembers;
                vtableType->collectMembersOrdered(vtableMembers);
                int remained = vtableMembers.size();
                for (auto & member : vtableMembers) {
                    // e.g ~~> this.vtable->functionPtr = function;
                    printIdentifier(vtableType->getGlobalInstanceName());
                    printSymbol(Symbol::Dot);
                    printIdentifier(member.first);
                    printSpace();
                    printSymbol(Symbol::Assign);
                    printSpace();
                    auto methodInfo = classType->getMethodInfo(member.first);
                    printSymbol(Symbol::BitAnd);
                    printIdentifier(methodInfo.fullName);
                    printSymbol(Symbol::Semicolon);
                    if(--remained > 0) {
                        printer_.newline();
                    }
                }
            }
            // * body end
            printer_.dedent();
            printer_.newline();
            printSymbol(Symbol::CurlyClose);
            printer_.newline();
            printer_.newline();
        }
        void printComplexTypeConstructorDeclaration(Type::Complex * complexType) {
            if (!complexType->requiresImplicitConstruction()) return;
            if (!names_.addGlobalVariable(complexType->getConstructorName(), complexType)) return;
            // * return type
            printType(complexType);
            printSpace();
            // * name
            printIdentifier(complexType->getConstructorName());
            // * arguments
            printSymbol(Symbol::ParOpen);
            printSymbol(Symbol::ParClose);
            printSpace();
            // * body start
            printSymbol(Symbol::CurlyOpen);
            printer_.indent();
            printer_.newline();
            {
                // ** class instance declaration
                printType(complexType->toString());
                printSpace();
                printIdentifier(symbols::KwThis);
                printSymbol(Symbol::Semicolon);
                printer_.newline();
                // ** class instance vtable assignment
                bool vtableRequiresInit = symbols::Entry == symbols::NoEntry;
                if (auto * classType = complexType->as<Type::Class>()) {
                    auto * vtable = classType->getVirtualTable();
                    if (vtableRequiresInit && vtable) {
                        printIdentifier(vtable->getGlobalInitFunctionName());
                        printSymbol(Symbol::ParOpen);
                        printSymbol(Symbol::ParClose);
                        printSymbol(Symbol::Semicolon);
                        printer_.newline();
                    }
                    printIdentifier(symbols::KwThis);
                    printSymbol(Symbol::Dot);
                    printIdentifier(symbols::VTable);
                    printSpace();
                    printSymbol(Symbol::Assign);
                    printSpace();
                    if (vtable) {
                        printSymbol(Symbol::BitAnd);
                        printIdentifier(vtable->getGlobalInstanceName());
                    } else {
                        printKeyword(Symbol::KwCast);
                        printSymbol(Symbol::Lt);
                        printKeyword(Symbol::KwVoid);
                        printSymbol(Symbol::Mul);
                        printSymbol(Symbol::Gt);
                        printSymbol(Symbol::ParOpen);
                        printNumber(0);
                        printSymbol(Symbol::ParClose);
                    }
                    printSymbol(Symbol::Semicolon);
                    printer_.newline();
                }
                // ** class instance field construction
                std::vector<std::pair<Symbol, Type::Complex::Member>> fields;
                complexType->collectMembersOrdered(fields);
                for (auto & field : fields) {
                    auto memberType = field.second.type;
                    if (memberType->isPointer()) continue;
                    auto * fieldComplexType = memberType->as<Type::Complex>();
                    if (fieldComplexType == nullptr) continue;
                    // e.g ~~> this.field = fieldClassConstructor();
                    printIdentifier(symbols::KwThis);
                    printSymbol(Symbol::Dot);
                    printIdentifier(field.first);
                    printSpace();
                    printSymbol(Symbol::Assign);
                    printSpace();
                    printIdentifier(fieldComplexType->getConstructorName());
                    printSymbol(Symbol::ParOpen);
                    printSymbol(Symbol::ParClose);
                    printSymbol(Symbol::Semicolon);
                    printer_.newline();
                }
            }
            // * return class instance
            printKeyword(Symbol::KwReturn);
            printSpace();
            printIdentifier(symbols::KwThis);
            printSymbol(Symbol::Semicolon);
            printer_.dedent();
            printer_.newline();
            // * body end
            printSymbol(Symbol::CurlyClose);
            printer_.newline();
        }
    public:
        void visit(AST * ast) override;
        void visit(ASTInteger * ast) override;
        void visit(ASTDouble * ast) override;
        void visit(ASTChar * ast) override;
        void visit(ASTString * ast) override;
        void visit(ASTIdentifier * ast) override;
        void visit(ASTType * ast) override;
        void visit(ASTPointerType * ast) override;
        void visit(ASTArrayType * ast) override;
        void visit(ASTNamedType * ast) override;
        void visit(ASTSequence * ast) override;
        void visit(ASTBlock * ast) override;
        void visit(ASTVarDecl * ast) override;
        void visit(ASTFunDecl * ast) override;
        void visit(ASTFunPtrDecl * ast) override;
        void visit(ASTStructDecl * ast) override;
        void visit(ASTClassDecl * ast) override;
        void visit(ASTMethodDecl * ast) override;
        void visit(ASTIf * ast) override;
        void visit(ASTSwitch * ast) override;
        void visit(ASTWhile * ast) override;
        void visit(ASTDoWhile * ast) override;
        void visit(ASTFor * ast) override;
        void visit(ASTBreak * ast) override;
        void visit(ASTContinue * ast) override;
        void visit(ASTReturn * ast) override;
        void visit(ASTBinaryOp * ast) override;
        void visit(ASTAssignment * ast) override;
        void visit(ASTUnaryOp * ast) override;
        void visit(ASTUnaryPostOp * ast) override;
        void visit(ASTAddress * ast) override;
        void visit(ASTDeref * ast) override;
        void visit(ASTIndex * ast) override;
        void visit(ASTMember * ast) override;
        void visit(ASTCall * ast) override;
        void visit(ASTCast * ast) override;
    }; // class Transpiler

}; // namespace tinycpp
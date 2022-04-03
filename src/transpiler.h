#pragma once

// standard
#include <iostream>
#include <vector>

// internal
#include "ast.h"
#include "types.h"
#include "contexts.h"

namespace tinycplus {

    class Transpiler : public ASTVisitor {
    private: // persistant data
        NamesContext & names_;
        TypesContext & types_;
        ASTPrettyPrinter printer_;
        bool isPrintColorful_ = false;
        std::unordered_map<Symbol, int> definitions_;
        std::vector<AST*> current_ast_hierarchy_;
    private: // temporary data
        bool programEntryWasDefined_ = false;
        std::vector<Type::VTable*> bufferVtableTypes_;
        std::vector<FieldInfo> bufferFields_;
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
        void pushAst(AST * ast) {
            current_ast_hierarchy_.push_back(ast);
        }
        // Used only with [pushAst] as a self-cleaning process
        void popAst() {
            current_ast_hierarchy_.pop_back();
        }
        bool isRootLevel() {
            return current_ast_hierarchy_.size() == 0;
        }
        AST * peekAst() {
            assert(!isRootLevel());
            return current_ast_hierarchy_.back();
        }
        void registerDeclaration(Symbol realName, Symbol name, int definitionsLimit = 0) {
            auto result = definitions_.find(realName);
            if (result == definitions_.end()) {
                definitions_.insert(std::make_pair(realName, definitionsLimit));
            } else {
                auto & limit = definitions_.at(realName);
                if (--limit < 0) {
                    throw std::runtime_error{STR("Multiple redefinitions of " << name)};
                }
            }
        }
        void validateName(Symbol const & name) {
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
        void printStructHeader(Type::Complex * type) {
            printKeyword(Symbol::KwStruct);
            printSpace();
            printIdentifier(type->toString());
            printSpace();
        }
        void printScopeOpen() {
            printSymbol(Symbol::CurlyOpen);
            printer_.indent();
        }
        void printScopeClose(bool isSemicolonTerminated) {
            printer_.dedent();
            printer_.newline();
            printSymbol(Symbol::CurlyClose);
            if (isSemicolonTerminated) {
                printSymbol(Symbol::Semicolon);
            }
            printer_.newline();
        }
        void printFields(std::vector<FieldInfo> & fields) {
            for (auto & field : fields) {
                printer_.newline();
                printType(field.type);
                printSpace();
                printIdentifier(field.name);
                printSymbol(Symbol::Semicolon);
            }
        }
        void printFunctionPointerTypeDeclaration(Type::Alias * type) {
            auto * functionType = type->base()->getCore<Type::Function>();
            assert(functionType != nullptr && "oh no, it is not a function pointer type alias");
            // * declaration starts
            printKeyword(Symbol::KwTypedef);
            printSpace();
            printType(functionType->returnType());
            // * name as pointer
            printSpace();
            printSymbol(Symbol::ParOpen);
            printSymbol(Symbol::Mul);
            printType(type);
            printSymbol(Symbol::ParClose);
            // * arguments
            printSymbol(Symbol::ParOpen);
            for (auto i = 0; i < functionType->numArgs(); i++) {
                if (i > 0) {
                    printSymbol(Symbol::Comma);
                    printSpace();
                }
                printType(functionType->argType(i));
            }
            printSymbol(Symbol::ParClose);
            // * declaration ends
            printSymbol(Symbol::Semicolon);
            printer_.newline();
        }
        void printVTableDeclaration(Type::Class * classType) {
            if (classType->isAbstract()) {
                return;
            }
            std::vector<FieldInfo> vtableFields;
            auto * vtableType = classType->getVirtualTable();
            vtableType->collectFieldsOrdered(vtableFields);
            // * vtable struct declaration
            printStructHeader(vtableType);
            printScopeOpen();
            // ** prints function pointers for each virtual method with respect to class precedence order
            printFields(vtableFields);
            printScopeClose(true);
            printer_.newline();
            // * global instance declaration
            printType(vtableType);
            printSpace();
            printIdentifier(vtableType->getGlobalInstanceName());
            printSymbol(Symbol::Semicolon);
            printer_.newline();
            printer_.newline();
        }
        void printVTableInitFunctionDeclaration(Type::Class * classType) {
            auto vtableType = classType->getVirtualTable();
            if (vtableType == nullptr) return;
            auto returnType = types_.getTypeVoid();
            auto functionName = vtableType->getGlobalInitFunctionName();
            if (!names_.addGlobalVariable(functionName, returnType)) return;
            auto vtableGlobalInstanceName = vtableType->getGlobalInstanceName();
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
            {
                std::vector<FieldInfo> vtableFields;
                vtableType->collectFieldsOrdered(vtableFields);
                for (auto & field : vtableFields) {
                    // e.g ~~> this.vtable->functionPtr = function;
                    printer_.newline();
                    printIdentifier(vtableGlobalInstanceName);
                    printSymbol(Symbol::Dot);
                    printIdentifier(field.name);
                    printSpace();
                    printSymbol(Symbol::Assign);
                    printSpace();
                    auto methodInfo = classType->getMethodInfo(field.name).value();
                    printSymbol(Symbol::BitAnd);
                    printIdentifier(methodInfo.fullName);
                    printSymbol(Symbol::Semicolon);
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
                // ** class instance field construction (all fields)
                std::vector<FieldInfo> fields;
                complexType->collectFieldsOrdered(fields);
                for (auto & field : fields) {
                    auto memberType = field.type;
                    if (memberType->isPointer()) continue;
                    auto * fieldComplexType = memberType->as<Type::Complex>();
                    if (fieldComplexType == nullptr) continue;
                    // e.g ~~> this.field = fieldClassConstructor();
                    printIdentifier(symbols::KwThis);
                    printSymbol(Symbol::Dot);
                    printIdentifier(field.name);
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

}; // namespace tinycplus
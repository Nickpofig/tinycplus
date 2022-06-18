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
        AST * peekAst(int depth) {
            assert(current_ast_hierarchy_.size() >= depth);
            return *(current_ast_hierarchy_.rbegin() + depth);
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

        #pragma region Printer Shortcuts
        inline void print(Symbol const & symbol, tiny::color color) {
            if (isPrintColorful_) printer_ << color;
            printer_ << symbol.name();
        }
        inline void printSpace() {
            printer_ << " ";
        }
        inline void printNewline() {
            printer_.newline();
        }
        inline void printIndent() {
            printer_. indent();
        }
        inline void printDedent() {
            printer_. dedent();
        }
        inline void printSymbol(Symbol const & name) {
            print(name, printer_.symbol);
        }
        inline void printIdentifier(Symbol const & name) {
            print(name, printer_.identifier);
        }
        inline void printType(Symbol const & name) {
            print(name, printer_.type);
        }
        inline void printKeyword(Symbol const & name) {
            print(name, printer_.keyword);
        }
        inline void printNumber(int value) {
            if (isPrintColorful_) printer_ << printer_.numberLiteral;
            printer_ << value;
        }
        inline void printNumber(int64_t value) {
            if (isPrintColorful_) printer_ << printer_.numberLiteral;
            printer_ << value;
        }
        inline void printNumber(double value) {
            if (isPrintColorful_) printer_ << printer_.numberLiteral;
            printer_ << value;
        }
        inline void printComment(std::string const & text, bool newline = true) {
            if (isPrintColorful_) printer_ << printer_.comment;
            printer_ << "// " << text;
            if (newline) printNewline();
        }
        #pragma endregion

        inline void printType(Type * type) {
            if (isPrintColorful_) printer_ << printer_.type;
            printer_ << type->toString();
        }
        inline void printScopeOpen() {
            printSymbol(Symbol::CurlyOpen);
            printIndent();
        }
        inline void printScopeClose(bool isSemicolonTerminated) {
            printDedent();
            printNewline();
            printSymbol(Symbol::CurlyClose);
            if (isSemicolonTerminated) {
                printSymbol(Symbol::Semicolon);
            }
            printNewline();
        }
        inline void printFields(std::vector<FieldInfo> & fields) {
            for (auto & field : fields) {
                printNewline();
                printType(field.type);
                printSpace();
                printIdentifier(field.name);
                printSymbol(Symbol::Semicolon);
            }
        }
        void printFunctionPointerType(Type::Alias * type) {
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
            printNewline();
        }
        void printVTableStruct(Type::Class * classType) {
            if (classType->isAbstract()) {
                return;
            }
            std::vector<FieldInfo> vtableFields;
            auto * vtableType = classType->getVirtualTable();
            vtableType->collectFieldsOrdered(vtableFields);
            // * vtable struct declaration
            printKeyword(Symbol::KwStruct);
            printSpace();
            printIdentifier(vtableType->typeName);
            printSpace();
            printScopeOpen();
            // ** prints function pointers for each virtual method with respect to class precedence order
            printFields(vtableFields);
            printScopeClose(true);
            // * global instance declaration
            printNewline();
            printType(vtableType->typeName);
            printSpace();
            printIdentifier(vtableType->instanceName);
            printSymbol(Symbol::Semicolon);
            printNewline();
            printNewline();
        }
        void printVTableInitFunction(Type::Class * classType) {
            auto vtableType = classType->getVirtualTable();
            if (vtableType == nullptr) return;
            auto returnType = types_.getTypeVoid();
            // * return type
            printType(returnType);
            printSpace();
            // * name
            printIdentifier(vtableType->initName);
            // * arguments
            printSymbol(Symbol::ParOpen);
            printSymbol(Symbol::ParClose);
            printSpace();
            // * body start
            printSymbol(Symbol::CurlyOpen);
            printIndent();
            {
                std::vector<FieldInfo> vtableFields;
                vtableType->collectFieldsOrdered(vtableFields);
                for (auto & field : vtableFields) {
                    // e.g ~~> this.vtable->functionPtr = function;
                    printNewline();
                    printIdentifier(vtableType->instanceName);
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
            printDedent();
            printNewline();
            printSymbol(Symbol::CurlyClose);
            printNewline();
            printNewline();
        }
        void printImplStruct(Type::Interface * interfaceType, Type::Class * classType) {
            // printKeyword(Symbol::KwStruct);
            // printSpace();
            // printIdentifier(generator_.getImplStruct(interfaceType, classType));
        }
        void printInitFunction(Type::Interface * interfaceType, Type::Class * classType) {
            // // * return type
            // printType(types_.getTypeVoid());
            // printSpace();
            // // * name
            // printIdentifier(generator_.getImplStruct(interfaceType ,classType));
            // // * arguments
            // printSymbol(Symbol::ParOpen);
            // printSymbol(Symbol::ParClose);
            // printSpace();
            // // * body start
            // printSymbol(Symbol::CurlyOpen);
            // printIndent();
            // {
            //     std::vector<FieldInfo> vtableFields;
            //     vtableType->collectFieldsOrdered(vtableFields);
            //     for (auto & field : vtableFields) {
            //         // e.g ~~> this.vtable->functionPtr = function;
            //         printNewline();
            //         printIdentifier(generator_.getInstance(vtableType));
            //         printSymbol(Symbol::Dot);
            //         printIdentifier(field.name);
            //         printSpace();
            //         printSymbol(Symbol::Assign);
            //         printSpace();
            //         auto methodInfo = classType->getMethodInfo(field.name).value();
            //         printSymbol(Symbol::BitAnd);
            //         printIdentifier(methodInfo.fullName);
            //         printSymbol(Symbol::Semicolon);
            //     }
            // }
            // // * body end
            // printDedent();
            // printNewline();
            // printSymbol(Symbol::CurlyClose);
            // printNewline();
            // printNewline();
        }
        void printDefaultConstructor(Type::Class * classType) {
            if (classType)
            // * return type
            printType(classType);
            printSpace();
            // * name
            printIdentifier(classType->makeName);
            // * arguments
            printSymbol(Symbol::ParOpen);
            printSymbol(Symbol::ParClose);
            printSpace();
            // * body start
            printSymbol(Symbol::CurlyOpen);
            printIndent();
            printNewline();
            {
                // ** class instance declaration
                printType(classType->toString());
                printSpace();
                printIdentifier(symbols::KwThis);
                printSymbol(Symbol::Semicolon);
                printNewline();
                // ** class instance vtable assignment
                printVTableInstanceAssignment(classType, false);
                // // ** class instance field construction (all fields)
                // std::vector<FieldInfo> fields;
                // classType->collectFieldsOrdered(fields);
                // for (auto & field : fields) {
                //     auto memberType = field.type;
                //     if (memberType->isPointer()) continue;
                //     auto * fieldClassType = memberType->as<Type::Complex>();
                //     if (fieldClassType == nullptr) continue;
                //     // e.g ~~> this.field = fieldClassConstructor();
                //     printIdentifier(symbols::KwThis);
                //     printSymbol(Symbol::Dot);
                //     printIdentifier(field.name);
                //     printSpace();
                //     printSymbol(Symbol::Assign);
                //     printSpace();
                //     printIdentifier(fieldClassType->getConstructorName());
                //     printSymbol(Symbol::ParOpen);
                //     printSymbol(Symbol::ParClose);
                //     printSymbol(Symbol::Semicolon);
                //     printNewline();
                // }
            }
            // * return class instance
            printKeyword(Symbol::KwReturn);
            printSpace();
            printIdentifier(symbols::KwThis);
            printSymbol(Symbol::Semicolon);
            printDedent();
            printNewline();
            // * body end
            printSymbol(Symbol::CurlyClose);
            printNewline();
        }
        
        void printVTableInstanceAssignment(Type::Class * classType, bool asPointer) {
            auto * vtable = classType->getVirtualTable();
            printIdentifier(symbols::KwThis);
            printSymbol(asPointer ? Symbol::ArrowR : Symbol::Dot);
            printIdentifier(symbols::VTable);
            printSpace();
            printSymbol(Symbol::Assign);
            printSpace();
            printSymbol(Symbol::BitAnd);
            printIdentifier(vtable->instanceName);
            printSymbol(Symbol::Semicolon);
            printNewline();
        }

        bool classConstructorIsIniting = true;
        void printConstructor(ASTFunDecl * ast) {
            auto * classType = peekAst()->getType()->as<Type::Class>();
            pushAst(ast);
            // * function return type
            if (classConstructorIsIniting) {
                printKeyword(Symbol::KwVoid);
            } else {
                visitChild(ast->typeDecl.get());
            }
            printSpace();
            // * function name
            printIdentifier(classConstructorIsIniting ? classType->initName : classType->makeName);
            // registerDeclaration(name.name(), name.name(), 1);
            // * function arguments
            printSymbol(Symbol::ParOpen);
            if (classConstructorIsIniting) {
                printType(classType->name);
                printSpace();
                printSymbol(Symbol::Mul);
                printSpace();
                printIdentifier(symbols::KwThis);
                printSymbol(Symbol::Comma);
                printSpace();
            }
            auto arg = ast->args.begin();
            if (arg != ast->args.end()) {
                visitChild(arg[0].get());
                while (++arg != ast->args.end()) {
                    printSymbol(Symbol::Comma);
                    printSpace();
                    visitChild(arg[0].get());
                }
            }
            printSymbol(Symbol::ParClose);
            printSpace();
            visitChild(ast->body.get());
            popAst();
        }

        void printFunction(ASTFunDecl * ast) {
            pushAst(ast);
            auto name = ast->name.value();
            validateName(name);
            // * function return type
            visitChild(ast->typeDecl.get());
            printSpace();
            // * function name
            printIdentifier(name.name());
            registerDeclaration(name.name(), name.name(), 1);
            // * function arguments
            printSymbol(Symbol::ParOpen);
            auto arg = ast->args.begin();
            if (arg != ast->args.end()) {
                visitChild(arg[0].get());
                while (++arg != ast->args.end()) {
                    printSymbol(Symbol::Comma);
                    printSpace();
                    visitChild(arg[0].get());
                }
            }
            printSymbol(Symbol::ParClose);
            // * function body
            if (ast->body) {
                printSpace();
                visitChild(ast->body.get());
            } else {
                printSymbol(Symbol::Semicolon);
            }
            popAst();
        }

        void printMethod(ASTFunDecl * ast) {
            auto * classParent = peekAst()->as<ASTClassDecl>();
            assert(classParent && "must have an ast class decl as parent ast");
            pushAst(ast);
            auto name = ast->name.value();
            validateName(name);
            // * method return type
            visitChild(ast->typeDecl.get());
            printSpace();
            // * method name
            auto classType = classParent->getType()->as<Type::Class>();
            bool isInterfaceMethod = classType->getMethodInfo(name).value().isInterfaceMethod;
            auto info = classType->getMethodInfo(name).value();
            registerDeclaration(info.fullName, name, 1);
            printIdentifier(info.fullName);
            // * method arguments
            printSymbol(Symbol::ParOpen);
            // inserts pointer to the owner class as the first argument
            printType(classParent->name.name());
            printSymbol(Symbol::Mul);
            printSpace();
            printIdentifier(isInterfaceMethod
                ? symbols::ThisInterface
                : symbols::KwThis
            );
            if (ast->args.size() > 0) {
                printSymbol(Symbol::Comma);
                printSpace();
            }
            auto arg = ast->args.begin();
            if (arg != ast->args.end()) {
                visitChild(arg[0].get());
                while (++arg != ast->args.end()) {
                    printSymbol(Symbol::Comma);
                    printSpace();
                    visitChild(arg[0].get());
                }
            }
            printSymbol(Symbol::ParClose);
            // * method body
            if (ast->body) {
                printSpace();
                visitChild(ast->body.get());
            } else {
                printSymbol(Symbol::Semicolon);
            }
            popAst();
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
        void visit(ASTProgram * ast) override;
        void visit(ASTVarDecl * ast) override;
        void visit(ASTFunDecl * ast) override;
        void visit(ASTFunPtrDecl * ast) override;
        void visit(ASTStructDecl * ast) override;
        void visit(ASTInterfaceDecl * ast) override;
        void visit(ASTClassDecl * ast) override;
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
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
            // if (!programEntryWasDefined_ && symbols::Entry != symbols::Main) {
            //     throw std::runtime_error(STR("Entry function " << symbols::Entry << " was not defined!"));
            // }
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
                throw std::runtime_error{STR("Name " << name << " is a reserved TinyC+ name!")};
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
            printNewline();
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

        inline void printField(Symbol type, Symbol name) {
            printType(type);
            printSpace();
            printIdentifier(name);
            printSymbol(Symbol::Semicolon);
            printNewline();
        }

        inline void printField(Type * type, Symbol name) {
            printField(Symbol{type->toString()}, name);
        }

        inline void printFields(std::vector<FieldInfo> & fields) {
            for (auto & field : fields) {
                printField(field.type->toString(), field.name);
            }
        }

        Symbol getClassImplInstanceName(Type::Interface * interfaceType, Type::Class * classType) {
            return symbols::start().add(symbols::ClassInterfaceImplInstPrefix)
                .add(classType->name).add("_").add(interfaceType->name)
                .end();
        }

        void printFunctionPointerType(Type::Alias * type) {
            auto * functionType = type->base()->unwrap<Type::Function>();
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

        void printVTableDefaultFields() {
            // ** this fields
            printField(types_.castToClassFuncPtrType, symbols::VirtualTableCastToClassField);
            // ** impl field
            printField(types_.getImplFuncPtrType, symbols::VirtualTableGetImplField);
        }

        void printVTableStruct(Type::Class * classType) {
            std::vector<FieldInfo> vtableFields;
            auto * vtableType = classType->getVirtualTable();
            vtableType->collectFieldsOrdered(vtableFields);
            // * vtable struct declaration
            printKeyword(Symbol::KwStruct);
            printSpace();
            printIdentifier(vtableType->typeName);
            printSpace();
            printScopeOpen();
            printVTableDefaultFields();
            // ** prints function pointers for each virtual method with respect to class precedence order
            printFields(vtableFields);
            printScopeClose(true);
            // * global instance declaration
            if (!classType->isAbstract()) {
                printNewline();
                printType(vtableType->typeName);
                printSpace();
                printIdentifier(vtableType->instanceName);
                printSymbol(Symbol::Semicolon);
                printNewline();
                printNewline();
            }
        }

        void printClassCast(ASTClassCast * ast) {
            auto * targetClassType = ast->type->getType()->unwrap<Type::Class>();
            auto * targetInterfaceType = ast->type->getType()->unwrap<Type::Interface>();
            auto * subjectClassType = ast->value->getType()->unwrap<Type::Class>();
            auto * subjectInterfaceType = ast->value->getType()->unwrap<Type::Interface>();
            if (targetInterfaceType != nullptr) {
                printIdentifier(targetInterfaceType->castName);
                printSymbol(Symbol::ParOpen);
                if (subjectInterfaceType != nullptr) {
                    // "interface to interface" case
                    visitChild(ast->value.get());
                } else if (subjectClassType != nullptr) {
                    // "class to interface" case
                    printKeyword(Symbol::KwCast);
                    printSymbol(Symbol::Lt);
                    printType(types_.getTypeVoidPtr());
                    printSymbol(Symbol::Gt);
                    printSymbol(Symbol::ParOpen);
                    visitChild(ast->value.get());
                    printSymbol(Symbol::ParClose);
                }
                printSymbol(Symbol::ParClose);
            } else if (targetClassType != nullptr && (subjectClassType == nullptr || !subjectClassType->inherits(targetClassType))) {
                printKeyword(Symbol::KwCast);
                printSymbol(Symbol::Lt);
                visitChild(ast->type.get());
                printSymbol(Symbol::Gt);
                printSymbol(Symbol::ParOpen);
                {
                    printIdentifier(symbols::ClassCastToClassFunction);
                    printSymbol(Symbol::ParOpen);
                    if (subjectInterfaceType != nullptr) {
                        visitChild(ast->value.get());
                    } else if (subjectClassType != nullptr) {
                        printKeyword(Symbol::KwCast);
                        printSymbol(Symbol::Lt);
                        printType(types_.getTypeVoidPtr());
                        printSymbol(Symbol::Gt);
                        printSymbol(Symbol::ParOpen);
                        visitChild(ast->value.get());
                        printSymbol(Symbol::ParClose);
                    }
                    printSymbol(Symbol::Comma);
                    printNumber(targetClassType->getId());
                    printSymbol(Symbol::ParClose);
                }
                printSymbol(Symbol::ParClose);
            } else {
                printKeyword(Symbol::KwCast);
                printSymbol(Symbol::Lt);
                visitChild(ast->type.get());
                printSymbol(Symbol::Gt);
                printSymbol(Symbol::ParOpen);
                visitChild(ast->value.get());
                printSymbol(Symbol::ParClose);
            }
        }

        void printAllMethodsForwardDeclaration(ASTClassDecl * classAst, Type::Class * classType) {
            // ** methods forward declaration
            for (auto & method : classAst->methods) {
                auto methodInfo = classType->getMethodInfo(method->name.value());
                visitChild(method->typeDecl.get());
                printSpace();
                printIdentifier(methodInfo->fullName);
                printSymbol(Symbol::ParOpen);
                printType(classType);
                printType(Symbol::Mul);
                printSpace();
                printIdentifier(symbols::KwThis);
                for (size_t i = 0; i < method->args.size(); i++) {
                    printSymbol(Symbol::Comma);
                    printSpace();
                    visitChild(method->args[i]->type.get());
                    printSpace();
                    visitChild(method->args[i]->name.get());
                }
                printSymbol(Symbol::ParClose);
                printSymbol(Symbol::Semicolon);
                printNewline();
            }
            // ** constructors forward declaration
            for (auto & constructor : classAst->constructors) {
                classConstructorIsIniting = true;
                printConstructor(constructor.get(), true);
                classConstructorIsIniting = false;
                if (!classType->isAbstract()) {
                    printConstructor(constructor.get(), true);
                }
            }
        }

        void printFuncPtrAssignment(
            Symbol base,
            Symbol fptr,
            Symbol function,
            std::optional<Symbol> typeToCast = std::nullopt
        ) {
            printIdentifier(base);
            printSymbol(Symbol::Dot);
            printIdentifier(fptr);
            printSpace();
            printSymbol(Symbol::Assign);
            printSpace();
            if (typeToCast.has_value()) {
                printSymbol(Symbol::KwCast);
                printSymbol(Symbol::Lt);
                printType(typeToCast.value());
                printSymbol(Symbol::Gt);
                printSymbol(Symbol::ParOpen);
            }
            printSymbol(Symbol::BitAnd);
            printIdentifier(function);
            if (typeToCast.has_value()) {
                printSymbol(Symbol::ParClose);
            }
            printSymbol(Symbol::Semicolon);
            printNewline();
        }

        void printClassSetupFunction(Type::Class * classType) {
            auto vtableType = classType->getVirtualTable();
            // * return type
            printType(types_.getTypeVoid());
            printSpace();
            // * name
            printIdentifier(classType->setupName);
            // * arguments
            printSymbol(Symbol::ParOpen);
            printSymbol(Symbol::ParClose);
            printSpace();
            // * body start
            printScopeOpen();
            {
                // ** sets fileds of globale vtable instance
                std::vector<FieldInfo> vtableFields;
                vtableType->collectFieldsOrdered(vtableFields);
                printComment(STR("setup of vtable instance"));
                printFuncPtrAssignment(vtableType->instanceName, symbols::VirtualTableCastToClassField, classType->classCastName);
                printFuncPtrAssignment(vtableType->instanceName, symbols::VirtualTableGetImplField, classType->getImplName);
                for (auto & field : vtableFields) {
                    // e.g ~~> this.vtable->functionPtr = function;
                    auto methodInfo = classType->getMethodInfo(field.name).value();
                    printFuncPtrAssignment(vtableType->instanceName, field.name, methodInfo.fullName);
                }
                printNewline();
                // ** set fields of each interfae implementation
                if (classType->interfaces.size() > 0) {
                    printComment(STR("setup of interface implementation instances"));
                }
                for (auto & face : classType->interfaces) {
                    auto * interfaceType = face.second;
                    auto implInstance = getClassImplInstanceName(interfaceType, classType);
                    for (auto & method : interfaceType->methods_) {
                        auto classMethod = classType->getMethodInfo(method.first).value();
                        printFuncPtrAssignment(
                            implInstance,
                            method.first,
                            classMethod.fullName,
                            method.second.ptrType->toString()
                        );
                    }
                }
            }
            // * body end
            printScopeClose(false);
            printNewline();
        }

        /* type: (void* class instance) -> interface view
        */
        void printCastToInterfaceFunction(Type::Interface * type) {
            auto argInstName = Symbol{"inst"};
            auto localVtableName = Symbol{"vtable"};
            auto localImplName = Symbol{"impl"};
            auto localViewName = Symbol{"view"};
            // * return type
            printType(symbols::InterfaceViewStruct);
            printSpace();
            // * name
            printIdentifier(type->castName);
            // * arguments
            printSymbol(Symbol::ParOpen);
            printType(types_.getTypeVoid());
            printSymbol(Symbol::Mul);
            printSpace();
            printIdentifier(argInstName);
            printSymbol(Symbol::ParClose);
            printSpace();
            // * body start
            printScopeOpen();
            {
                // * interface view instance
                printType(symbols::InterfaceViewStruct);
                printSpace();
                printIdentifier(localViewName);
                printSymbol(Symbol::Semicolon);
                printNewline();
                // * check input instance for being null
                printKeyword(Symbol::KwIf);
                printSpace();
                printKeyword(Symbol::ParOpen);
                {
                    printIdentifier(argInstName);
                    printSpace();
                    printSymbol(Symbol::Eq);
                    printSpace();
                    printIdentifier(symbols::KwNull);
                }
                printKeyword(Symbol::ParClose);
                printSpace();
                printScopeOpen(); // null case
                {
                    // * assigns (null) impl ptr to view
                    printIdentifier(localViewName);
                    printSymbol(Symbol::Dot);
                    printIdentifier(symbols::InterfaceImplAsField);
                    printSpace();
                    printSymbol(Symbol::Assign);
                    printSpace();
                    printIdentifier(symbols::KwNull);
                    printSymbol(Symbol::Semicolon);
                    printNewline();
                    // * assigns (null) target ptr to view
                    printIdentifier(localViewName);
                    printSymbol(Symbol::Dot);
                    printIdentifier(symbols::InterfaceTargetAsField);
                    printSpace();
                    printSymbol(Symbol::Assign);
                    printSpace();
                    printIdentifier(symbols::KwNull);
                    printSymbol(Symbol::Semicolon);
                }
                printScopeClose(false);
                printKeyword(Symbol::KwElse);
                printSpace();
                printScopeOpen(); // not null case
                {
                    // * declares general vtable ptr
                    // * casts instance to general vtable ptr (as they share the same memory position)
                    printType(symbols::VirtualTableGeneralStruct);
                    printType(Symbol::Mul);
                    printSpace();
                    printIdentifier(localVtableName);
                    printSpace();
                    printSymbol(Symbol::Assign);
                    printSpace();
                    printSymbol(Symbol::KwCast);
                    printSymbol(Symbol::Lt);
                    printType(symbols::VirtualTableGeneralStruct);
                    printType(Symbol::Mul);
                    printSymbol(Symbol::Gt);
                    printSymbol(Symbol::ParOpen);
                    printIdentifier(argInstName);
                    printSymbol(Symbol::ParClose);
                    printSymbol(Symbol::Semicolon);
                    printNewline();
                    // * stores result of "check impl" function call
                    printType(type->implStructName);
                    printType(Symbol::Mul);
                    printSpace();
                    printIdentifier(localImplName);
                    printSpace();
                    printSymbol(Symbol::Assign);
                    printSpace();
                    { // cast from (void*) to (impl struct*)
                        printSymbol(Symbol::KwCast);
                        printSymbol(Symbol::Lt);
                        printType(type->implStructName);
                        printType(Symbol::Mul);
                        printSymbol(Symbol::Gt);
                        printSymbol(Symbol::ParOpen);
                        { // vtable get implementation call
                            printIdentifier(localVtableName);
                            printSymbol(Symbol::ArrowR);
                            printIdentifier(symbols::VirtualTableGetImplField);
                            printSymbol(Symbol::ParOpen);
                            printNumber(type->getId());
                            printSymbol(Symbol::ParClose);
                        }
                        printSymbol(Symbol::ParClose);
                        printSymbol(Symbol::Semicolon);
                    }
                    printNewline();
                    // * "check impl" result in if statement
                    printKeyword(Symbol::KwIf);
                    printSymbol(Symbol::ParOpen);
                    printIdentifier(localImplName);
                    printSpace();
                    printSymbol(Symbol::Eq);
                    printSpace();
                    printIdentifier(symbols::KwNull);
                    printSymbol(Symbol::ParClose);
                    printScopeOpen(); // is null case
                    {
                        // * assigns (null) impl ptr to view
                        printIdentifier(localViewName);
                        printSymbol(Symbol::Dot);
                        printIdentifier(symbols::InterfaceImplAsField);
                        printSpace();
                        printSymbol(Symbol::Assign);
                        printSpace();
                        printIdentifier(symbols::KwNull);
                        printSymbol(Symbol::Semicolon);
                        printNewline();
                        // * assigns (null) target ptr to view
                        printIdentifier(localViewName);
                        printSymbol(Symbol::Dot);
                        printIdentifier(symbols::InterfaceTargetAsField);
                        printSpace();
                        printSymbol(Symbol::Assign);
                        printSpace();
                        printIdentifier(symbols::KwNull);
                        printSymbol(Symbol::Semicolon);
                    }
                    printScopeClose(false);
                    printKeyword(Symbol::KwElse);
                    printScopeOpen(); // is not null case
                    {
                        // * assigns impl ptr to view
                        printIdentifier(localViewName);
                        printSymbol(Symbol::Dot);
                        printIdentifier(symbols::InterfaceImplAsField);
                        printSpace();
                        printSymbol(Symbol::Assign);
                        printSpace();
                        printIdentifier(localImplName);
                        printSymbol(Symbol::Semicolon);
                        printNewline();
                        // * assigns target ptr to view
                        printIdentifier(localViewName);
                        printSymbol(Symbol::Dot);
                        printIdentifier(symbols::InterfaceTargetAsField);
                        printSpace();
                        printSymbol(Symbol::Assign);
                        printSpace();
                        printIdentifier(argInstName);
                        printSymbol(Symbol::Semicolon);
                    }
                    printScopeClose(false);
                }
                printScopeClose(false);
            }
            // * body end
            printKeyword(Symbol::KwReturn);
            printSpace();
            printIdentifier(localViewName);
            printSymbol(Symbol::Semicolon);
            printScopeClose(false);
            printNewline();
        }

        void printGlobalClassCastFunction() {
            auto argInstName = Symbol{"inst"};
            auto argIdName = Symbol{"id"};
            printType(types_.getTypeVoidPtr());
            printSpace();
            printIdentifier(symbols::ClassCastToClassFunction);
            printSymbol(Symbol::ParOpen);
            printType(types_.getTypeVoidPtr());
            printSpace();
            printIdentifier(argInstName);
            printSymbol(Symbol::Comma);
            printType(types_.getTypeInt());
            printSpace();
            printSymbol(argIdName);
            printSymbol(Symbol::ParClose);
            printScopeOpen();
            {
                printKeyword(Symbol::KwIf);
                printSpace();
                printSymbol(Symbol::ParOpen);
                printIdentifier(argInstName);
                printSpace();
                printSymbol(Symbol::Eq);
                printSpace();
                printIdentifier(symbols::KwNull);
                printSymbol(Symbol::ParClose);
                printSpace();
                printScopeOpen(); // null case
                {
                    printKeyword(Symbol::KwReturn);
                    printSpace();
                    printIdentifier(symbols::KwNull);
                    printSymbol(Symbol::Semicolon);
                }
                printScopeClose(false);
                printKeyword(Symbol::KwElse);
                printScopeOpen(); // not null case
                {
                    printKeyword(Symbol::KwReturn);
                    printSpace();
                    printKeyword(Symbol::KwCast);
                    printSymbol(Symbol::Lt);
                    printType(symbols::VirtualTableGeneralStruct);
                    printType(Symbol::Mul);
                    printSymbol(Symbol::Gt);
                    printSymbol(Symbol::ParOpen);
                    printIdentifier(argInstName);
                    printSymbol(Symbol::ParClose);
                    printSymbol(Symbol::ArrowR);
                    printIdentifier(symbols::VirtualTableCastToClassField);
                    printSymbol(Symbol::ParOpen);
                    printIdentifier(argInstName);
                    printSymbol(Symbol::Comma);
                    printIdentifier(argIdName);
                    printSymbol(Symbol::ParClose);
                    printSymbol(Symbol::Semicolon);
                }
                printScopeClose(false);
            }
            printScopeClose(false);
        }

        void printGetImplFunction(Type::Class * classType) {
            auto argIdName = Symbol{"id"};
            // * return type
            printType(types_.getTypeVoid());
            printSymbol(Symbol::Mul);
            printSpace();
            // * name
            printIdentifier(classType->getImplName);
            // * arguments
            printSymbol(Symbol::ParOpen);
            printType(types_.getTypeInt());
            printSpace();
            printIdentifier(argIdName);
            printSymbol(Symbol::ParClose);
            printSpace();
            // * body start
            printScopeOpen();
            {
                // ** switches between base class ids
                printKeyword(Symbol::KwSwitch);
                printSymbol(Symbol::ParOpen);
                printIdentifier(argIdName);
                printSymbol(Symbol::ParClose);
                printScopeOpen();
                for (auto & it : classType->interfaces) {
                    // ** class index case
                    printKeyword(Symbol::KwCase);
                    printSpace();
                    printNumber(it.second->getId());
                    printSymbol(Symbol::Colon);
                    printSpace();
                    printKeyword(Symbol::KwReturn);
                    printSpace();
                    printSymbol(Symbol::KwCast);
                    printSymbol(Symbol::Lt);
                    printType(types_.getTypeVoidPtr());
                    printSymbol(Symbol::Gt);
                    printSymbol(Symbol::ParOpen);
                    printSymbol(Symbol::BitAnd);
                    printIdentifier(getClassImplInstanceName(it.second, classType));
                    printSymbol(Symbol::ParClose);
                    printSymbol(Symbol::Semicolon);
                    printNewline();
                }
                // ** default case
                printKeyword(Symbol::KwDefault);
                printSymbol(Symbol::Colon);
                printSpace();
                printKeyword(Symbol::KwReturn);
                printSpace();
                printSymbol(symbols::KwNull);
                printSymbol(Symbol::Semicolon);
                printScopeClose(false);
            }
            // * body end
            printScopeClose(false);
            printNewline();
        }

        void printCastToClassFunction(Type::Class * classType) {
            auto argInstName = Symbol{"inst"};
            auto argIdName = Symbol{"id"};
            // * return type
            printType(types_.getTypeVoid());
            printSymbol(Symbol::Mul);
            printSpace();
            // * name
            printIdentifier(classType->classCastName);
            // * arguments
            printSymbol(Symbol::ParOpen);
            printType(types_.getTypeVoid());
            printSymbol(Symbol::Mul);
            printSpace();
            printIdentifier(argInstName);
            printSymbol(Symbol::Comma);
            printType(types_.getTypeInt());
            printSpace();
            printIdentifier(argIdName);
            printSymbol(Symbol::ParClose);
            printSpace();
            // * body start
            printScopeOpen();
            {
                // ** switches between base class ids
                printKeyword(Symbol::KwSwitch);
                printSymbol(Symbol::ParOpen);
                printIdentifier(argIdName);
                printSymbol(Symbol::ParClose);
                printScopeOpen();
                for (auto * base = classType; base != nullptr; base = base->getBase()) {
                    // ** class index case
                    printKeyword(Symbol::KwCase);
                    printSpace();
                    printNumber(base->getId());
                    printSymbol(Symbol::Colon);
                    printSpace();
                    printKeyword(Symbol::KwReturn);
                    printSpace();
                    printSymbol(argInstName);
                    printSymbol(Symbol::Semicolon);
                    printNewline();
                }
                // ** default case
                printKeyword(Symbol::KwDefault);
                printSymbol(Symbol::Colon);
                printSpace();
                printKeyword(Symbol::KwReturn);
                printSpace();
                printSymbol(symbols::KwNull);
                printSymbol(Symbol::Semicolon);
                printScopeClose(false);
            }
            // * body end
            printScopeClose(false);
            printNewline();
        }

        void printDefaultConstructor(Type::Class * classType) {
            if (!classConstructorIsIniting && classType->isAbstract()) return;
            // * return type
            if (classConstructorIsIniting) {
                printType(Symbol::KwVoid);
            } else {
                printType(classType);
            }
            printSpace();
            // * name
            printIdentifier(classConstructorIsIniting
                ? classType->getConstructorInitName(classType->defaultConstructorFuncType)
                : classType->getConstructorMakeName(classType->defaultConstructorFuncType));
            // * arguments
            printSymbol(Symbol::ParOpen);
            if (classConstructorIsIniting) {
                printType(classType->name);
                printSpace();
                printSymbol(Symbol::Mul);
                printSpace();
                printIdentifier(symbols::KwThis);
            }
            printSymbol(Symbol::ParClose);
            printSpace();
            // * body start
            printScopeOpen();
            if (!classConstructorIsIniting) {
                // ** class instance declaration
                printType(classType->toString());
                printSpace();
                printIdentifier(symbols::KwThis);
                printSymbol(Symbol::Semicolon);
                printNewline();
                // ** class instance vtable assignment
                printVTableInstanceAssignment(classType, false);
                // * return class instance
                printKeyword(Symbol::KwReturn);
                printSpace();
                printIdentifier(symbols::KwThis);
                printSymbol(Symbol::Semicolon);
            }
            // * body end
            printScopeClose(false);
            printNewline();
        }

        void printVTableInstanceAssignment(Type::Class * classType, bool asPointer) {
            auto * vtable = classType->getVirtualTable();
            printIdentifier(symbols::KwThis);
            printSymbol(asPointer ? Symbol::ArrowR : Symbol::Dot);
            printIdentifier(symbols::VirtualTableAsField);
            printSpace();
            printSymbol(Symbol::Assign);
            printSpace();
            printSymbol(Symbol::BitAnd);
            printIdentifier(vtable->instanceName);
            printSymbol(Symbol::Semicolon);
            printNewline();
        }

        bool classConstructorIsIniting = true;
        void printConstructor(ASTFunDecl * ast, bool asForwardDeclaration) {
            auto * classType = peekAst()->getType()->as<Type::Class>();
            auto * funcType = ast->getType()->as<Type::Function>();
            pushAst(ast);
            // * function return type
            if (classConstructorIsIniting) {
                printKeyword(Symbol::KwVoid);
            } else {
                visitChild(ast->typeDecl.get());
            }
            printSpace();
            // * function name
            printIdentifier(classConstructorIsIniting
                ? classType->getConstructorInitName(funcType)
                : classType->getConstructorMakeName(funcType));
            // registerDeclaration(name.name(), name.name(), 1);
            // * function arguments
            printSymbol(Symbol::ParOpen);
            if (classConstructorIsIniting) {
                printType(classType->name);
                printSpace();
                printSymbol(Symbol::Mul);
                printSpace();
                printIdentifier(symbols::KwThis);
                if (ast->args.size() > 0) {
                    printSymbol(Symbol::Comma);
                    printSpace();
                }
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
            if (asForwardDeclaration) {
                printSymbol(Symbol::Semicolon);
                printNewline();
            } else {
                printSpace();
                visitChild(ast->body.get());
            }
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
            auto info = classType->getMethodInfo(name).value();
            registerDeclaration(info.fullName, name, 1);
            printIdentifier(info.fullName);
            // * method arguments
            printSymbol(Symbol::ParOpen);
            // inserts pointer to the owner class as the first argument
            printType(classParent->name.name());
            printSymbol(Symbol::Mul);
            printSpace();
            printIdentifier(symbols::KwThis);
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

        void printFunctionPointerCall(ASTMember * member, ASTCall * call) {
            visitChild(member->base.get());
            printSymbol(member->op);
            visitChild(call);
        }

        void printInterfaceMethodCall(ASTMember * member, ASTCall * call, Type::Interface * interfaceType) {
            auto methodName = call->function->as<ASTIdentifier>();
            auto baseAsIdent = member->base->as<ASTIdentifier>();
            printKeyword(Symbol::KwCast);
            printSymbol(Symbol::Lt);
            printType(interfaceType->implStructName);
            printType(Symbol::Mul);
            printSymbol(Symbol::Gt);
            printSymbol(Symbol::ParOpen);
            printIdentifier(baseAsIdent->name);
            printSymbol(Symbol::Dot);
            printIdentifier(symbols::InterfaceImplAsField);
            printSymbol(Symbol::ParClose);
            printSymbol(Symbol::ArrowR);
            printIdentifier(methodName->name);
            // * arguments
            printSymbol(Symbol::ParOpen);
            {
                visitChild(member->base.get());
                // * the rest of arguments
                for(auto & arg : call->args) {
                    printSymbol(Symbol::Comma);
                    printSpace();
                    visitChild(arg.get());
                }
            }
            printSymbol(Symbol::ParClose);
        }

        void printClassMethodCall(ASTMember * member, ASTCall * call, Type::Class * classType) {
            bool isPointerAccess = member->op == Symbol::ArrowR;
            auto methodName = call->function->as<ASTIdentifier>();
            auto methodInfo = classType->getMethodInfo(methodName->name).value();
            auto * targetClassType = methodInfo.type->argType(0)->unwrap<Type::Class>();
            auto baseAsIdent = member->base->as<ASTIdentifier>();
            bool isBaseCall = baseAsIdent != nullptr && baseAsIdent->name != symbols::KwBase;
            bool methodIsVirtual = methodInfo.ast->isVirtualized();
            if (methodIsVirtual && isBaseCall) {
                visitChild(member->base.get());
                printSymbol(isPointerAccess ? Symbol::ArrowR : Symbol::Dot);
                printIdentifier(symbols::VirtualTableAsField);
                printSymbol(Symbol::ArrowR);
                printIdentifier(methodName->name);
            } else {
                // direct call
                printIdentifier(methodInfo.fullName);
            }
            // * arguments
            printSymbol(Symbol::ParOpen);
            {
                // * target as the first argument
                if (classType != targetClassType) {
                    // downcasts because method belongs to base class
                    printKeyword(Symbol::KwCast);
                    printSymbol(Symbol::Lt);
                    printType(targetClassType->toString());
                    printType(Symbol::Mul);
                    printSymbol(Symbol::Gt);
                    printSymbol(Symbol::ParOpen);
                    if (member->op == Symbol::Dot) {
                        printSymbol(Symbol::BitAnd);
                    }
                    visitChild(member->base.get());
                    printSymbol(Symbol::ParClose);
                } else {
                    // no cast
                    if (member->op == Symbol::Dot) {
                        printSymbol(Symbol::BitAnd);
                    }
                    visitChild(member->base.get());
                }
                // * the rest of arguments
                for(auto & arg : call->args) {
                    printSymbol(Symbol::Comma);
                    printSpace();
                    visitChild(arg.get());
                }
            }
            printSymbol(Symbol::ParClose);
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
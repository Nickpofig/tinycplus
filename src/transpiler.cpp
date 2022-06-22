#include <typeinfo>
#include <vector>
#include <unordered_set>

// internal
#include "transpiler.h"
#include "types.h"

namespace tinycplus {

    void Transpiler::visit(AST * ast) {
         visitChild(ast);
    }

    void Transpiler::visit(ASTInteger * ast) {
        printNumber(ast->value);
    }

    void Transpiler::visit(ASTDouble * ast) {
        printNumber(ast->value);
    }

    void Transpiler::visit(ASTChar * ast) {
        if (isPrintColorful_) printer_ << printer_.charLiteral;
        printer_ << STR('\'' << ast->value << '\'');
    }

    void Transpiler::visit(ASTString * ast) {
        if (isPrintColorful_) printer_ << printer_.stringLiteral;
        printer_ << '\"' << ast->value << '\"';
    }

    void Transpiler::visit(ASTIdentifier * ast) {
        if (ast->name == symbols::KwBase) {
            // downcasts because method belongs to base class
            printKeyword(Symbol::KwCast);
            printSymbol(Symbol::Lt);
            printType(ast->getType()->toString());
            printSymbol(Symbol::Gt);
            printSymbol(Symbol::ParOpen);
            printIdentifier(symbols::KwThis);
            printSymbol(Symbol::ParClose);
        } else {
            auto interfaceType = ast->getType()->unwrap<Type::Interface>();
            if (interfaceType != nullptr) {
                auto parent = peekAst();
                if (parent->as<ASTAssignment>()
                    || parent->as<ASTVarDecl>()
                    || parent->as<ASTCall>()
                ) {  // pass as is for (call, assignments, declarations, cast)
                    printIdentifier(ast->name);
                } else { // submit class instance whenever possible
                    printIdentifier(ast->name);
                    printSymbol(Symbol::Dot);
                    printIdentifier(symbols::InterfaceTargetAsField);
                }
            } else {
                printIdentifier(ast->name);
            }
        }
    }

    void Transpiler::visit(ASTType * ast) {
        // unreachable
    }

    void Transpiler::visit(ASTPointerType * ast) {
        pushAst(ast);
        if (ast->base->getType()->as<Type::Interface>()) {
            printType(symbols::InterfaceViewStruct);
        } else {
            visitChild(ast->base.get());
            printSymbol(Symbol::Mul);
        }
        popAst();
    }

    void Transpiler::visit(ASTArrayType * ast) {
        pushAst(ast);
        visitChild(ast->base.get());
        printSymbol(Symbol::SquareOpen);
        visitChild(ast->size.get());
        printSymbol(Symbol::SquareClose);
        popAst();
    }

    void Transpiler::visit(ASTNamedType * ast) {
        pushAst(ast);
        auto * type = ast->getType()->as<Type::Class>();
        if (type == types_.defaultClassType) {
            printType(Symbol::KwVoid);
        } else {
            printType(ast->name.name());
        }
        popAst();
    }

    void Transpiler::visit(ASTSequence * ast) {
        pushAst(ast);
        auto i = ast->body.begin();
        if (i != ast->body.end()) {
            visitChild(i[0].get()); // visits sequence element
            while (++i != ast->body.end()) {
                printSymbol(Symbol::Comma);
                printSpace();
                visitChild(i[0].get()); // visits sequence element
            }
        }
        popAst();
    }

    void Transpiler::visit(ASTBlock * ast) {
        auto * functionAst = peekAst()->as<ASTFunDecl>();
        auto * classAst = peekAst(1)->as<ASTClassDecl>();
        pushAst(ast);
        /// TODO: refactor code -> root is a special case that happens once - no need to check for it for all blocks
        printSymbol(Symbol::CurlyOpen);
        printer_.indent();
        if (functionAst != nullptr) {
            if (functionAst->isClassConstructor()) {
                auto classType = classAst->getType()->as<Type::Class>();
                printNewline();
                if (!classConstructorIsIniting) {
                    // ** hidden class instance (x)
                    printType(classType);
                    printSpace();
                    printIdentifier(symbols::HiddenThis);
                    printSymbol(Symbol::Semicolon);
                    printNewline();
                    // ** pointer to instance (x)
                    printType(classType);
                    printSpace();
                    printSymbol(Symbol::Mul);
                    printSpace();
                    printIdentifier(symbols::KwThis);
                    printSpace();
                    printSymbol(Symbol::Assign);
                    printSpace();
                    printSymbol(Symbol::BitAnd);
                    printIdentifier(symbols::HiddenThis);
                    printSymbol(Symbol::Semicolon);
                    printNewline();
                    // ** assigns vtable
                    printVTableInstanceAssignment(classType, true);
                }
                // ** call base class constructor init version
                if (functionAst->base.has_value()) {
                    auto & base = functionAst->base.value();
                    auto baseClassType = types_.getType(base.getName())->as<Type::Class>();
                    auto baseConstructorType = base.name->getType()->as<Type::Function>();
                    printIdentifier(baseClassType->getConstructorInitName(baseConstructorType));
                    printSymbol(Symbol::ParOpen);
                    // *** passed "this" into init call
                    printKeyword(Symbol::KwCast);
                    printSymbol(Symbol::Lt);
                    printType(baseClassType->name);
                    printSymbol(Symbol::Mul);
                    printSymbol(Symbol::Gt);
                    printSymbol(Symbol::ParOpen);
                    printIdentifier(symbols::KwThis);
                    printSymbol(Symbol::ParClose);
                    // *** other arguments
                    for (size_t i = 0; i < base.args.size(); i++) {
                        printSymbol(Symbol::Comma);
                        printSpace();
                        printIdentifier(base.args[i]->name);
                    }
                    printSymbol(Symbol::ParClose);
                    printSymbol(Symbol::Semicolon);
                    printNewline();
                }
                if (!classConstructorIsIniting) {
                    printNewline();
                    printSymbol(Symbol::KwReturn);
                    printSpace();
                    printSymbol(symbols::HiddenThis);
                    printSymbol(Symbol::Semicolon);
                }
            } else if (functionAst->name == symbols::Main) {
                // Program Entry must be fully declared only after all class declarations and never earlier.
                // Otherwise resulted TinyC code won't compile.
                programEntryWasDefined_ = true;
                printNewline();
                std::vector<Type::Class*> classTypes;
                types_.findEachClassType(classTypes);
                printComment(" === Initializing virtual tables === ");
                for (auto * classType : classTypes) {
                    if (classType == types_.defaultClassType) continue;
                    printIdentifier(classType->setupName);
                    printSymbol(Symbol::ParOpen);
                    printSymbol(Symbol::ParClose);
                    printSymbol(Symbol::Semicolon);
                    printNewline();
                }
                printNewline();
                printComment(" === Running the rest of the program === ");
            }
        }  
        for (auto & i : ast->body) {
            printNewline();
            visitChild(i.get());
            /// TODO: check semicolon is set correctly when necessary
            if (!i->as<ASTBlock>()
                && !i->as<ASTIf>()
                && !i->as<ASTSwitch>()
                && !i->as<ASTWhile>()
                && !i->as<ASTFor>()
            ) {
                printSymbol(Symbol::Semicolon);
            }
        }
        printer_.dedent();
        printer_.newline();
        printSymbol(Symbol::CurlyClose);
        printer_.newline();
        popAst();
    }

    void Transpiler::visit(ASTProgram * ast) {
        pushAst(ast);
        printComment(" --- Generated program globals --- ");

        {
            // * null pointer declaration
            printType(types_.getTypeVoidPtr());
            printSpace();
            printIdentifier(symbols::KwNull);
            printSpace();
            printSymbol(Symbol::Assign);
            printSpace();
            printSymbol(Symbol::KwCast);
            printSymbol(Symbol::Lt);
            printType(types_.getTypeVoidPtr());
            printSymbol(Symbol::Gt);
            printSymbol(Symbol::ParOpen);
            printNumber(0);
            printSymbol(Symbol::ParClose);
            printSymbol(Symbol::Semicolon);
            printNewline();

            // * global class cast wrapper
            printGlobalClassCastFunction();
        }

        // * default interface view struct
        {
            printKeyword(Symbol::KwStruct);
            printSpace();
            printIdentifier(symbols::InterfaceViewStruct);
            printSpace();
            printScopeOpen();
            {
                // ** this fields
                printField(types_.getTypeVoidPtr(), symbols::InterfaceTargetAsField);
                // ** impl field
                printField(types_.getTypeVoidPtr(), symbols::InterfaceImplAsField);
            }
            printScopeClose(true);
            printNewline();
        }

        // * default vtable struct
        {
            
            printFunctionPointerType(types_.castToClassFuncPtrType);
            printFunctionPointerType(types_.getImplFuncPtrType);

            printKeyword(Symbol::KwStruct);
            printSpace();
            printIdentifier(symbols::VirtualTableGeneralStruct);
            printSpace();
            printScopeOpen();
            {
                printVTableDefaultFields();
            }
            printScopeClose(true);
            printNewline();
        }

        // Forward decalration of all class types
        std::vector<Type::Class*> classTypes;
        types_.findEachClassType(classTypes);
        printComment(" --- Classes --- ");
        for (auto * classType : classTypes) {
            if (classType == types_.defaultClassType) continue;
            printKeyword(Symbol::KwStruct);
            printSpace();
            printIdentifier(classType->name);
            printSymbol(Symbol::Semicolon);
            printNewline();
        }

        printComment(" --- User program starts --- ");

        for (auto & i : ast->body) {
            visitChild(i.get());
            printer_.newline();
            printer_.newline();
        }
        popAst();
    }

    void Transpiler::visit(ASTVarDecl * ast) {
        auto parentAst = peekAst();
        pushAst(ast);
        validateName(ast->name->name);
        if (auto arrayType = ast->type->as<ASTArrayType>()) {
            // base type part
            visitChild(arrayType->base.get());
            printSpace();
            // variable name
            visitChild(ast->name.get());
            // array type part
            printSymbol(Symbol::SquareOpen);
            visitChild(arrayType->size.get());
            printSymbol(Symbol::SquareClose);
        } else if (auto interfaceType = ast->getType()->unwrap<Type::Interface>()) {
            // variable type
            printType(symbols::InterfaceViewStruct);
            printSpace();
            // variable name
            visitChild(ast->name.get());
        } else {
            // base type part
            visitChild(ast->type.get());
            printSpace();
            // variable name
            visitChild(ast->name.get());
        }
        // immediate value assignment
        if (ast->value.get() != nullptr) {
            printSpace();
            printSymbol(Symbol::Assign);
            printSpace();
            visitChild(ast->value.get());
        }
        if (parentAst->as<ASTProgram>()
            || parentAst->as<ASTStructDecl>()
            || parentAst->as<ASTClassDecl>()
            || parentAst->as<ASTInterfaceDecl>())
        {
            printSymbol(Symbol::Semicolon);
        }
        popAst();
    }




    void Transpiler::visit(ASTFunDecl * ast) {
        if (ast->isClassMethod()) printMethod(ast);
        else if (ast->isClassConstructor()) printConstructor(ast);
        else printFunction(ast);
    }




    void Transpiler::visit(ASTStructDecl * ast) {
        pushAst(ast);
        validateName(ast->name);
        printKeyword(Symbol::KwStruct);
        printSpace();
        printIdentifier(ast->name.name());
        printSpace();
        if (ast->isDefinition) {
            printSymbol(Symbol::CurlyOpen);
            printer_.indent();
            for (auto & i : ast->fields) {
                printer_.newline();
                visitChild(i.get());
            }
            printer_.dedent();
            printer_.newline();
            printSymbol(Symbol::CurlyClose);
            printSymbol(Symbol::Semicolon);
        }
        printer_.newline();
        popAst();
    }




    void Transpiler::visit(ASTInterfaceDecl * ast) {
        pushAst(ast);
        validateName(ast->name);
        auto type = ast->getType()->as<Type::Interface>();
        printNewline();
        // * interface ID (comment)
        printComment(STR(" --- interface " << type->name.name() << " --- id: " << type->getId()), true);
        // * interface method function pointer types
        std::vector<FieldInfo> implFields;
        type->vtable->collectFieldsOrdered(implFields);
        for (auto & it : implFields) {
            printFunctionPointerType(it.type->as<Type::Alias>());
        }
        printNewline();
        // * interface Impl struct
        printKeyword(Symbol::KwStruct);
        printSpace();
        printIdentifier(type->implStructName);
        printSpace();
        printScopeOpen();
        printFields(implFields);
        printScopeClose(true);
        // * cast to interface function
        printCastToInterfaceFunction(type);
        popAst();
    }




    void Transpiler::visit(ASTClassDecl * ast) {
        if (!ast->isDefinition) return; // forward declarations for all class types comes at the program start
        pushAst(ast);
        validateName(ast->name);
        auto * classType = ast->getType()->as<Type::Class>();
        auto * vtableType = classType->getVirtualTable();
        printComment(STR(" --- class " << ast->name.name() << " --- id:" << classType->getId()));
        printer_.newline();
        // * virtual table declaration and definition
        if (classType->isFullyDefined()) {
            std::vector<FieldInfo> vtableFields;
            vtableType->collectFieldsOrdered(vtableFields);
            for (auto & it : vtableFields) {
                printFunctionPointerType(it.type->as<Type::Alias>());
            }
            printVTableStruct(classType);
        }
        // * class declarartion
        printKeyword(Symbol::KwStruct);
        printSpace();
        printIdentifier(ast->name.name());
        printSpace();
        if (ast->isDefinition) {
            // ** class struct scope
            printScopeOpen();
            {
                // ** vtable pointer declaration
                printer_.newline();
                printType(classType->isAbstract() ? Symbol::KwVoid : vtableType->typeName);
                printSpace();
                printSymbol(Symbol::Mul);
                printSpace();
                printIdentifier(symbols::VirtualTableAsField);
                printSymbol(Symbol::Semicolon);
                // ** class fields declaration
                std::vector<FieldInfo> classFields;
                classType->collectFieldsOrdered(classFields);
                for (auto & i : classFields) {
                    printer_.newline();
                    visitChild(i.ast);
                }
            }
            printScopeClose(true);
            printNewline();
            printAllMethodsForwardDeclaration(ast, classType);

            // ** methods declaration
            for (auto & i : ast->methods) {
                if (i->isAbstract()) continue;
                printer_.newline();
                visitChild(i.get());
            }

            // ** constructor instance make declarations
            if (ast->constructors.size() == 0) {
                classConstructorIsIniting = false;
                printDefaultConstructor(classType);
                classConstructorIsIniting = true;
                printDefaultConstructor(classType);
            } else {
                classConstructorIsIniting = false;
                for (auto & i : ast->constructors) {
                    if (i->isAbstract()) continue;
                    printer_.newline();
                    visitChild(i.get());
                }
                classConstructorIsIniting = true;
                // ** constructor instance init declarations
                for (auto & i : ast->constructors) {
                    printer_.newline();
                    visitChild(i.get());
                }
            }

            if (!classType->isAbstract()) {
                // ** all implemented interface instances
                for (auto & it : classType->interfaces) {
                    printField(it.second->implStructName, getClassImplInstanceName(it.second, classType));
                    printNewline();
                }
                // ** "cast to class" function
                printCastToClassFunction(classType);
                printGetImplFunction(classType);
                // ** setup function declaration
                printNewline();
                printClassSetupFunction(classType);
            }
        }
        popAst();
    }




    void Transpiler::visit(ASTFunPtrDecl * ast) {
        pushAst(ast);
        {
            validateName(ast->name->name);
            printKeyword(Symbol::KwTypedef);
            // return type
            printSpace();
            visitChild(ast->returnType.get());
            printSpace();
            // name as pointer
            printSymbol(Symbol::ParOpen);
            printSymbol(Symbol::Mul);
            visitChild(ast->name.get());
            printSymbol(Symbol::ParClose);
            // arguments
            printSymbol(Symbol::ParOpen);
            auto i = ast->args.begin();
            if (i != ast->args.end()) {
                visitChild(i[0].get());
                while (++i != ast->args.end()) {
                    printSymbol(Symbol::Comma);
                    printSpace();
                    visitChild(i[0].get());
                }
            }
            printSymbol(Symbol::ParClose);
            printSymbol(Symbol::Semicolon);
        }
        popAst();
    }

    void Transpiler::visit(ASTIf * ast) {
        pushAst(ast);
        {
            // keyword
            printKeyword(Symbol::KwIf);
            printSpace();
            // condition
            printSymbol(Symbol::ParOpen);
            visitChild(ast->cond.get());
            printSymbol(Symbol::ParClose);
            // true case body
            visitChild(ast->trueCase.get());
            // false case body
            if (ast->falseCase.get() != nullptr) {
                printKeyword(Symbol::KwElse);
                visitChild(ast->falseCase.get());
            }
        }
        popAst();
    }

    void Transpiler::visit(ASTSwitch * ast) {
        pushAst(ast);
        {
            // keyword
            printKeyword(Symbol::KwSwitch);
            // expression/condition
            printSpace();
            printSymbol(Symbol::ParOpen);
            visitChild(ast->cond.get());
            printSymbol(Symbol::ParClose);
            // body
            printSpace();
            printSymbol(Symbol::CurlyOpen);
            printer_.indent();
            for (auto & i : ast->cases) {
                printer_.newline();
                // case keyword
                printKeyword(Symbol::KwCase);
                printSpace();
                // case constant
                printNumber(i.first);
                // case body
                printSymbol(Symbol::Colon);
                visitChild(i.second.get());
            }
            if (ast->defaultCase.get() != nullptr) {
                printer_.newline();
                printKeyword(Symbol::KwDefault);
                printSymbol(Symbol::Colon);
                visitChild(ast->defaultCase.get());
            }
            printer_.dedent();
            printer_.newline();
            printSymbol(Symbol::CurlyClose);
        }
        popAst();
    }

    void Transpiler::visit(ASTWhile * ast) {
        pushAst(ast);
        {
            printKeyword(Symbol::KwWhile);
            printSpace();
            printSymbol(Symbol::ParOpen);
            visitChild(ast->cond.get());
            printSymbol(Symbol::ParClose);
            visitChild(ast->body.get());
        }
        popAst();
    }

    void Transpiler::visit(ASTDoWhile * ast) {
        pushAst(ast);
        {
            printKeyword(Symbol::KwDo);
            visitChild(ast->body.get());
            printKeyword(Symbol::KwWhile);
            printSpace();
            printSymbol(Symbol::ParOpen);
            visitChild(ast->cond.get());
            printSymbol(Symbol::ParClose);
        }
        popAst();
    }

    void Transpiler::visit(ASTFor * ast) {
        pushAst(ast);
        {
            printKeyword(Symbol::KwFor);
            printSpace();
            printSymbol(Symbol::ParOpen);
            visitChild(ast->init.get());
            printSymbol(Symbol::Semicolon);
            visitChild(ast->cond.get());
            printSymbol(Symbol::Semicolon);
            visitChild(ast->increment.get());
            printSymbol(Symbol::ParClose);
            visitChild(ast->body.get());
        }
        popAst();
    }

    void Transpiler::visit(ASTBreak * ast) {
        printKeyword(Symbol::KwBreak);
    }

    void Transpiler::visit(ASTContinue * ast) {
        printKeyword(Symbol::KwContinue);
    }

    void Transpiler::visit(ASTReturn * ast) {
        pushAst(ast);
        {
            printKeyword(Symbol::KwReturn);
            if (ast->value) {
                printSpace();
                visitChild(ast->value.get());
            }
        }
        popAst();
    }

    void Transpiler::visit(ASTBinaryOp * ast) {
        pushAst(ast);
        {
            visitChild(ast->left.get());
            printSpace();
            printSymbol(ast->op.name());
            printSpace();
            visitChild(ast->right.get());
        }
        popAst();
    }

    void Transpiler::visit(ASTAssignment * ast) {
        auto * parentAst = peekAst()->as<ASTBlock>();
        pushAst(ast); 
        {
            visitChild(ast->lvalue.get()); 
            printSpace();
            printSymbol(ast->op.name());
            printSpace();
            visitChild(ast->value.get());
            if (parentAst != nullptr) {
                printSymbol(Symbol::Semicolon);
            }
        }
        popAst();
    }

    void Transpiler::visit(ASTUnaryOp * ast) {
        auto parentAsBlock = peekAst()->as<ASTBlock>();
        pushAst(ast);
        {
            printSymbol(ast->op.name());
            visitChild(ast->arg.get());
            if (parentAsBlock != nullptr) {
                printSymbol(Symbol::Semicolon);
            }
        }
        popAst();
    }

    void Transpiler::visit(ASTUnaryPostOp * ast) {
        pushAst(ast);
        {
            visitChild(ast->arg.get());
            printSymbol(ast->op.name());
        }
        popAst();
    }

    void Transpiler::visit(ASTAddress * ast) {
        auto interfaceType = ast->getType()->unwrap<Type::Interface>();
        if (interfaceType) {
            throw ParserError{STR("TRANS: cannot get address of interface!"), ast->location()};
        }
        pushAst(ast);
        {
            printSymbol(Symbol::BitAnd);
            visitChild(ast->target.get());
        }
        popAst();
    }

    void Transpiler::visit(ASTDeref * ast) {
        auto interfaceType = ast->getType()->unwrap<Type::Interface>();
        if (interfaceType) {
            throw ParserError{STR("TRANS: cannot dereference interface!"), ast->location()};
        }
        pushAst(ast);
        {
            printSymbol(Symbol::Mul);
            visitChild(ast->target.get());
        }
        popAst();
    }

    void Transpiler::visit(ASTIndex * ast) {
        auto interfaceType = ast->getType()->unwrap<Type::Interface>();
        if (interfaceType) {
            throw ParserError{STR("TRANS: cannot use indecies with interface!"), ast->location()};
        }
        pushAst(ast);
        visitChild(ast->base.get());
        printSymbol(Symbol::SquareOpen);
        visitChild(ast->index.get());
        printSymbol(Symbol::SquareClose);
        popAst();
    }

    void Transpiler::visit(ASTMember * ast) {
        pushAst(ast);
        if (ast->member->as<ASTCall>()) {
            visitChild(ast->member.get());
        } else {
            visitChild(ast->base.get());
            printSymbol(ast->op);
            visitChild(ast->member.get());
        }
        popAst();
    }

    void Transpiler::visit(ASTCall * ast) {
        auto * member = peekAst()->as<ASTMember>();
        pushAst(ast);
        if (member != nullptr) { // method call
            auto baseType = member->base->getType();
            // std::cout << "DEBUG: member base type is (" << baseType->toString() << ")" << std::endl;
            if (auto * classType = baseType->unwrap<Type::Class>()) {
                printClassMethodCall(member, ast, classType);
            } else if (auto * interfaceType = baseType->unwrap<Type::Interface>()) {
                printInterfaceMethodCall(member, ast, interfaceType);
            } else {
                printFunctionPointerCall(member, ast);
            }
        } else { // function or global function pointer type variable call
            if (auto * classTypeAst = ast->function->as<ASTNamedType>()) {
                if (auto * classType = types_.getType(classTypeAst->name)->as<Type::Class>()) {
                    auto constructorFuncType = ast->function->getType()->as<Type::Function>();
                    assert(constructorFuncType != nullptr);
                    auto constructorName = classType->getConstructorMakeName(constructorFuncType);
                    printIdentifier(constructorName);
                }
            } else {
                visitChild(ast->function.get());
            }
            printSymbol(Symbol::ParOpen);
            auto i = ast->args.begin();
            if (i != ast->args.end()) {
                visitChild(i[0].get());
                while (++i != ast->args.end()) {
                    printSymbol(Symbol::Comma);
                    printSpace();
                    visitChild(i[0].get());
                }
            }
            printSymbol(Symbol::ParClose);
        }
        popAst();
    }

    void Transpiler::visit(ASTCast * ast) {
        pushAst(ast);
        if (auto classCast = ast->as<ASTClassCast>()) {
            printClassCast(classCast);
        } else {
            printKeyword(Symbol::KwCast);
            printSymbol(Symbol::Lt);
            visitChild(ast->type.get());
            printSymbol(Symbol::Gt);
            printSymbol(Symbol::ParOpen);
            visitChild(ast->value.get());
            printSymbol(Symbol::ParClose);
        }
        popAst();
    }

} // namespace tinycplus
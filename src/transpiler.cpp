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
            printType(ast->getType()->getCore<Type::Class>()->toString());
            printType(Symbol::Mul);
            printSymbol(Symbol::Gt);
            printSymbol(Symbol::ParOpen);
            printIdentifier(symbols::KwThis);
            printSymbol(Symbol::ParClose);
        } else {
            printIdentifier(ast->name);
        }
    }

    void Transpiler::visit(ASTType * ast) {
        // unreachable
    }

    void Transpiler::visit(ASTPointerType * ast) {
        pushAst(ast);
        visitChild(ast->base.get());
        printSymbol(Symbol::Mul);
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
        printType(ast->name.name());
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
            if (functionAst->isMethod()) {
                auto classType = classAst->getType()->as<Type::Class>();
                if (classType != nullptr) {
                    auto methodInfo = classType->getMethodInfo(functionAst->name.value());
                    assert(methodInfo.has_value());
                    bool isInterface = methodInfo.value().isInterfaceMethod;
                    if (isInterface) {
                        printType(classType);
                        printSpace();
                        printSymbol(Symbol::Mul);
                        printSpace();
                        printIdentifier(symbols::KwThis);
                        printSpace();
                        printSymbol(Symbol::Assign);
                        printSpace();
                        printKeyword(Symbol::KwCast);
                        printSymbol(Symbol::Lt);
                        printType(classType);
                        printSymbol(Symbol::Mul);
                        printSymbol(Symbol::Gt);
                        printSymbol(Symbol::ParOpen);
                        printIdentifier(symbols::ThisInterface);
                        printSymbol(Symbol::ParClose);
                    }
                }
            } else if (functionAst->isConstructor()) {
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
                }
                // ** assigns vtable
                printVTableInstanceAssignment(classType, true);
                // ** call base class constructor init version
                if (functionAst->base.has_value()) {
                    auto & base = functionAst->base.value();
                    auto baseClassType = types_.getType(base.getName())->as<Type::Class>();
                    printIdentifier(baseClassType->initName);
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
                    printSymbol(Symbol::Comma);
                    printSpace();
                    // *** other arguments
                    if (base.args.size() > 0) {
                        printIdentifier(base.args[0]->name);
                        for (size_t i = 1; i < base.args.size(); i++) {
                            printSymbol(Symbol::Comma);
                            printIdentifier(base.args[i]->name);
                        }
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
                    printIdentifier(classType->getVirtualTable()->initName);
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
        printer_.newline();
        pushAst(ast);
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
        if (parentAst->as<ASTBlock>()
            || parentAst->as<ASTStructDecl>()
            || parentAst->as<ASTClassDecl>()
            || parentAst->as<ASTInterfaceDecl>())
        {
            printSymbol(Symbol::Semicolon);
        }
        popAst();
    }




    void Transpiler::visit(ASTFunDecl * ast) {
        if (ast->isMethod()) printMethod(ast);
        else if (ast->isConstructor()) printConstructor(ast);
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
        // validateName(ast->name);
        // auto interfaceType = ast->getType()->as<Type::Interface>();
        // printer_.newline();
        // /// TODO:
        // /// Interface ID (comment)
        // printComment(STR("ID: "), true);
        // /// Interface Impl struct
        // /// Interface View struct
        // /// Interface View make function

        // // * header
        // printKeyword(Symbol::KwStruct);
        // printSpace();
        // printIdentifier(ast->name.name());
        // // ** body begins
        // printSymbol(Symbol::CurlyOpen);
        // printer_.indent();
        // /// TODO: finish tranpilation code!
        // // ** vtable pointer declaration
        // printer_.newline();
        // printType(classType->isAbstract() ? types_.getTypeVoid() : vtableType);
        // printSymbol(Symbol::Mul);
        // printSpace();
        // printIdentifier(symbols::VTable);
        // printSymbol(Symbol::Semicolon);
        // // ** class fields declaration
        // std::vector<FieldInfo> classFields;
        // classType->collectFieldsOrdered(classFields);
        // for (auto & i : classFields) {
        //     printer_.newline();
        //     visitChild(i.ast);
        //     printSymbol(Symbol::Semicolon);
        // }
        // // ** class declaration closed
        // printer_.dedent();
        // printer_.newline();
        // printSymbol(Symbol::CurlyClose);
        // printSymbol(Symbol::Semicolon);
        // printer_.newline();
        // // ** methods declaration
        // for (auto & i : ast->methods) {
        //     if (i->isAbstract()) continue;
        //     printer_.newline();
        //     visitChild(i.get());
        // }
        // printer_.newline();
        popAst();
    }




    void Transpiler::visit(ASTClassDecl * ast) {
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
            // ** class declaration opened
            printSymbol(Symbol::CurlyOpen);
            printer_.indent();
            // ** vtable pointer declaration
            printer_.newline();
            printType(classType->isAbstract() ? Symbol::KwVoid : vtableType->typeName);
            printSpace();
            printSymbol(Symbol::Mul);
            printSpace();
            printIdentifier(symbols::VTable);
            printSymbol(Symbol::Semicolon);
            // ** class fields declaration
            std::vector<FieldInfo> classFields;
            classType->collectFieldsOrdered(classFields);
            for (auto & i : classFields) {
                printer_.newline();
                visitChild(i.ast);
            }
            // ** class declaration closed
            printer_.dedent();
            printer_.newline();
            printSymbol(Symbol::CurlyClose);
            printSymbol(Symbol::Semicolon);
            printer_.newline();
            // ** methods declaration
            for (auto & i : ast->methods) {
                if (i->isAbstract()) continue;
                printer_.newline();
                visitChild(i.get());
            }
            printer_.newline();
            if (!classType->isAbstract()) {
                printVTableInitFunction(classType);
            }
            // ** constructor instance make declarations
            if (ast->constructors.size() == 0) {
                printDefaultConstructor(classType);
            } else {
                classConstructorIsIniting = false;
                for (auto & i : ast->constructors) {
                    if (i->isAbstract()) continue;
                    printer_.newline();
                    visitChild(i.get());
                }
                // ** constructor instance init declarations
                classConstructorIsIniting = true;
                for (auto & i : ast->constructors) {
                    printer_.newline();
                    visitChild(i.get());
                }
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
            printSpace();
            visitChild(ast->value.get());
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
        pushAst(ast);
        {
            printSymbol(Symbol::BitAnd);
            visitChild(ast->target.get());
        }
        popAst();
    }

    void Transpiler::visit(ASTDeref * ast) {
        pushAst(ast);
        {
            printSymbol(Symbol::Mul);
            visitChild(ast->target.get());
        }
        popAst();
    }

    void Transpiler::visit(ASTIndex * ast) {
        pushAst(ast);
        visitChild(ast->base.get());
        printSymbol(Symbol::SquareOpen);
        visitChild(ast->index.get());
        printSymbol(Symbol::SquareClose);
        popAst();
    }

    void Transpiler::visit(ASTMember * ast) {
        pushAst(ast);
        if (ast->member->as<ASTCall>()) { // method call
            /// WARNING: function pointer is allowed only in root context,
            ///       however if it will change and data structures become allowed to use them
            ///       then a redesign must take place.
            visitChild(ast->member.get());
        } else {
            visitChild(ast->base.get());
            printSymbol(ast->op);
            visitChild(ast->member.get());
        }
        popAst();
    }

    void Transpiler::visit(ASTCall * ast) {
        auto parentBlock = peekAst()->as<ASTBlock>();
        auto * member = peekAst()->as<ASTMember>();
        pushAst(ast);
        auto * ident = ast->function->as<ASTIdentifier>();
        if (member != nullptr && !ident->getType()->isPointer()) { // method call
            auto targetType = member->base->getType();
            auto * classType = targetType->getCore<Type::Class>();
            auto methodInfo = classType->getMethodInfo(ident->name).value();
            auto * targetClassType = methodInfo.type->argType(0)->getCore<Type::Class>();
            auto baseAsIdent = member->base->as<ASTIdentifier>();
            if (methodInfo.ast->isVirtualized() && (baseAsIdent == nullptr || baseAsIdent->name != symbols::KwBase)) {
                visitChild(member->base.get());
                printSymbol(targetType->isPointer() ? Symbol::ArrowR : Symbol::Dot);
                printIdentifier(symbols::VTable);
                printSymbol(Symbol::ArrowR);
                printIdentifier(ident->name);
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
                for(auto & arg : ast->args) {
                    printSymbol(Symbol::Comma);
                    printSpace();
                    visitChild(arg.get());
                }
            }
            printSymbol(Symbol::ParClose);
        } else { // function or global function pointer type variable call
            auto * classType = ast->function->getType()->as<Type::Class>();
            if (classType != nullptr) {
                printIdentifier(classType->makeName);
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
        if (parentBlock != nullptr) {
            printSymbol(Symbol::Semicolon);
        }
        popAst();
    }

    void Transpiler::visit(ASTCast * ast) {
        pushAst(ast);
        printKeyword(Symbol::KwCast);
        printSymbol(Symbol::Lt);
        visitChild(ast->type.get());
        printSymbol(Symbol::Gt);
        printSymbol(Symbol::ParOpen);
        visitChild(ast->value.get());
        printSymbol(Symbol::ParClose);
        popAst();
    }

} // namespace tinycplus
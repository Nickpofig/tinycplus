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
        pushAst(ast);
        /// TODO: refactor code -> root is a special case that happens once - no need to check for it for all blocks
        printSymbol(Symbol::CurlyOpen);
        printer_.indent();
        if (auto * function = peekAst()->as<ASTFunDecl>();
            function != nullptr // is a function
        ) {
            if (auto method = function->as<ASTMethodDecl>()) {
                auto * classType = peekAst(1)->as<ASTClassDecl>()->getType()->as<Type::Class>();
                if (classType != nullptr) {
                    auto methodInfo = classType->getMethodInfo(method->name);
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
            } else if (function->name == symbols::Main) {
                // Program Entry must be fully declared only after all class declarations and never earlier.
                // Otherwise resulted TinyC code won't compile.
                programEntryWasDefined_ = true;
                printer_.newline();
                std::vector<Type::VTable*> vtables;
                types_.findEachVirtualTable(vtables);
                printComment(" === Initializing virtual tables === ");
                for (auto * vtable : vtables) {
                    printIdentifier(vtable->getGlobalInitFunctionName());
                    printSymbol(Symbol::ParOpen);
                    printSymbol(Symbol::ParClose);
                    printSymbol(Symbol::Semicolon);
                    printer_.newline();
                }
                printComment(" === Running the rest of the program === ");
            }
        }  
        for (auto & i : ast->body) {
            printer_.newline();
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
        for (auto & i : ast->body) {
            visitChild(i.get());
            if (!i->as<ASTFunDecl>()) {
                printSymbol(Symbol::Semicolon);
            }
            printer_.newline();
        }
        popAst();
    }

    void Transpiler::visit(ASTVarDecl * ast) {
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
        } else if (auto * complexType = ast->getType()->as<Type::Complex>();
            complexType != nullptr
            && complexType->requiresImplicitConstruction()
            && peekAst()->as<ASTSequence>()
        ) {
            printSpace();
            printSymbol(Symbol::Assign);
            printSpace();
            printIdentifier(complexType->getConstructorName());
            printSymbol(Symbol::ParOpen);
            printSymbol(Symbol::ParClose);
        }
        popAst();
    }

    void Transpiler::visit(ASTFunDecl * ast) {
        pushAst(ast);
        validateName(ast->name);
        // * function return type
        visitChild(ast->typeDecl.get());
        printSpace();
        // * function name
        printIdentifier(ast->name.name());
        registerDeclaration(ast->name.name(), ast->name.name(), 1);
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

    void Transpiler::visit(ASTStructDecl * ast) {
        pushAst(ast);
        validateName(ast->name);
        printKeyword(Symbol::KwStruct);
        printSpace();
        printIdentifier(ast->name.name());
        if (ast->isDefinition) {
            printSymbol(Symbol::CurlyOpen);
            printer_.indent();
            for (auto & i : ast->fields) {
                printer_.newline();
                visitChild(i.get());
                printSymbol(Symbol::Semicolon);
            }
            printer_.dedent();
            printer_.newline();
            printSymbol(Symbol::CurlyClose);
            printSymbol(Symbol::Semicolon);
        }
        printer_.newline();
        printComplexTypeConstructorDeclaration(ast->getType()->as<Type::Complex>());
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
        printer_.newline();
        printComment(STR(" === class " << ast->name.name() << " ==="));
        printer_.newline();
        printer_.newline();
        // * virtual table declaration and definition
        if (classType->isFullyDefined()) {
            std::vector<FieldInfo> vtableFields;
            vtableType->collectFieldsOrdered(vtableFields);
            for (auto & it : vtableFields) {
                printFunctionPointerTypeDeclaration(it.type->as<Type::Alias>());
            }
            printVTableDeclaration(classType);
        }
        // * class declarartion
        printKeyword(Symbol::KwStruct);
        printSpace();
        printIdentifier(ast->name.name());
        if (ast->isDefinition) {
            // ** class declaration opened
            printSymbol(Symbol::CurlyOpen);
            printer_.indent();
            // ** vtable pointer declaration
            printer_.newline();
            printType(classType->isAbstract() ? types_.getTypeVoid() : vtableType);
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
                printSymbol(Symbol::Semicolon);
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
                printVTableInitFunctionDeclaration(classType);
                printComplexTypeConstructorDeclaration(classType);
            }
        }
        popAst();
    }


    void Transpiler::visit(ASTMethodDecl * ast) {
        auto * classParent = peekAst()->as<ASTClassDecl>();
        assert(classParent && "must have an ast class decl as parent ast");
        pushAst(ast);
        validateName(ast->name);
        // * method return type
        visitChild(ast->typeDecl.get());
        printSpace();
        // * method name
        auto classType = classParent->getType()->as<Type::Class>();
        bool isInterfaceMethod = classType->getMethodInfo(ast->name).value().isInterfaceMethod;
        auto info = classType->getMethodInfo(ast->name).value();
        registerDeclaration(info.fullName, ast->name, 1);
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
        pushAst(ast);
        {
            visitChild(ast->lvalue.get()); 
            printSpace();
            printSymbol(ast->op.name());
            printSpace();
            visitChild(ast->value.get());
        }
        popAst();
    }

    void Transpiler::visit(ASTUnaryOp * ast) {
        pushAst(ast);
        {
            printSymbol(ast->op.name());
            visitChild(ast->arg.get());
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
            visitChild(ast->function.get());
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
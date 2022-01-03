#include <typeinfo>
#include <vector>
#include <unordered_set>

// internal
#include "transpiler.h"
#include "types.h"

namespace tinycpp {

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
            // downcasts because method belonfs to base class
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
        visitChild(ast->base.get());
        printSymbol(Symbol::Mul);
    }

    void Transpiler::visit(ASTArrayType * ast) {
        visitChild(ast->base.get());
        printSymbol(Symbol::SquareOpen);
        visitChild(ast->size.get());
        printSymbol(Symbol::SquareClose);
    }

    void Transpiler::visit(ASTNamedType * ast) {
        printType(ast->name.name());
    }

    void Transpiler::visit(ASTSequence * ast) {
        auto i = ast->body.begin();
        if (i != ast->body.end()) {
            visitChild(i[0].get()); // visits sequence element
            while (++i != ast->body.end()) {
                printSymbol(Symbol::Comma);
                printSpace();
                visitChild(i[0].get()); // visits sequence element
            }
        }
    }

    void Transpiler::visit(ASTBlock * ast) {
        if (ast->parent) { // ..when not the root context
            printSymbol(Symbol::CurlyOpen);
            printer_.indent();
        }
        for (auto & i : ast->body) {
            printer_.newline();
            visitChild(i.get());
            if (ast->parent != nullptr && !i->as<ASTFunDecl>()) {
                printSymbol(Symbol::Semicolon);
            }
        }
        if (ast->parent) { // ..when not the root context
            printer_.dedent();
            printer_.newline();
            printSymbol(Symbol::CurlyClose);
        }
        printer_.newline();
    }

    void Transpiler::visit(ASTVarDecl * ast) {
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
            && ast->parent->as<ASTSequence>()
        ) {
            printSpace();
            printSymbol(Symbol::Assign);
            printSpace();
            printIdentifier(complexType->getConstructorName());
            printSymbol(Symbol::ParOpen);
            printSymbol(Symbol::ParClose);
        }
    }

    void Transpiler::visit(ASTFunDecl * ast) {
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
    }

    void Transpiler::visit(ASTStructDecl * ast) {
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
    }

    void Transpiler::visit(ASTClassDecl * ast) {
        bool isProcessingSelf = inheritanceDepth == 0;
        auto * classType = dynamic_cast<Type::Class*>(ast->getType());
        auto * vtableType = classType->getVirtualTable();
        std::vector<std::pair<Symbol, Type::Complex::Member>> vtableMembers;
        if (isProcessingSelf) {
            printer_.newline();
            printComment(STR(" === class " << ast->name.name() << " ==="));
            printer_.newline();
            // * virtual table declaration and definition
            if (vtableType != nullptr && classType->hasOwnVirtualTable() && classType->isFullyDefined()) {
                vtableType->collectMembersOrdered(vtableMembers);
                // ** function pointer types
                for (auto & it : vtableMembers) {
                    if (it.second.ast->as<ASTMethodDecl>()->parent != ast) continue;
                    auto * funPtrType = it.second.type->as<Type::Alias>();
                    auto * functionType = funPtrType->base()->getCore<Type::Function>();
                    printKeyword(Symbol::KwTypedef);
                    // *** return type declaration
                    printSpace();
                    printType(functionType->returnType());
                    // *** name as pointer
                    printSpace();
                    printSymbol(Symbol::ParOpen);
                    printSymbol(Symbol::Mul);
                    printType(funPtrType);
                    printSymbol(Symbol::ParClose);
                    // *** arguments
                    printSymbol(Symbol::ParOpen);
                    for (auto i = 0; i < functionType->numArgs(); i++) {
                        if (i > 0) {
                            printSymbol(Symbol::Comma);
                            printSpace();
                        }
                        printType(functionType->argType(i));
                    }
                    printSymbol(Symbol::ParClose);
                    printSymbol(Symbol::Semicolon);
                    printer_.newline();
                }
                printer_.newline();
                // ** vtable struct
                printKeyword(Symbol::KwStruct);
                printSpace();
                // *** struct name
                printIdentifier(vtableType->toString());
                printSpace();
                // *** function pointers
                printSymbol(Symbol::CurlyOpen);
                printer_.indent();
                printer_.newline();
                auto remained = vtableMembers.size();
                for (auto & member : vtableMembers) {
                    printType(member.second.type);
                    printSpace();
                    printIdentifier(member.first);
                    printSymbol(Symbol::Semicolon);
                    if (--remained > 0) {
                        printer_.newline();
                    }
                }
                printer_.dedent();
                printer_.newline();
                printSymbol(Symbol::CurlyClose);
                printSymbol(Symbol::Semicolon);
                printer_.newline();
                printer_.newline();
                printType(vtableType);
                printSpace();
                printIdentifier(vtableType->getGlobalInstanceName());
                printSymbol(Symbol::Semicolon);
                printer_.newline();
                printer_.newline();
            }
            // * class declarartion
            printKeyword(Symbol::KwStruct);
            printSpace();
            printIdentifier(ast->name.name());
        }
        if (ast->isDefinition) {
            // * class fields
            if (isProcessingSelf) {
                printSymbol(Symbol::CurlyOpen);
                printer_.indent();
                // ** pointer to vtable
                printer_.newline();
                printType(vtableType != nullptr ? vtableType : types_.getTypeVoid());
                printSymbol(Symbol::Mul);
                printSpace();
                printIdentifier(symbols::VTable);
                printSymbol(Symbol::Semicolon);
            }
            // ** base class fields
            if (ast->baseClass) {
                inheritanceDepth++; 
                auto * baseClassType = dynamic_cast<Type::Class *>(ast->baseClass->getType());
                visit(baseClassType->ast());
                inheritanceDepth--;
            }
            // ** this class fields
            for (auto & i : ast->fields) {
                printer_.newline();
                visitChild(i.get());
                printSymbol(Symbol::Semicolon);
            }
            // ** this class methods
            if (isProcessingSelf) {
                printer_.dedent();
                printer_.newline();
                printSymbol(Symbol::CurlyClose);
                printSymbol(Symbol::Semicolon);
                printer_.newline();
                // *** function declaration
                for (auto & i : ast->methods) {
                    printer_.newline();
                    visitChild(i.get());
                }
                printer_.newline();
                printVTableInitFunctionDeclaration(classType);
                printComplexTypeConstructorDeclaration(classType);
            }
        }
    }


    void Transpiler::visit(ASTMethodDecl * ast) {
        auto * classParent = ast->findParent<ASTClassDecl>();
        assert(classParent && "must have an ast class decl as parent ast");
        // * method return type
        visitChild(ast->typeDecl.get());
        printSpace();
        // * method name
        auto classType = classParent->getType()->as<Type::Class>();
        auto info = classType->getMethodInfo(ast->name);
        registerDeclaration(info.fullName, ast->name, 1);
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
    }

    void Transpiler::visit(ASTFunPtrDecl * ast) {
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

    void Transpiler::visit(ASTIf * ast) {
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

    void Transpiler::visit(ASTSwitch * ast) {
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

    void Transpiler::visit(ASTWhile * ast) {
        printKeyword(Symbol::KwWhile);
        printSpace();
        printSymbol(Symbol::ParOpen);
        visitChild(ast->cond.get());
        printSymbol(Symbol::ParClose);
        visitChild(ast->body.get());
    }

    void Transpiler::visit(ASTDoWhile * ast) {
        printKeyword(Symbol::KwDo);
        visitChild(ast->body.get());
        printKeyword(Symbol::KwWhile);
        printSpace();
        printSymbol(Symbol::ParOpen);
        visitChild(ast->cond.get());
        printSymbol(Symbol::ParClose);
    }

    void Transpiler::visit(ASTFor * ast) {
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

    void Transpiler::visit(ASTBreak * ast) {
        printKeyword(Symbol::KwBreak);
    }

    void Transpiler::visit(ASTContinue * ast) {
        printKeyword(Symbol::KwContinue);
    }

    void Transpiler::visit(ASTReturn * ast) {
        printKeyword(Symbol::KwReturn);
        printSpace();
        visitChild(ast->value.get());
    }

    void Transpiler::visit(ASTBinaryOp * ast) {
        visitChild(ast->left.get());
        printSpace();
        printSymbol(ast->op.name());
        printSpace();
        visitChild(ast->right.get());
    }

    void Transpiler::visit(ASTAssignment * ast) {
        visitChild(ast->lvalue.get()); 
        printSpace();
        printSymbol(ast->op.name());
        printSpace();
        visitChild(ast->value.get());
    }

    void Transpiler::visit(ASTUnaryOp * ast) {
        printSymbol(ast->op.name());
        visitChild(ast->arg.get());
    }

    void Transpiler::visit(ASTUnaryPostOp * ast) {
        visitChild(ast->arg.get());
        printSymbol(ast->op.name());
    }

    void Transpiler::visit(ASTAddress * ast) {
        printSymbol(Symbol::BitAnd);
        visitChild(ast->target.get());
    }

    void Transpiler::visit(ASTDeref * ast) {
        printSymbol(Symbol::Mul);
        visitChild(ast->target.get());
    }

    void Transpiler::visit(ASTIndex * ast) {
        visitChild(ast->base.get());
        printSymbol(Symbol::SquareOpen);
        visitChild(ast->index.get());
        printSymbol(Symbol::SquareClose);
    }

    void Transpiler::visit(ASTMember * ast) {
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
    }

    void Transpiler::visit(ASTCall * ast) {
        auto * ident = ast->function->as<ASTIdentifier>();
        auto * member = ast->parent->as<ASTMember>();
        if (member != nullptr && !ident->getType()->isPointer()) { // method call
            auto targetType = member->base->getType();
            auto * classType = targetType->getCore<Type::Class>();
            auto methodInfo = classType->getMethodInfo(ident->name);
            auto * targetClassType = methodInfo.targetClassType;
            auto baseAsIdent = member->base->as<ASTIdentifier>();
            if (methodInfo.ast->isVirtual() && (baseAsIdent == nullptr || baseAsIdent->name != symbols::KwBase)) {
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
                    // downcasts because method belonfs to base class
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
    }

    void Transpiler::visit(ASTCast * ast) {
        printKeyword(Symbol::KwCast);
        printSymbol(Symbol::Lt);
        visitChild(ast->type.get());
        printSymbol(Symbol::Gt);
        printSymbol(Symbol::ParOpen);
        visitChild(ast->value.get());
        printSymbol(Symbol::ParClose);
    }

} // namespace tinycpp
#include <typeinfo>

// internal
#include "transpiler.h"
#include "types.h"

namespace tinycpp {

    /// TODO: enforce use of "this" keyword or somehow define that a variable exist in a context of a transpiled tinyc code

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
        printIdentifier(ast->name);
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
        if (ast->parentAST) { // ..when not the root context
            printSymbol(Symbol::CurlyOpen);
            printer_.indent();
        }
        for (auto & i : ast->body) {
            printer_.newline();
            visitChild(i.get());
            if (ast->parentAST != nullptr && !dynamic_cast<ASTFunDecl *>(i.get())) {
                printSymbol(Symbol::Semicolon);
            }
        }
        if (ast->parentAST) { // ..when not the root context
            printer_.dedent();
            printer_.newline();
            printSymbol(Symbol::CurlyClose);
        }
        printer_.newline();
    }

    void Transpiler::visit(ASTVarDecl * ast) {
        if (auto arrayType = dynamic_cast<ASTArrayType*>(ast->type.get())) {
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
    }

    void Transpiler::visit(ASTFunDecl * ast) {
        auto * classParent = ast->findParent<ASTClassDecl>();
        // * method return type
        visitChild(ast->typeDecl.get());
        printSpace();
        // * method name
        if (classParent) { // ..inserts class name at the beginning
            printClassPrefix(classParent->getType());
        }
        printIdentifier(ast->name.name());
        // * method arguments
        printSymbol(Symbol::ParOpen);
        if (classParent) { // ..inserts pointer to the owner class as the first argument
            printType(classParent->name.name());
            printSymbol(Symbol::Mul);
            printSpace();
            printIdentifier(symbols::KwThis);
            if (ast->args.size() > 0) {
                printSymbol(Symbol::Colon);
                printSpace();
            }
        }
        auto arg = ast->args.begin();
        if (arg != ast->args.end()) {
            visitChild(arg[0].get());
            while (++arg != ast->args.end()) {
                printSymbol(Symbol::Colon);
                printSpace();
                visitChild(arg[0].get());
            }
        }
        printSymbol(Symbol::ParClose);
        // * method body
        visitChild(ast->body.get());
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
    }

    void Transpiler::visit(ASTClassDecl * ast) {
        bool isProcessingSelf = inheritanceDepth == 0;
        if (isProcessingSelf) {
            printKeyword(Symbol::KwStruct);
            printSpace();
            printIdentifier(ast->name.name());
        }
        if (ast->isDefinition) {
            // * class fields
            if (isProcessingSelf) {
                printSymbol(Symbol::CurlyOpen);
                printer_.indent();
            }
            if (ast->baseClass) {
                inheritanceDepth++; 
                auto * baseClassType = dynamic_cast<Type::Class *>(ast->baseClass->getType());
                visit(baseClassType->ast());
                inheritanceDepth--;
            }
            for (auto & i : ast->fields) {
                printer_.newline();
                visitChild(i.get());
                printSymbol(Symbol::Semicolon);
            }
            if (isProcessingSelf) {
                printer_.dedent();
                printer_.newline();
                printSymbol(Symbol::CurlyClose);
                printSymbol(Symbol::Semicolon);
                // * class methods
                for (auto & i : ast->methods) {
                    printer_.newline();
                    visitChild(i.get());
                }
            }
        }
    }

    void Transpiler::visit(ASTFunPtrDecl * ast) {
        printKeyword(Symbol::KwTypedef);
        // return type
        printSpace();
        visitChild(ast->returnType.get());
        // name as pointer
        printSymbol(Symbol::ParOpen);
        printSpace();
        printSymbol(Symbol::Mul);
        visitChild(ast->name.get());
        printSymbol(Symbol::ParClose);
        // arguments
        printSymbol(Symbol::ParOpen);
        auto i = ast->args.begin();
        if (i != ast->args.end()) {
            visitChild(i[0].get());
            while (++i != ast->args.end())
                printSymbol(Symbol::Colon);
                printSpace();
                visitChild(i[0].get());
        }
        printSymbol(Symbol::ParClose);
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
        if (auto call = dynamic_cast<ASTCall*>(ast->member.get())) { // method call
            /// WARNING: function pointer is allowed only in root context,
            ///       however if it will change and data structures become allowed to use them
            ///       then a redesign must take place.
            visitChild(ast->member.get());
        } else { // variable access
            visitChild(ast->base.get());
            printSymbol(ast->op.name());
            visitChild(ast->member.get());
        }
    }

    void Transpiler::visit(ASTCall * ast) {
        auto * methodType = dynamic_cast<Type::Method*>(ast->function->getType());
        if (methodType) {
            printClassPrefix(methodType->classType);
        }
        visitChild(ast->function.get());
        printSymbol(Symbol::ParOpen);
        if (methodType) { // ..moves method call target to a position of functions's first argument
            auto * member = ast->findParent<ASTMember>();
            if (member->op == Symbol::Dot) {
                printSymbol(Symbol::BitAnd);
            }
            auto * baseType = member->base->getType();
            bool castIsRequired = baseType->getCore<Type::Complex>() != methodType->classType;
            if (castIsRequired) {
                printKeyword(Symbol::KwCast);
                printSymbol(Symbol::Lt);
                printType(methodType->classType->toString());
                printType(Symbol::Mul);
                printSymbol(Symbol::Gt);
                printSymbol(Symbol::ParOpen);
            }
            visitChild(member->base.get());
            if (castIsRequired) {
                printSymbol(Symbol::ParClose);
            }
            if (ast->args.size() > 0) { 
                printSymbol(Symbol::Colon);
                printSpace();
            }
        }
        auto i = ast->args.begin();
        if (i != ast->args.end()) {
            visitChild(i[0].get());
            while (++i != ast->args.end()) {
                printSymbol(Symbol::Colon);
                printSpace();
                visitChild(i[0].get());
            }
        }
        printSymbol(Symbol::ParClose);
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
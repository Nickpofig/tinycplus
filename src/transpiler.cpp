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
        printer_ << '\'' << ast->value << '\'';
    }

    void Transpiler::visit(ASTString * ast) {
        if (isPrintColorful_) printer_ << printer_.stringLiteral;
        printer_ << '\"' << ast->value << '\"';
    }

    void Transpiler::visit(ASTIdentifier * ast) {
        if (isPrintColorful_) printer_ << printer_.identifier;
        printer_ << ast->name.name();
    }

    void Transpiler::visit(ASTType * ast) {
        // unreachable
    }

    void Transpiler::visit(ASTPointerType * ast) {
        visitChild(ast->base.get());
        printSymbols("*");
    }

    void Transpiler::visit(ASTArrayType * ast) {
        visitChild(ast->base.get());
        printSymbols("[");
        visitChild(ast->size.get());
        printSymbols("]");
    }

    void Transpiler::visit(ASTNamedType * ast) {
        printType(ast->name.name());
    }

    void Transpiler::visit(ASTSequence * ast) {
        auto i = ast->body.begin();
        if (i != ast->body.end()) {
            visitChild(i[0].get()); // visits sequence element
            while (++i != ast->body.end()) {
                printSymbols(", ");
                visitChild(i[0].get()); // visits sequence element
            }
        }
    }

    void Transpiler::visit(ASTBlock * ast) {
        if (ast->parentAST) { // ..when not the root context
            printSymbols("{");
            printer_.indent();
        }
        for (auto & i : ast->body) {
            printer_.newline();
            visitChild(i.get());
            if (ast->parentAST != nullptr && !dynamic_cast<ASTFunDecl *>(i.get())) {
                printSymbols(";");
            }
        }
        if (ast->parentAST) { // ..when not the root context
            printer_.dedent();
            printer_.newline();
            printSymbols("}");
        }
        printer_.newline();
    }

    void Transpiler::visit(ASTVarDecl * ast) {
        if (auto arrayType = dynamic_cast<ASTArrayType*>(ast->type.get())) {
            // base type part
            visitChild(arrayType->base.get());
            printer_ << " ";
            // variable name
            visitChild(ast->name.get());
            // array type part
            printSymbols("[");
            visitChild(arrayType->size.get());
            printSymbols("]");
        } else {
            // base type part
            visitChild(ast->type.get());
            printer_ << " ";
            // variable name
            visitChild(ast->name.get());
        }
        // immediate value assignment
        if (ast->value.get() != nullptr) {
            printSymbols(" = ");
            visitChild(ast->value.get());
        }
    }

    void Transpiler::visit(ASTFunDecl * ast) {
        auto * classParent = ast->findParent<ASTClassDecl>();
        // * method return type
        visitChild(ast->typeDecl.get());
        printer_ << " ";
        // * method name
        if (classParent) { // ..inserts class name at the beginning
            printClassPrefix(classParent->getType(), printer_.identifier);
        }
        printIdentifier(ast->name.name());
        // * method arguments
        printSymbols("(");
        if (classParent) { // ..inserts pointer to the owner class as the first argument
            printType(classParent->name.name());
            printSymbols("* ");
            printIdentifier("this");
            if (ast->args.size() > 0) {
                printSymbols(", ");
            }
        }
        auto arg = ast->args.begin();
        if (arg != ast->args.end()) {
            visitChild(arg[0].get());
            while (++arg != ast->args.end()) {
                printSymbols(", ");
                visitChild(arg[0].get());
            }
        }
        printSymbols(")");
        // * method body
        visitChild(ast->body.get());
    }

    void Transpiler::visit(ASTStructDecl * ast) {
        printKeyword("struct ");
        printIdentifier(ast->name.name());
        if (ast->isDefinition) {
            printSymbols("{");
            printer_.indent();
            for (auto & i : ast->fields) {
                printer_.newline();
                visitChild(i.get());
                printSymbols(";");
            }
            printer_.dedent();
            printer_.newline();
            printSymbols("};");
        }
    }

    void Transpiler::visit(ASTClassDecl * ast) {
        bool isProcessingSelf = inheritanceDepth == 0;
        if (isProcessingSelf) {
            printKeyword("struct ");
            printIdentifier(ast->name.name());
        }
        if (ast->isDefinition) {
            // * class fields
            if (isProcessingSelf) {
                printSymbols("{");
                printer_.indent();
            }
            if (ast->baseClass) {
                inheritanceDepth++;
                // ASTPrettyPrinter dbg {std::cerr};
                // dbg.newline();
                // dbg << "[Debug] base class type is: " << 
                auto * baseClassType = dynamic_cast<Type::Class *>(ast->baseClass->getType());
                visit(baseClassType->ast());
                inheritanceDepth--;
            }
            for (auto & i : ast->fields) {
                printer_.newline();
                visitChild(i.get());
                printSymbols(";");
            }
            if (isProcessingSelf) {
                printer_.dedent();
                printer_.newline();
                printSymbols("};");
                // * class methods
                for (auto & i : ast->methods) {
                    printer_.newline();
                    visitChild(i.get());
                }
            }
        }
    }

    void Transpiler::visit(ASTFunPtrDecl * ast) {
        printKeyword("typedef ");
        visitChild(ast->returnType.get());
        printSymbols("( *");
        visitChild(ast->name.get());
        printSymbols(")(");
        auto i = ast->args.begin();
        if (i != ast->args.end()) {
            visitChild(i[0].get());
            while (++i != ast->args.end())
                printSymbols(", ");
                visitChild(i[0].get());
        }
        printSymbols(")");
    }

    void Transpiler::visit(ASTIf * ast) {
        printKeyword("if ");
        printSymbols("(");
        visitChild(ast->cond.get());
        printSymbols(")");
        visitChild(ast->trueCase.get());
        if (ast->falseCase.get() != nullptr) {
            printKeyword("else");
            visitChild(ast->falseCase.get());
        }
    }

    void Transpiler::visit(ASTSwitch * ast) {
        printKeyword("switch ");
        printSymbols("(");
        visitChild(ast->cond.get());
        printSymbols(") {");
        printer_.indent();
        for (auto & i : ast->cases) {
            printer_.newline();
            printKeyword("case ");
            printNumber(i.first);
            printSymbols(":");
            visitChild(i.second.get());
        }
        if (ast->defaultCase.get() != nullptr) {
            printer_.newline();
            printKeyword("default");
            printSymbols(":");
            visitChild(ast->defaultCase.get());
        }
        printer_.dedent();
        printer_.newline();
        printSymbols("}");
    }

    void Transpiler::visit(ASTWhile * ast) {
        printKeyword("while ");
        printSymbols("(");
        visitChild(ast->cond.get());
        printSymbols(")");
        visitChild(ast->body.get());
    }

    void Transpiler::visit(ASTDoWhile * ast) {
        printKeyword("do");
        visitChild(ast->body.get());
        printKeyword("while ");
        printSymbols("(");
        visitChild(ast->cond.get());
        printSymbols(")");
    }

    void Transpiler::visit(ASTFor * ast) {
        printKeyword("for ");
        printSymbols("(");
        visitChild(ast->init.get());
        printSymbols(";");
        visitChild(ast->cond.get());
        printSymbols(";");
        visitChild(ast->increment.get());
        printSymbols(")");
        visitChild(ast->body.get());
    }

    void Transpiler::visit(ASTBreak * ast) {
        printKeyword("break");
    }

    void Transpiler::visit(ASTContinue * ast) {
        printKeyword("continue");
    }

    void Transpiler::visit(ASTReturn * ast) {
        printKeyword("return ");
        visitChild(ast->value.get());
    }

    void Transpiler::visit(ASTBinaryOp * ast) {
        visitChild(ast->left.get());
        printer_ << " ";
        printSymbols(ast->op.name());
        printer_ << " ";
        visitChild(ast->right.get());
    }

    void Transpiler::visit(ASTAssignment * ast) {
        visitChild(ast->lvalue.get()); 
        printer_ << " ";
        printSymbols(ast->op.name());
        printer_ << " ";
        visitChild(ast->value.get());
    }

    void Transpiler::visit(ASTUnaryOp * ast) {
        printSymbols(ast->op.name());
        visitChild(ast->arg.get());
    }

    void Transpiler::visit(ASTUnaryPostOp * ast) {
        visitChild(ast->arg.get());
        printSymbols(ast->op.name());
    }

    void Transpiler::visit(ASTAddress * ast) {
        printSymbols("&");
        visitChild(ast->target.get());
    }

    void Transpiler::visit(ASTDeref * ast) {
        printSymbols("*");
        visitChild(ast->target.get());
    }

    void Transpiler::visit(ASTIndex * ast) {
        visitChild(ast->base.get());
        printSymbols("[");
        visitChild(ast->index.get());
        printSymbols("]");
    }

    void Transpiler::visit(ASTMember * ast) {
        if (auto call = dynamic_cast<ASTCall*>(ast->member.get())) { // method call
            /// WARNING: function pointer is allowed only in root context,
            ///       however if it will change and data structures become allowed to use them
            ///       then a redesign must take place.
            visitChild(ast->member.get());
        } else { // variable access
            visitChild(ast->base.get());
            printSymbols(ast->op.name());
            visitChild(ast->member.get());
        }
    }

    void Transpiler::visit(ASTCall * ast) {
        auto * methodType = dynamic_cast<Type::Method*>(ast->function->getType());
        if (methodType) {
            printClassPrefix(methodType->classType, printer_.identifier);
        }
        visitChild(ast->function.get());
        printSymbols("(");
        if (methodType) { // ..moves method call target to a position of functions's first argument
            auto * member = ast->findParent<ASTMember>();
            if (member->op == Symbol::Dot) {
                printSymbols("&");
            }
            /// TODO: check result type of the base to be the exact host type of the member call
            visitChild(member->base.get());
            if (ast->args.size() > 0) printSymbols(", ");
        }
        auto i = ast->args.begin();
        if (i != ast->args.end()) {
            visitChild(i[0].get());
            while (++i != ast->args.end()) {
                printSymbols(", ");
                visitChild(i[0].get());
            }
        }
        printSymbols(")");
    }

    void Transpiler::visit(ASTCast * ast) {
        printKeyword("cast");
        printSymbols("<");
        visitChild(ast->type.get());
        printSymbols(">(");
        visitChild(ast->value.get());
        printSymbols(")");
    }

} // namespace tinycpp
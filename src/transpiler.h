#pragma once

#include <iostream>
#include <vector>

#include "ast.h"

namespace tinycpp {

    class TranspilerASTVisiter : public ASTVisitor {
    private: // ASTScope: helper to access parent ast.
        std::vector<AST *> _currentASTStack;
        class ASTScope {
        private:
            TranspilerASTVisiter * transpiler;
        public:
            AST const * parentAST;
            ASTScope(TranspilerASTVisiter * transpiler, AST * ast): transpiler{transpiler} { 
                parentAST = transpiler->_currentASTStack.size() > 0 
                    ? transpiler->_currentASTStack.back()
                    : nullptr;
                transpiler->_currentASTStack.push_back(ast);
            }
            ~ASTScope() {
                transpiler->_currentASTStack.pop_back();
            }
            template<class T>
            T * findParent() {
                for (auto it = transpiler->_currentASTStack.rbegin() + 1; it != transpiler->_currentASTStack.rend(); it++) {
                    if (auto * parent = dynamic_cast<T*>(it[0])) {
                        return parent;
                    }
                }
                return nullptr;
            }
        };
        ASTScope track(AST * ast) {
            return ASTScope{this, ast};
        }
    private: // persistant data
        ASTPrettyPrinter printer_;
        bool isPrintColorful_ = false;
    public:
        TranspilerASTVisiter(std::ostream & output, bool isColorful)
            :printer_{output}
            ,isPrintColorful_{isColorful}
        { }
    private:
        void printSymbols(std::string const & text) {
            if (isPrintColorful_) printer_ << printer_.symbol;
            printer_ << text;
        }
        void printIdentifier(std::string const & text) {
            if (isPrintColorful_) printer_ << printer_.identifier;
            printer_ << text;
        }
        void printType(std::string const & text) {
            if (isPrintColorful_) printer_ << printer_.type;
            printer_ << text;
        }
        void printKeyword(std::string const & text) {
            if (isPrintColorful_) printer_ << printer_.keyword;
            printer_ << text;
        }
        void printNumber(int64_t value) {
            if (isPrintColorful_) printer_ << printer_.numberLiteral;
            printer_ << value;
        }
    public:
        void visit(AST * ast) override {
            if (ast == nullptr) {
                return;
            } else if (auto exactAst = dynamic_cast<ASTInteger*>(ast)) {
                visit(exactAst);
            } else if (auto exactAst = dynamic_cast<ASTInteger*>(ast)) {
                visit(exactAst);
            } else if (auto exactAst = dynamic_cast<ASTDouble*>(ast)) {
                visit(exactAst);
            } else if (auto exactAst = dynamic_cast<ASTChar*>(ast)) {
                visit(exactAst);
            } else if (auto exactAst = dynamic_cast<ASTIdentifier*>(ast)) {
                visit(exactAst);
            } else if (auto exactAst = dynamic_cast<ASTType*>(ast)) {
                visit(exactAst);
            } else if (auto exactAst = dynamic_cast<ASTBlock*>(ast)) {
                visit(exactAst);
            } else if (auto exactAst = dynamic_cast<ASTSequence*>(ast)) {
                visit(exactAst);
            } else if (auto exactAst = dynamic_cast<ASTVarDecl*>(ast)) {
                visit(exactAst);
            } else if (auto exactAst = dynamic_cast<ASTFunDecl*>(ast)) {
                visit(exactAst);
            } else if (auto exactAst = dynamic_cast<ASTStructDecl*>(ast)) {
                visit(exactAst);
            } else if (auto exactAst = dynamic_cast<ASTClassDecl*>(ast)) {
                visit(exactAst);
            } else if (auto exactAst = dynamic_cast<ASTFunPtrDecl*>(ast)) {
                visit(exactAst);
            } else if (auto exactAst = dynamic_cast<ASTIf*>(ast)) {
                visit(exactAst);
            } else if (auto exactAst = dynamic_cast<ASTSwitch*>(ast)) {
                visit(exactAst);
            } else if (auto exactAst = dynamic_cast<ASTWhile*>(ast)) {
                visit(exactAst);
            } else if (auto exactAst = dynamic_cast<ASTDoWhile*>(ast)) {
                visit(exactAst);
            } else if (auto exactAst = dynamic_cast<ASTFor*>(ast)) {
                visit(exactAst);
            } else if (auto exactAst = dynamic_cast<ASTBreak*>(ast)) {
                visit(exactAst);
            } else if (auto exactAst = dynamic_cast<ASTContinue*>(ast)) {
                visit(exactAst);
            } else if (auto exactAst = dynamic_cast<ASTReturn*>(ast)) {
                visit(exactAst);
            } else if (auto exactAst = dynamic_cast<ASTBinaryOp*>(ast)) {
                visit(exactAst);
            } else if (auto exactAst = dynamic_cast<ASTAssignment*>(ast)) {
                visit(exactAst);
            } else if (auto exactAst = dynamic_cast<ASTUnaryOp*>(ast)) {
                visit(exactAst);
            } else if (auto exactAst = dynamic_cast<ASTUnaryPostOp*>(ast)) {
                visit(exactAst);
            } else if (auto exactAst = dynamic_cast<ASTAddress*>(ast)) {
                visit(exactAst);
            } else if (auto exactAst = dynamic_cast<ASTDeref*>(ast)) {
                visit(exactAst);
            } else if (auto exactAst = dynamic_cast<ASTIndex*>(ast)) {
                visit(exactAst);
            } else if (auto exactAst = dynamic_cast<ASTMember*>(ast)) {
                visit(exactAst);
            } else if (auto exactAst = dynamic_cast<ASTMemberPtr*>(ast)) {
                visit(exactAst);
            } else if (auto exactAst = dynamic_cast<ASTCall*>(ast)) {
                visit(exactAst);
            } else if (auto exactAst = dynamic_cast<ASTCast*>(ast)) {
                visit(exactAst);
            }
        }
        void visit(ASTInteger * ast) override {
            printNumber(ast->value);
        }
        void visit(ASTDouble * ast) override {
            printNumber(ast->value);
        }
        void visit(ASTChar * ast) override {
            if (isPrintColorful_) printer_ << printer_.charLiteral;
            printer_ << '\'' << ast->value << '\'';
        }
        void visit(ASTString * ast) override {
            if (isPrintColorful_) printer_ << printer_.stringLiteral;
            printer_ << '\"' << ast->value << '\"';
        }
        void visit(ASTIdentifier * ast) override {
            if (isPrintColorful_) printer_ << printer_.identifier;
            printer_ << ast->name.name();
        }
        void visit(ASTType * ast) override {
            if (auto exactAst = dynamic_cast<ASTPointerType*>(ast)) {
                visit(exactAst);
            } else if (auto exactAst = dynamic_cast<ASTArrayType*>(ast)) {
                visit(exactAst);
            } else if (auto exactAst = dynamic_cast<ASTNamedType*>(ast)) {
                visit(exactAst);
            } 
        }
        void visit(ASTPointerType * ast) override {
            auto scope = track(ast);
            visit(ast->base.get());
            printSymbols("*");
        }
        void visit(ASTArrayType * ast) override {
            auto scope = track(ast);
            visit(ast->base.get());
            printSymbols("[");
            visit(ast->size.get());
            printSymbols("]");
        }
        void visit(ASTNamedType * ast) override {
            printType(ast->name.name());
        }
        void visit(ASTSequence * ast) override {
            auto scope = track(ast);
            auto i = ast->body.begin();
            if (i != ast->body.end()) {
                visit(i[0].get()); // visits sequence element
                while (++i != ast->body.end()) {
                    printSymbols(", ");
                    visit(i[0].get()); // visits sequence element
                }
            }
        }
        void visit(ASTBlock * ast) override {
            auto scope = track(ast);
            auto * parent = scope.parentAST;
            if (parent) { // ..when not the root scope
                printSymbols("{");
                printer_.indent();
            }
            for (auto & i : ast->body) {
                printer_.newline();
                visit(i.get());
            }
            if (parent) { // ..when not the root scope
                printer_.dedent();
                printer_.newline();
                printSymbols("}");
            }
            printer_.newline();
        }
        void visit(ASTVarDecl * ast) override {
            auto scope = track(ast);
            auto const * parent = scope.parentAST;
            if (auto arrayType = dynamic_cast<ASTArrayType*>(ast->type.get())) {
                // base type part
                visit(arrayType->base.get());
                printer_ << " ";
                // variable name
                visit(ast->name.get());
                // array part
                printSymbols("[");
                visit(arrayType->size.get());
                printSymbols("]");
            } else {
                visit(ast->type.get());
                printer_ << " ";
                visit(ast->name.get());
            }
            // adds semicolon when it is a statement
            if (dynamic_cast<ASTBlock const *>(parent)) {
                printSymbols(";");
            }
        }
        void visit(ASTFunDecl * ast) override {
            auto scope = track(ast);
            auto * classParent = scope.findParent<ASTClassDecl>();
            // * method return type
            visit(ast->typeDecl.get());
            printer_ << " ";
            // * method name
            if (classParent) { // ..inserts class name at the beginning
                printIdentifier(classParent->name.name());
                printIdentifier("_");
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
                visit(arg[0].get());
                while (++arg != ast->args.end()) {
                    printSymbols(", ");
                    visit(arg[0].get());
                }
            }
            printSymbols(")");
            // * method body
            visit(ast->body.get());
        }
        void visit(ASTStructDecl * ast) override {
            auto scope = track(ast);
            printKeyword("struct ");
            printIdentifier(ast->name.name());
            if (ast->isDefinition) {
                printSymbols("{");
                printer_.indent();
                for (auto & i : ast->fields) {
                    printer_.newline();
                    visit(i.get());
                    printSymbols(";");
                }
                printer_.dedent();
                printer_.newline();
                printSymbols("};");
            }
        }
        void visit(ASTClassDecl * ast) override {
            auto scope = track(ast);
            printKeyword("struct ");
            printIdentifier(ast->name.name());
            if (ast->isDefinition) {
                // * class fields
                printSymbols("{");
                printer_.indent();
                for (auto & i : ast->fields) {
                    printer_.newline();
                    visit(i.get());
                    printSymbols(";");
                }
                printer_.dedent();
                printer_.newline();
                printSymbols("};");
                // * class methods
                for (auto & i : ast->methods) {
                    printer_.newline();
                    visit(i.get());
                }
            }
        }
        void visit(ASTFunPtrDecl * ast) override {
            auto scope = track(ast);
            printKeyword("typedef ");
            visit(ast->returnType.get());
            printSymbols("( *");
            visit(ast->name.get());
            printSymbols(")(");
            auto i = ast->args.begin();
            if (i != ast->args.end()) {
                visit(i[0].get());
                while (++i != ast->args.end())
                    printSymbols(", ");
                    visit(i[0].get());
            }
            printSymbols(")");
        }
        void visit(ASTIf * ast) override {
            auto scope = track(ast);
            printKeyword("if ");
            printSymbols("(");
            visit(ast->cond.get());
            printSymbols(")");
            visit(ast->trueCase.get());
            if (ast->falseCase.get() != nullptr) {
                printKeyword("else");
                visit(ast->falseCase.get());
            }
        }
        void visit(ASTSwitch * ast) override {
            auto scope = track(ast);
            printKeyword("switch ");
            printSymbols("(");
            visit(ast->cond.get());
            printSymbols(") {");
            printer_.indent();
            for (auto & i : ast->cases) {
                printer_.newline();
                printKeyword("case ");
                printNumber(i.first);
                printSymbols(":");
                visit(i.second.get());
            }
            if (ast->defaultCase.get() != nullptr) {
                printer_.newline();
                printKeyword("default");
                printSymbols(":");
                visit(ast->defaultCase.get());
            }
            printer_.dedent();
            printer_.newline();
            printSymbols("}");
        }
        void visit(ASTWhile * ast) override {
            auto scope = track(ast);
            printKeyword("while ");
            printSymbols("(");
            visit(ast->cond.get());
            printSymbols(")");
            visit(ast->body.get());
        }
        void visit(ASTDoWhile * ast) override {
            auto scope = track(ast);
            printKeyword("do");
            visit(ast->body.get());
            printKeyword("while ");
            printSymbols("(");
            visit(ast->cond.get());
            printSymbols(")");
        }
        void visit(ASTFor * ast) override {
            auto scope = track(ast);
            printKeyword("for ");
            printSymbols("(");
            visit(ast->init.get());
            printSymbols(";");
            visit(ast->cond.get());
            printSymbols(";");
            visit(ast->increment.get());
            printSymbols(")");
            visit(ast->body.get());
        }
        void visit(ASTBreak * ast) override {
            printKeyword("break");
        }
        void visit(ASTContinue * ast) override {
            printKeyword("continue");
        }
        void visit(ASTReturn * ast) override {
            printKeyword("retunr");
        }
        void visit(ASTBinaryOp * ast) override {
            auto scope = track(ast);
            visit(ast->left.get());
            printer_ << " ";
            printSymbols(ast->op.name());
            printer_ << " ";
            visit(ast->right.get());
        }
        void visit(ASTAssignment * ast) override {
            auto scope = track(ast);
            visit(ast->lvalue.get()); 
            printer_ << " ";
            printSymbols(ast->op.name());
            printer_ << " ";
            visit(ast->value.get());
        }
        void visit(ASTUnaryOp * ast) override {
            auto scope = track(ast);
            printSymbols(ast->op.name());
            visit(ast->arg.get());
        }
        void visit(ASTUnaryPostOp * ast) override {
            auto scope = track(ast);
            visit(ast->arg.get());
            printSymbols(ast->op.name());
        }
        void visit(ASTAddress * ast) override {
            auto scope = track(ast);
            printSymbols("&");
            visit(ast->target.get());
        }
        void visit(ASTDeref * ast) override {
            auto scope = track(ast);
            printSymbols("*");
            visit(ast->target.get());
        }
        void visit(ASTIndex * ast) override {
            auto scope = track(ast);
            visit(ast->base.get());
            printSymbols("[");
            visit(ast->index.get());
            printSymbols("]");
        }
        void visit(ASTMember * ast) override {
            auto scope = track(ast);
            visit(ast->base.get());
            printSymbols(".");
            visit(ast->member.get());
        }
        void visit(ASTMemberPtr * ast) override {
            auto scope = track(ast);
            visit(ast->base.get());
            printSymbols("->");
            visit(ast->member.get());
        }
        void visit(ASTCall * ast) override {
            auto scope = track(ast);
            visit(ast->function.get());
            printSymbols("(");
            auto i = ast->args.begin();
            if (i != ast->args.end()) {
                visit(i[0].get());
                while (++i != ast->args.end()) {
                    printSymbols(", ");
                    visit(i[0].get());
                }
            }
            printSymbols(")");
        }
        void visit(ASTCast * ast) override {
            auto scope = track(ast);
            printKeyword("cast");
            printSymbols("<");
            visit(ast->type.get());
            printSymbols(">(");
            visit(ast->value.get());
            printSymbols(")");
        }
    }; // class TranspilerASTVisiter

}; // namespace tinycpp
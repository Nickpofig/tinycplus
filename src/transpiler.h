#pragma once

#include "ast.h"

namespace tinycpp {

    class TranspilerASTVisiter : public ASTVisitor {
    public:
        void visit(AST * ast) override {
            if (auto exactAst = dynamic_cast<ASTInteger*>(ast)) {
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

        }
        void visit(ASTDouble * ast) override {

        }
        void visit(ASTChar * ast) override {

        }
        void visit(ASTString * ast) override {

        }
        void visit(ASTIdentifier * ast) override {

        }
        void visit(ASTType * ast) override {

        }
        void visit(ASTPointerType * ast) override {

        }
        void visit(ASTArrayType * ast) override {

        }
        void visit(ASTNamedType * ast) override {

        }
        void visit(ASTSequence * ast) override {

        }
        void visit(ASTBlock * ast) override {

        }
        void visit(ASTVarDecl * ast) override {

        }
        void visit(ASTFunDecl * ast) override {

        }
        void visit(ASTStructDecl * ast) override {

        }
        void visit(ASTFunPtrDecl * ast) override {

        }
        void visit(ASTIf * ast) override {

        }
        void visit(ASTSwitch * ast) override {

        }
        void visit(ASTWhile * ast) override {

        }
        void visit(ASTDoWhile * ast) override {

        }
        void visit(ASTFor * ast) override {

        }
        void visit(ASTBreak * ast) override {

        }
        void visit(ASTContinue * ast) override {

        }
        void visit(ASTReturn * ast) override {

        }
        void visit(ASTBinaryOp * ast) override {

        }
        void visit(ASTAssignment * ast) override {

        }
        void visit(ASTUnaryOp * ast) override {

        }
        void visit(ASTUnaryPostOp * ast) override {

        }
        void visit(ASTAddress * ast) override {

        }
        void visit(ASTDeref * ast) override {

        }
        void visit(ASTIndex * ast) override {

        }
        void visit(ASTMember * ast) override {

        }
        void visit(ASTMemberPtr * ast) override {

        }
        void visit(ASTCall * ast) override {

        }
        void visit(ASTCast * ast) override {

        }
    }; // class TranspilerASTVisiter

}; // namespace tinycpp
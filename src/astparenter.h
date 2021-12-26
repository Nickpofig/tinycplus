#pragma once

// internal
#include "ast.h"

namespace tinycpp {

    class ASTParenter : public ASTVisitor {
    private:
        void traverse(AST * parent, AST * child) {
            if (child == nullptr) return;
            child->parentAST = parent;
            visitChild(child);
        }

    public:
        void visit(AST * ast) override {
            visitChild(ast);
        }
        void visit(ASTInteger * ast) override { }
        void visit(ASTDouble * ast) override { }
        void visit(ASTChar * ast) override { }
        void visit(ASTString * ast) override { }
        void visit(ASTIdentifier * ast) override { }
        void visit(ASTType * ast) override { }
        void visit(ASTPointerType * ast) override {
            traverse(ast, ast->base.get());
        }
        void visit(ASTArrayType * ast) override {
            traverse(ast, ast->base.get());
            traverse(ast, ast->size.get());
        }
        void visit(ASTNamedType * ast) override { }
        void visit(ASTSequence * ast) override {
            for (auto & it : ast->body) {
                traverse(ast, it.get());
            }
        }
        void visit(ASTBlock * ast) override {
            for (auto & it : ast->body) {
                traverse(ast, it.get());
            }
        }
        void visit(ASTVarDecl * ast) override {
            traverse(ast, ast->type.get());
            traverse(ast, ast->name.get());
            traverse(ast, ast->value.get());
        }
        void visit(ASTFunDecl * ast) override {
            traverse(ast, ast->typeDecl.get());
            traverse(ast, ast->body.get());
            for (auto & it : ast->args) {
                traverse(ast, it.get());
            }
        }
        void visit(ASTFunPtrDecl * ast) override {
            traverse(ast, ast->returnType.get());
            traverse(ast, ast->name.get());
            for (auto & it : ast->args) {
                traverse(ast, it.get());
            }
        }
        void visit(ASTStructDecl * ast) override {
            for (auto & it : ast->fields) {
                traverse(ast, it.get());
            }
        }
        void visit(ASTClassDecl * ast) override {
            for (auto & it : ast->fields) {
                traverse(ast, it.get());
            }
            for (auto & it : ast->methods) {
                traverse(ast, it.get());
            }
        }
        void visit(ASTMethodDecl * ast) override {
            traverse(ast, ast->typeDecl.get());
            traverse(ast, ast->body.get());
            for (auto & it : ast->args) {
                traverse(ast, it.get());
            }
        }
        void visit(ASTIf * ast) override {
            traverse(ast, ast->cond.get());
            traverse(ast, ast->falseCase.get());
            traverse(ast, ast->trueCase.get());
        }
        void visit(ASTSwitch * ast) override {
            for (auto & it : ast->cases) {
                traverse(ast, it.second.get());
            }
            traverse(ast, ast->defaultCase.get());
        }
        void visit(ASTWhile * ast) override {
            traverse(ast, ast->cond.get());
            traverse(ast, ast->body.get());
        }
        void visit(ASTDoWhile * ast) override {
            traverse(ast, ast->cond.get());
            traverse(ast, ast->body.get());
        }
        void visit(ASTFor * ast) override {
            traverse(ast, ast->init.get());
            traverse(ast, ast->cond.get());
            traverse(ast, ast->increment.get());
            traverse(ast, ast->body.get());
        }
        void visit(ASTBreak * ast) override { }
        void visit(ASTContinue * ast) override { }
        void visit(ASTReturn * ast) override { }
        void visit(ASTBinaryOp * ast) override {
            traverse(ast, ast->left.get());
            traverse(ast, ast->right.get());
        }
        void visit(ASTAssignment * ast) override {
            traverse(ast, ast->lvalue.get());
            traverse(ast, ast->value.get());
        }
        void visit(ASTUnaryOp * ast) override {
            traverse(ast, ast->arg.get());
        }
        void visit(ASTUnaryPostOp * ast) override {
            traverse(ast, ast->arg.get());
        }
        void visit(ASTAddress * ast) override {
            traverse(ast, ast->target.get());
        }
        void visit(ASTDeref * ast) override {
            traverse(ast, ast->target.get());
        }
        void visit(ASTIndex * ast) override {
            traverse(ast, ast->base.get());
            traverse(ast, ast->index.get());
        }
        void visit(ASTMember * ast) override {
            traverse(ast, ast->base.get());
            traverse(ast, ast->member.get());
        }
        void visit(ASTCall * ast) override {
            traverse(ast, ast->function.get());
            for (auto & it : ast->args) {
                traverse(ast, it.get());
            }
        }
        void visit(ASTCast * ast) override {
            traverse(ast, ast->type.get());
            traverse(ast, ast->value.get());
        }
    }; // class ASTParenter

} // namespace tinycpp
#pragma once

// standard
#include <string>

// internal
#include "ast.h"
#include "types.h"

namespace tinycpp {

    class TypesSpace;

    class TypeChecker : public ASTVisitor {
    public:
        TypeChecker(TypesSpace & space);

        Type * getArithmeticResult(Type * lhs, Type * rhs) const;

        /** If the given type is a function pointer, returns the corresponding function. Otherwise returns nullptr. 
         */
        Type::Function const * asFunctionType(Type * t);

        void enterBlockContext() {
            context_.push_back(Context{context_.back().returnType, {}});
        }

        void enterFunctionContext(Type * returnType) {
            context_.push_back(Context{returnType, {}});
        }

        void leaveContext() {
            context_.pop_back();
        }

        void addVariable(Symbol name, Type * type) {
            context_.back().locals.insert(std::make_pair(name, type));
        }

        bool addGlobalVariable(Symbol name, Type * type) {
            // check if the name already exists
            if (context_.front().locals.find(name) != context_.front().locals.end())
                return false;
            context_.front().locals.insert(std::make_pair(name, type));
            return true;
        }

        /** Returns the type of variable.
         */
        Type * getVariable(Symbol name) {
            for (auto i = context_.rbegin(), e = context_.rend(); i != e; ++i) {
                auto l = i->locals.find(name);
                if (l != i->locals.end())
                    return l->second;
            }
            return nullptr;
        }

        Type * currentReturnType() {
            return context_.back().returnType;
        }

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
        void visit(ASTVarDecl * ast) override;
        void visit(ASTFunDecl * ast) override;
        void visit(ASTStructDecl * ast) override;
        void visit(ASTClassDecl * ast) override;
        void visit(ASTFunPtrDecl * ast) override;
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

    protected:
        Type * visitChild(AST * ast) {
            ASTVisitor::visitChild(ast);
            return ast->getType();
        }

        template<typename T>
        Type * visitChild(std::unique_ptr<T> const & ptr) {
            return visitChild(ptr.get());
        }

    private:
        struct Context {
            Type * returnType;
            std::unordered_map<Symbol, Type *> locals;
        };

        std::vector<Context> context_;
        TypesSpace & space_;

    }; // tinyc::TypeChecker

} // namespace tinyc
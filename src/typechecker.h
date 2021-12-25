#pragma once

// standard
#include <string>
#include <variant>

// internal
#include "ast.h"
#include "types.h"
#include "contexts.h"

namespace tinycpp {

    class TypeChecker : public ASTVisitor {
    private: // data
        NamesContext & names_;
        TypesContext & types_;

    private: // transpiler case configurations
        struct Context {
            struct Member {
                Type::Complex * memberBaseType;
            };
            struct Complex {
                Type::Complex * complexType;
            };
        };
        std::vector<std::variant<
            Context::Member,
            Context::Complex
        >> contextStack_;

        template<typename T>
        size_t push(T && context) {
            contextStack_.push_back(context);
            return contextStack_.size() - 1;
        }

        template<typename T>
        std::optional<T> pop() {
            if (contextStack_.size() > 0) {
                auto context = contextStack_.back();
                contextStack_.pop_back();
                auto * result = std::get_if<T>(&context);
                return result == nullptr 
                    ? std::optional<T>{std::nullopt}
                    : std::optional<T>{*result};
            }
            return std::nullopt;
        }

        void wipeContext(size_t position) {
            if (position > contextStack_.size()) return;
            contextStack_.erase(contextStack_.begin() + position, contextStack_.end());
        }

    public: // constructor
        TypeChecker(TypesContext & types, NamesContext & names);

    public: // helper methods
        Type * getArithmeticResult(Type * lhs, Type * rhs) const;

        /** If the given type is a function pointer, returns the corresponding function. Otherwise returns nullptr. 
         */
        Type::Function const * asFunctionType(Type * t);

    public: // visitor implementation
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

    protected: // shortcuts
        Type * visitChild(AST * ast) {
            ASTVisitor::visitChild(ast);
            return ast->getType();
        }

        template<typename T>
        Type * visitChild(std::unique_ptr<T> const & ptr) {
            return visitChild(ptr.get());
        }

    }; // tinyc::TypeChecker

} // namespace tinyc
#pragma once

// standard
#include <iostream>
#include <vector>

// internal
#include "ast.h"
#include "types.h"

namespace tinycpp {

    class Transpiler : public ASTVisitor {
    private: // persistant data
        ASTPrettyPrinter printer_;
        bool isPrintColorful_ = false;
        int inheritanceDepth = 0;
    public:
        Transpiler(std::ostream & output, bool isColorful)
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
        void printClassPrefix(Type const * classType, tiny::color color) {
            if (dynamic_cast<Type::Class const *>(classType) == nullptr) {
                throw std::runtime_error{STR("Expected class type, but got: " << classType->toString() << " type.")};
            }
            if (isPrintColorful_) printer_ << color;
            printer_ << classType->toString() << "_";
        }
    public:
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
    }; // class Transpiler

}; // namespace tinycpp
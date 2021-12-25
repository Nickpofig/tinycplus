#pragma once

// standard
#include <iostream>
#include <vector>

// internal
#include "ast.h"
#include "types.h"
#include "contexts.h"

namespace tinycpp {

    class Transpiler : public ASTVisitor {
    private: // persistant data
        NamesContext & names_;
        ASTPrettyPrinter printer_;
        bool isPrintColorful_ = false;
    private: // temporary data
        int inheritanceDepth = 0;
    public:
        Transpiler(NamesContext & names, std::ostream & output, bool isColorful)
            :names_{names}
            ,printer_{output}
            ,isPrintColorful_{isColorful}
        { }
    private:
        inline void print(Symbol const & symbol, tiny::color color) {
            if (isPrintColorful_) printer_ << color;
            printer_ << symbol.name();
        }
        void printSpace() { printer_ << " "; }
        void printSymbol(Symbol const & name) { print(name, printer_.symbol);}
        void printIdentifier(Symbol const & name) { print(name, printer_.identifier); }
        void printType(Symbol const & name) { print(name, printer_.type); }
        void printKeyword(Symbol const & name) { print(name, printer_.keyword); }
        void printNumber(int64_t value) {
            if (isPrintColorful_) printer_ << printer_.numberLiteral;
            printer_ << value;
        }
        void printClassPrefix(Type const * classType) {
            if (dynamic_cast<Type::Class const *>(classType) == nullptr) {
                throw std::runtime_error{STR("Expected class type, but got: " << classType->toString() << " type.")};
            }
            if (isPrintColorful_) printer_ << printer_.identifier;
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
#pragma once

// standard
#include <vector>
#include <memory>
#include <string>
#include <sstream>

// internal
#include "shared.h"

namespace tinycpp {

    class ASTContext {
    private:
        ASTPrettyPrinter printer_;
        std::vector<Symbol> currentDomain_;
    public:
        ASTContext(): printer_{std::cout} { }
    public:
        ASTPrettyPrinter & getPrinter() {
            return printer_;
        }
    public:
        void print(Token const & token) {
            switch (token.kind())
            {
            case Token::Kind::Identifier:
                printer_ << printer_.identifier << token.valueSymbol().name();
                break;
            case Token::Kind::Double:
                printer_ << printer_.numberLiteral <<  token.valueDouble();
                break;
            case Token::Kind::Integer:
                printer_ << printer_.numberLiteral << token.valueInt();
                break;
            case Token::Kind::StringDoubleQuoted:
                printer_ << printer_.stringLiteral << "\"" << token.valueString() << "\"";
                break;
            case Token::Kind::StringSingleQuoted:
                printer_ << printer_.stringLiteral << "'" << token.valueString() << "'";
                break;
            case Token::Kind::Operator:
                printer_ << token.valueSymbol().name();
                break;
            default:
                break;
            }
            printer_ << color::reset << " ";
        }

        void printName(Symbol name, color color) {
            printer_ << color;
            for (auto & name : currentDomain_) {
                printer_ << name.name() << "_";
            }
            printer_ << name.name();
            printer_ << color::reset;
        }

        void enterNamespace(Symbol name){
            currentDomain_.push_back(name);
        }

        void exitNamespace() {
            currentDomain_.pop_back();
        }
    }; // struct ASTContext


    class AST : public ASTBase {
    protected:
        AST(Token const & token) : ASTBase{token} { }
    public:
        virtual void print(ASTContext & context) = 0;
        void print(ASTPrettyPrinter & printer) const override {
            assert(false && "not implemented");
        }
    public:
        class Scope;
        class Raw;
        class Type;
        class Variable;
        class FunctionPointer;
        class Struct;
        class Function;
        class Field;
        class Class;
    }; // class AST


    class AST::Scope : public AST {
    private:
        std::vector<uptr<AST>> content_;
    public:
        Scope(Token const & token): AST{token} { }
    public:
        void print(ASTContext & context) override {
            for (auto & ast : content_) {
                ast->print(context);
                context.getPrinter().newline();
            }
        }
        void take(uptr<AST> & ast) {
            content_.push_back(std::move(ast));
        }
        void take(uptr<AST> && ast) {
            content_.push_back(std::move(ast));
        }
    }; // class AST::Scope


    class AST::Raw : public AST {
    private:
        std::vector<Token> tokens_;
    public:
        Raw(Token const & token): AST{token} { }
    public:
        void print(ASTContext & context) override {
            for (auto & t : tokens_) {
                context.print(t);
            }
        }

        void add(Token token) {
            tokens_.push_back(token);
        }
    }; // class AST::Raw

    class AST::Type : public AST {
    private:
        Symbol name_;
        int pointerCount_;
        int arraySize_;
        // need a way to destinguish "set of traits" from other types.
    public:
        Type(Token const & token, Symbol name) : AST{token}
            ,name_{name}
            ,pointerCount_{0}
            ,arraySize_{0}
        { }
    public:
        void print(ASTContext & context) {
            assert(false && "not implemented");
        }

        void increamentPointerCount() {
            pointerCount_++;
        }

        void setArraySize(int value) {
            arraySize_ = value;
        }
    };


    class AST::Variable : public AST {
    private:
        Symbol name_;
        uptr<AST> type_;
        uptr<AST> assignment_;
    public:
        Variable(
            Token const & nameToken,
            uptr<AST::Type> & type,
            Symbol name
        ): AST{nameToken}
            ,type_{std::move(type)}
            ,name_{name}
        { }
    public:
        void print(ASTContext & context) {
            assert(false && "not implemented");
        }

        void takeAsAssignemnt(uptr<AST> & ast) {
            assignment_ = std::move(ast);
        }
    }; // class AST::Variable


    class AST::FunctionPointer : public AST {
    private:
        Symbol returnType_;
        Symbol name_;
        std::vector<uptr<AST>> parameters_;
    public:
        FunctionPointer(
            Token const & nameToken,
            Symbol returnType,
            Symbol name
        ): AST(nameToken)
            ,returnType_{returnType_}
            ,name_{name}
        { }
    public:
        void print(ASTContext & context) {
            assert(false && "not implemented");
        }
        void takeAsParameter(uptr<AST> & ast) {
            parameters_.push_back(std::move(ast));
        }
    }; // class AST::FunctionPointer


    class AST::Struct : public AST {
    private:
        Symbol name_;
        std::vector<uptr<AST>> fields_;
    public:
        Struct(Token const & token, Symbol name) : AST{token}, name_{name} { }
    public:
        void print(ASTContext & context) override {
            auto printer = context.getPrinter();
            printer << "struct ";
            context.printName(name_, printer.keyword);
            printer << "{\n";
            printer.indent();
            for (auto & ast : fields_)
            {
                ast->print(context);
                printer.newline();
            }
            printer.dedent();
            printer << "}\n";
        }

        void takeAsField(uptr<AST> & ast) {
            fields_.push_back(std::move(ast));
        }
    }; // class AST::Struct


    class AST::Function : public AST {
    private:
        Symbol name_;
        uptr<AST::Type> type_;
        std::vector<uptr<AST>> parameters_;
    public:
        Function(
            Token const & token,
            uptr<AST::Type> & type,
            Symbol name
        ): AST{token}
            ,type_{std::move(type)}
            ,name_{name}
        { }
    public:
        void takeAsParameter(uptr<AST> & ast) {
            parameters_.push_back(std::move(ast));
        }
    }; // class AST::Function

    enum class AccessLevel {
        Override,
        Private,
        Protected,
        Public,
    };

    class AST::Field : public AST::Variable {
    private:
        AccessLevel access_;
    public:
        Field(
            Token const & nameToken,
            uptr<AST::Type> & type,
            Symbol name,
            AccessLevel access
        ): Variable{nameToken, type, name}
            ,access_{access}
        { }
    };

    class AST::Class : public AST {
    private:
        Symbol name_;
        std::vector<uptr<AST>> traits_;
        std::vector<uptr<AST>> fields_;
        std::vector<uptr<AST>> functions_;
    public:
        Class(Token const & token) : AST{token}, name_{token.valueSymbol()} { }
    public:
        void print(ASTContext & context) override {
            auto p = context.getPrinter();
            p.newline();
            p << p.keyword << "struct "
              << p.identifier << name_.name()
              << color::reset << " {";
            p.indent();
            p.newline();
            for (auto & ast : fields_)
            {
                ast->print(context);
                p.newline();
            }
            p.dedent();
            p.newline();
            p << "}";
            p.newline();
            for (auto & ast : functions_)
            {
                ast->print(context);
                p.newline();
            }
        }

        void takeAsTraitType(uptr<AST> & ast) {
            traits_.push_back(std::move(ast));
        }

        void takeAsField(uptr<AST> & ast) {
            fields_.push_back(std::move(ast));
        }

        void takeAsMethod(uptr<AST> & ast) {
            fields_.push_back(std::move(ast));
        }
    }; // class AST::Class

} // namespace tinycpp
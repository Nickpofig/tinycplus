#pragma once

// standard
#include <cassert>

// internal
#include "shared.h"
#include "ast.h"

namespace tinycpp {

    /** Representation of a tinycpp type.
     */
    class Type {
    public: // subtypes
        class Alias;
        class POD;
        class Pointer;
        class Function;
        class Complex;
        class Struct;
        class Class;
        class Method;

    public:
        virtual ~Type() = default;

    public:
        /** Converts the type to a printable string.
         */
        std::string toString() const {
            std::stringstream ss;
            toStream(ss);
            return ss.str();
        }

        /** Determines if the type is fully defined.
            A fully defined type can be instantiated. In general all types with the exception of forward declared structures are fully defined.
         */
        virtual bool isFullyDefined() const {
            return true;
        }

        virtual bool isPointer() const {
            return false;
        }

        template<typename T>
        T * getCore();

    private:
        virtual void toStream(std::ostream & s) const = 0;
    }; // tinycpp::Type

    /** A type alias, i.e. a different name for same type.
     */
    class Type::Alias : public Type {
    public:
        Alias(Symbol name, Type * base):
            name_{name},
            base_{base} {
        }

        bool isFullyDefined() const override {
            return base_->isFullyDefined();
        }

        Type * base() const {
            return base_;
        }

    private:

        void toStream(std::ostream & s) const override {
            s << name_.name();
        }

        Symbol name_;
        Type * base_;

    }; // tinycpp::Type::Alias

    /** Plain old data type declaration.
        These are created automatically by the backend for the primitive types supported by the language. In the case of tinyC, these are:
        - int
        - double
        - char
        - void ??
     */
    class Type::POD : public Type {
    public:

        POD(Symbol name):
            name_{name} {
        }

    private:

        void toStream(std::ostream & s) const override {
            s << name_.name();
        }

        Symbol name_;
    }; // tinycpp::Type::POD


    /** Pointer to a type. 
     */
    class Type::Pointer : public Type {
    public:
        Pointer(Type * base):
            base_{base} {
        }

        Type * base() const {
            return base_;
        }

        bool isPointer() const override {
            return true;
        }

    private:

        void toStream(std::ostream & s) const override {
            base_->toStream(s);
            s << "*";
        }

        Type * base_;
    };


    /** Function type declaration.
     * 
        A function type. Determines the types of the arguments (we do not need names for the type as tinyC does not support keyword arguments or other fancy things) and the return type.
     */
    class Type::Function : public Type {
    protected:
        Type * returnType_;
        std::vector<Type *> args_;

    public:
        Function(Type * returnType):
            returnType_{returnType} {
        }

        Type * returnType() const {
            return returnType_;
        }

        void addArgument(Type * type) {
            args_.push_back(type);
        }

        size_t numArgs() const {
            return args_.size();
        }

        Type * argType(size_t i) const {
            return args_[i];
        }

    private:
        void toStream(std::ostream & s) const override {
            returnType_->toStream(s);
            s << " (";
            auto i = args_.begin();
            auto e = args_.end();
            if (i != e) {
                (*i)->toStream(s);
                while (++i != e) {
                    s << ", ";
                    (*i)->toStream(s);
                }
            }
            s << ")";
        }
    }; // tinycpp::Type::Function

    /** Complex declaration.
     * 
        Keeps a mapping from the fields to their types.
     */
    class Type::Complex : public Type {
    private:
        std::unordered_map<Symbol, Type *> members_;
    public:
        virtual void registerMember(Symbol name, Type * type, AST * ast) {
            if (!type->isFullyDefined())
                throw ParserError{STR("Member " << name.name() << " has not fully defined type " << type->toString()), ast->location()};
            if (members_.find(name) != members_.end())
                throw ParserError{STR("Member " << name.name() << " already defined "), ast->location()};
            members_.insert(std::make_pair(name, type));
        }

        virtual Type * getMemberType(Symbol name) const {
            auto it = members_.find(name);
            return it == members_.end() ? nullptr : it->second;
        }
    }; // tinycpp::Type::Complex

    /** Structure declaration.
     * 
        Keeps a mapping from the fields to their types and the AST where the type was declared.
     */
    class Type::Struct : public Type::Complex {
    public:
        Struct(ASTStructDecl * ast):
            ast_{ast} {
        }

        ASTStructDecl * ast() const {
            return ast_;
        }

        /** Struct type is fully defined if its ast the definition, not just forward declaration of the type.
         */
        bool isFullyDefined() const override {
            return ast_ != nullptr && ast_->isDefinition;
        }

        void updateDefinition(ASTStructDecl * ast) {
            assert(ast_ == nullptr || ! ast_->isDefinition);
            ast_ = ast;
        }

    private:
        friend class TypeChecker;

        void toStream(std::ostream & s) const override {
            s << ast_->name.name();
        }

        ASTStructDecl * ast_;
    }; // tinycpp::Type::Struct


    /** Structure declaration.
     * 
        Keeps a mapping from the fields and methods to their types and the AST where the type was declared.
     */
    class Type::Class : public Type::Complex {
    private:
        ASTClassDecl * ast_;
        Type::Class * base_ = nullptr; 
    public:
        Class(ASTClassDecl * ast):
            ast_{ast} {
        }
    public:
        ASTClassDecl * ast() const {
            return ast_;
        }

        /** Struct type is fully defined if its ast the definition, not just forward declaration of the type.
         */
        bool isFullyDefined() const override {
            return ast_ != nullptr && ast_->isDefinition;
        }

        Type::Class * getBase() {
            return base_;
        }

        void setBase(Type * type) {
            base_ = dynamic_cast<Type::Class *>(type);
            if (!base_) {
                throw std::runtime_error(STR("[T1] A type: " << (type ? type->toString() : "null") 
                    << " - is not an instance of class type.")
                );
            }
        }

        void updateDefinition(ASTClassDecl * ast) {
            assert(ast_ == nullptr || ! ast_->isDefinition);
            ast_ = ast;
        }

    public:
        void registerMember(Symbol name, Type * type, AST * ast) override {
            if (auto * type = getMemberType(name); type != nullptr) {
                throw ParserError{
                    STR("Member with name \"" << name.name() << "\""
                        << " is declared the second time (which is not allowed!)"
                        << " in class \"" << toString() << "\""),
                    ast->location()
                };
            }
            Type::Complex::registerMember(name, type, ast);
        }

        Type * getMemberType(Symbol name) const override {
            auto * type = Type::Complex::getMemberType(name);
            if (type == nullptr && base_ != nullptr) {
                type = base_->getMemberType(name);
            }
            return type;
        }

    private:
        friend class TypeChecker;

        void toStream(std::ostream & s) const override {
            s << ast_->name.name();
        }
    }; // tinycpp::Type::Class


    /** Structure declaration.
     * 
        Keeps a reference to class type and everything that Function type has.
     */
    class Type::Method : public Function {
    public:
        Type * classType;
    public:
        Method(Type * returnType, Type * classType): Function{returnType}, classType{classType} { }
    private:
        void toStream(std::ostream & s) const override {
            returnType_->toStream(s);
            s << " (";
            auto i = args_.begin();
            auto e = args_.end();
            if (i != e) {
                (*i)->toStream(s);
                while (++i != e) {
                    s << ", ";
                    (*i)->toStream(s);
                }
            }
            s << ")";
        }
    }; // tinycpp::Type::Method


    template<typename T>
    T * Type::getCore() { // [?] defined after everything in order to work with fully defined types
        if (auto asIs = dynamic_cast<T*>(this)) {
            return asIs;
        }
        if (auto asPointer = dynamic_cast<Type::Pointer *>(this)) {
            return asPointer->base()->getCore<T>();
        }
        return nullptr;
    }
} // namespace tinycpp
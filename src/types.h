#pragma once

// standard
#include <cassert>
#include <optional>

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
        class VTable;

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

        template<typename T>
        T * as() {
            return dynamic_cast<T*>(this);
        }

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
    public:
        struct Member {
            Type * type;
            AST * ast;
        };
    private:
        std::unordered_map<Symbol, Member> members_;
        std::vector<Symbol> order_;
        std::optional<Symbol> constructorName_;
    protected:
        void throwMemberIsAlreadyDefined(Symbol name, AST * ast) {
            throw ParserError{
                STR("Member " << name.name() << " already defined "),
                ast->location()
            };
        }
        void checkMemberTypeIsFullyDefined(Symbol & name, Type * type, AST * ast) {
            if (!type->isFullyDefined()) throw ParserError{
                STR("Member " << name.name() << " has not fully defined type " << type->toString()),
                ast->location()
            };
        }
    public:
        virtual void registerMember(Symbol name, Type * type, AST * ast) {
            checkMemberTypeIsFullyDefined(name, type, ast);
            if (members_.find(name) != members_.end()) { 
                throwMemberIsAlreadyDefined(name, ast);
            }
            members_.insert(std::make_pair(name, Member{type, ast}));
            order_.push_back(name);
        }

        void overrideMember(Symbol name, Type * type, AST * ast) {
            checkMemberTypeIsFullyDefined(name, type, ast);
            if (members_.find(name) == members_.end()) { 
                throwMemberIsAlreadyDefined(name, ast);
            }
            members_[name] = Member{type, ast};
        }

        virtual Type * getMemberType(Symbol name) const {
            auto it = members_.find(name);
            return it == members_.end() ? nullptr : it->second.type;
        }

        virtual AST * getMemberDeclaration(Symbol name) const {
            auto it = members_.find(name);
            return it == members_.end() ? nullptr : it->second.ast;
        }

        virtual bool requiresImplicitConstruction() const {
            for (auto & member : members_) {
                auto * memberType = member.second.type;
                if (memberType->isPointer()) continue;
                if (auto * complexMemberType = memberType->as<Type::Complex>();
                    complexMemberType != nullptr && complexMemberType->requiresImplicitConstruction()
                ) return true;
            }
            return false;
        }

    public:
        Symbol getConstructorName() {
            if (!constructorName_.has_value()) {
                constructorName_ = Symbol{STR("__tinycpp__make__" << toString())};
            }
            return constructorName_.value();
        }

        void copyMembersTo(Type::Complex * other) {
            for (auto & name : order_) {
                other->members_.insert({name, members_[name]});
                other->order_.push_back(name);
            }
        }

        void collectMembersOrdered(std::vector<std::pair<Symbol, Member>> & result) {
            for (auto & name : order_) {
                result.push_back({name, members_[name]});
            }
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


    class Type::VTable : public Type::Complex {
    private:
        Symbol name_;
    public:
        VTable(Symbol name): name_{name} { }
    public:
        Symbol getGlobalInstanceName() {
            return Symbol{STR(name_.name() << "instance__")};
        }
        Symbol getGlobalInitFunctionName() {
            return Symbol{STR(name_.name() << "init__")};
        }

        bool requiresImplicitConstruction() const override {
            return false;
        }
    private:
        friend class TypeChecker;
        void toStream(std::ostream & s) const override {
            s << name_.name();
        }
    };

    /** Structure declaration.
     * 
        Keeps a mapping from the fields and methods to their types and the AST where the type was declared.
     */
    class Type::Class : public Type::Complex {
    public:
        struct MethodInfo {
            Symbol name;
            Symbol fullName;
            Type::Function * type;
            Type::Class * targetClassType;
            ASTMethodDecl * ast;
        };
    private:
        ASTClassDecl * ast_;
        Type::Class * base_ = nullptr;
        Type::VTable * vtable_ = nullptr;
        std::vector<MethodInfo> methods_;
    public:
        Class(ASTClassDecl * ast):
            ast_{ast} {
        }
    public:
        ASTClassDecl * ast() const {
            return ast_;
        }

        Type::Class * getBase() const {
            return base_;
        }

        void setBase(Type::Class * type) {
            if (!type->isFullyDefined()) throw ParserError{
                STR("[T2] A base type must be fully defined before inherited."),
                ast_->location()
            };
            base_ = type;
            vtable_ = type->vtable_;
        }

        Type::VTable * getVirtualTable() const {
            return vtable_;
        }

        void setVirtualTable(Type::VTable * type) {
            if (vtable_ != nullptr) {
                vtable_->copyMembersTo(type);
            }
            vtable_ = type;
        }

        bool hasOwnVirtualTable() {
            return vtable_ != nullptr && (base_ == nullptr || base_->vtable_ != vtable_);
        }

        void updateDefinition(ASTClassDecl * ast) {
            assert(ast_ == nullptr || ! ast_->isDefinition);
            ast_ = ast;
        }

        bool hasMethod(Symbol name, bool includeBaseInSearch) const {
            for (auto & method : methods_) {
                if (method.name == name) {
                    return true;
                }
            }
            if (includeBaseInSearch && base_ != nullptr) {
                return base_->hasMethod(name, true);
            }
            return false;
        }

        MethodInfo getMethodInfo(Symbol name) const {
            for (auto & method : methods_) {
                if (method.name == name) {
                    return method;
                }
            }
            if (base_ != nullptr) {
                return base_->getMethodInfo(name);
            }
            throw ParserError {
                STR("There is no method with name: " << name.name()),
                ast_->location()
            };
        }

    public: // overrides
        /** Struct type is fully defined if its ast the definition, not just forward declaration of the type.
         */
        bool isFullyDefined() const override {
            return ast_ != nullptr && ast_->isDefinition;
        }

        void registerMember(Symbol name, Type * type, AST * ast) override {
            auto * method = ast->as<ASTMethodDecl>();
            if (method != nullptr) {
                if (hasMethod(name, false)) {
                    throwMemberIsAlreadyDefined(name, ast);
                }
                if (method->isOverride()) {
                    if (base_ == nullptr) throw ParserError{
                        STR("There is no base class to override"),
                        ast->location()
                    };
                    if (!base_->hasMethod(name, true)) throw ParserError{
                        STR("There is no base method called " << name << " to override"),
                        ast->location()
                    };
                    // reassigning new override implementation
                }
                bool isVirtual = method->isVirtual();
                auto fullName = Symbol{
                    STR("__tinycpp__" << toString() << (isVirtual ? "__virtual__" : "__") << name.name())
                };
                methods_.push_back(MethodInfo{name, fullName, type->as<Type::Function>(), this, method});
            }
            else {
                if (auto * type = getMemberType(name); type != nullptr) {
                    throwMemberIsAlreadyDefined(name, ast);
                }
                Type::Complex::registerMember(name, type, ast);
            }
        }

        bool requiresImplicitConstruction() const override {
            return true;
        }

        Type * getMemberType(Symbol name) const override {
            auto * type = Type::Complex::getMemberType(name);
            if (type == nullptr) {
                for (auto & method : methods_) {
                    if (method.name == name) {
                        return method.type;
                    }
                }
            }
            if (type == nullptr && base_ != nullptr) {
                type = base_->getMemberType(name);
            }
            return type;
        }

        AST * getMemberDeclaration(Symbol name) const override {
            auto * ast = Type::Complex::getMemberDeclaration(name);
            if (ast == nullptr) {
                for (auto & method : methods_) {
                    if (method.name == name) {
                        return method.ast;
                    }
                }
            }
            if (ast == nullptr && base_ != nullptr) {
                ast = base_->getMemberDeclaration(name);
            }
            return ast;
        }

    private:
        friend class TypeChecker;
        void toStream(std::ostream & s) const override {
            s << ast_->name.name();
        }
    }; // tinycpp::Type::Class


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
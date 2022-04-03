#pragma once

// standard
#include <cassert>
#include <optional>
#include <algorithm>

// internal
#include "shared.h"
#include "ast.h"

namespace tinycplus {

    /** Representation of a tinycplus type.
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
    }; // tinycplus::Type


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

    }; // tinycplus::Type::Alias

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
    }; // tinycplus::Type::POD


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
    }; // tinycplus::Type::Function

    class FieldInfo {
    public:
        Symbol name;
        Type * type;
        AST * ast;
    public:
        FieldInfo(): name{""} { }
        FieldInfo(Symbol name, Type * type, AST * ast):
            name{name},
            type{type},
            ast{ast}
        { }
    };

    class MethodInfo {
    public:
        Symbol name;
        Symbol fullName;
        Type::Function * type;
        ASTMethodDecl * ast;
    };

    /** Complex declaration.
     * 
        Keeps a mapping from the fields to their types.
     */
    class Type::Complex : public Type {
    protected:
        std::unordered_map<Symbol, FieldInfo> fields_;
        std::vector<Symbol> fieldsOrder_;
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
        virtual void registerField(Symbol name, Type * type, AST * ast) {
            checkMemberTypeIsFullyDefined(name, type, ast);
            if (fields_.find(name) != fields_.end()) { 
                throwMemberIsAlreadyDefined(name, ast);
            }
            fields_.insert(std::make_pair(name, FieldInfo{name, type, ast}));
            fieldsOrder_.push_back(name);
        }

        virtual bool requiresImplicitConstruction() const {
            for (auto & field : fields_) {
                auto * fieldType = field.second.type->as<Type::Complex>();
                if (fieldType == nullptr) {
                    continue;
                }
                if (fieldType->requiresImplicitConstruction()) {
                    return true;
                }
            }
            return false;
        }

        virtual std::optional<FieldInfo> getFieldInfo(Symbol name) const {
            auto it = fields_.find(name);
            if (it == fields_.end()) {
                return std::nullopt;
            }
            return it->second;
        }

        virtual Type * getMemberType(Symbol name) const {
            auto field = getFieldInfo(name);
            if (field.has_value()) return field.value().type;
            return nullptr;
        }

        virtual void collectFieldsOrdered(std::vector<FieldInfo> & resultAppendList) const {
            for (auto & name : fieldsOrder_) {
                resultAppendList.push_back(fields_.at(name));
            }
        }
    public:
        Symbol getConstructorName() {
            if (!constructorName_.has_value()) {
                constructorName_ = symbols::startLanguageName()
                    .add("make_")
                    .add(toString())
                    .end();
            }
            return constructorName_.value();
        }

        void copyFieldsTo(Type::Complex * other) {
            for (auto & name : fieldsOrder_) {
                auto field = fields_[name];
                other->fields_.insert(std::make_pair(name, field));
                other->fieldsOrder_.push_back(name);
            }
        }

        /// TODO: remove this
        // void copyMembersTo(Type::Complex * other) {
        //     for (auto & name : fieldsOrder_) {
        //         other->fields_.insert({name, fields_[name]});
        //         other->fieldsOrder_.push_back(name);
        //     }
        // }
    }; // tinycplus::Type::Complex

    /** Structure declaration.
     * 
        Keeps a mapping from the fields to their types and the AST where the type was declared.
     */
    class Type::Struct : public Type::Complex {
    public:
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
    }; // tinycplus::Type::Struct


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
    public: // overrides
        void registerField(Symbol name, Type * type, AST * ast) override {
            checkMemberTypeIsFullyDefined(name, type, ast);
            if (fields_.find(name) == fields_.end()) {
                fields_.insert(std::make_pair(name, FieldInfo{name, type, ast}));
                fieldsOrder_.push_back(name);
            } else {
                fields_[name] = FieldInfo{name, type, ast};
            }
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
    private:
        ASTClassDecl * ast_ = nullptr;
        Type::Class * base_ = nullptr;
        Type::VTable * vtable_ = nullptr;
        std::unordered_map<Symbol, MethodInfo> methods_;
        bool isAbstract_ = false;
    public:
        Class(Type::VTable * vtable):
            vtable_{vtable} {
        }
    public:
        Symbol getName() const {
            return ast_->name;
        }

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
            base_->getVirtualTable()->copyFieldsTo(vtable_);
        }

        Type::VTable * getVirtualTable() const {
            return vtable_;
        }

        void updateDefinition(ASTClassDecl * ast) {
            assert(ast_ == nullptr || ! ast_->isDefinition);
            ast_ = ast;
        }

        bool hasOwnVirtualTable() {
            return vtable_ != nullptr && (base_ == nullptr || base_->vtable_ != vtable_);
        }

        bool hasMethod(Symbol name, bool includeBaseInSearch) const {
            for (auto & method : methods_) {
                if (method.first == name) {
                    return true;
                }
            }
            if (includeBaseInSearch && base_ != nullptr) {
                return base_->hasMethod(name, true);
            }
            return false;
        }

        std::optional<MethodInfo> getMethodInfo(Symbol name, bool searchInBase = true) const {
            for (auto & method : methods_) {
                if (method.first == name) {
                    return method.second;
                }
            }
            if (base_ != nullptr) {
                return base_->getMethodInfo(name);
            }
            return std::nullopt;
            // throw ParserError {
            //     STR("There is no method with name: " << name.name()),
            //     ast_->location()
            // };
        }

        bool isAbstract() const {
            return isAbstract_;
        }

        void registerMethod(Symbol name, Type::Function * type, ASTMethodDecl * ast) {
            this->isAbstract_ |= ast->isAbstract();
            if (hasMethod(name, false)) {
                throwMemberIsAlreadyDefined(name, ast);
            }
            if (ast->isOverride()) {
                if (base_ == nullptr) throw ParserError {
                    STR("There is no base class to override"),
                    ast->location()
                };
                if (!base_->hasMethod(name, true)) throw ParserError {
                    STR("There is no base method called " << name << " to override"),
                    ast->location()
                };
            }
            bool isVirtual = ast->isVirtualized();
            auto fullName = symbols::startLanguageName()
                .add(toString())
                .add(isVirtual ? "__virtual__" : "__")
                .add(name)
                .end();
            methods_.insert({ name, MethodInfo{name, fullName, type, ast} });
        }

    public: // overrides
        /** Struct type is fully defined if its ast the definition, not just forward declaration of the type.
         */
        bool isFullyDefined() const override {
            return ast_ != nullptr && ast_->isDefinition;
        }

        bool requiresImplicitConstruction() const override {
            return true;
        }

        std::optional<FieldInfo> getFieldInfo(Symbol name) const override {
            auto fieldInfo = Complex::getFieldInfo(name);
            if (fieldInfo.has_value()) return fieldInfo.value();
            if (base_ != nullptr) {
                return base_->getFieldInfo(name);
            }
            return std::nullopt;
        }

        Type * getMemberType(Symbol name) const override {
            auto field = getFieldInfo(name);
            if (field.has_value()) return field.value().type;
            auto method = getMethodInfo(name);
            if (method.has_value()) return method.value().type;
            return nullptr;
        }

        void collectFieldsOrdered(std::vector<FieldInfo> & result) const override {
            if (base_ != nullptr) {
                base_->collectFieldsOrdered(result);
            }
            Type::Complex::collectFieldsOrdered(result);
        }

        void collectVirtualTables(std::vector<Type::VTable*> & result) const {
            // ! Assuming that class hierarchy is always finite, then...
            if (base_ != nullptr) {
                assert(base_->vtable_ != nullptr && "oh no, vtable is assumed to be unique, therefor -> not null");
                base_->collectVirtualTables(result);
            }
            result.push_back(vtable_);
        }
    private:
        friend class TypeChecker;
        void toStream(std::ostream & s) const override {
            s << ast_->name.name();
        }
    }; // tinycplus::Type::Class


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
} // namespace tinycplus
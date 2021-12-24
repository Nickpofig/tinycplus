#pragma once

// standard
#include <cassert>

// external
#include "common/symbol.h"

// internal
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
            return ast_->isDefinition;
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
    T * Type::getCore() {
        if (auto asIs = dynamic_cast<T*>(this)) {
            return asIs;
        }
        if (auto asPointer = dynamic_cast<Type::Pointer *>(this)) {
            return asPointer->base()->getCore<T>();
        }
        return nullptr;
    }


    /** The TinyC+ program types infromation.
     */
    class TypesSpace {
    private: // data
        std::unordered_map<std::string, std::unique_ptr<Type>> types_;
        Type * int_;
        Type * double_;
        Type * char_;
        Type * void_;

    public: // constructors
        TypesSpace() {
            int_ = types_.insert(std::make_pair(Symbol::KwInt.name(), std::unique_ptr<Type>{new Type::POD{Symbol::KwInt}})).first->second.get();
            double_ = types_.insert(std::make_pair(Symbol::KwDouble.name(), std::unique_ptr<Type>{new Type::POD{Symbol::KwDouble}})).first->second.get();
            char_ = types_.insert(std::make_pair(Symbol::KwChar.name(), std::unique_ptr<Type>{new Type::POD{Symbol::KwChar}})).first->second.get();
            void_ = types_.insert(std::make_pair(Symbol::KwVoid.name(), std::unique_ptr<Type>{new Type::POD{Symbol::KwVoid}})).first->second.get();
        }

    public: // getters
        Type * getTypeInt() const {
            return int_;
        }

        Type * getTypeDouble() const {
            return double_;
        }

        Type * getTypeChar() const {
            return char_;
        }

        Type * getTypeVoid() const {
            return void_;
        }

        Type * getType(Symbol symbol) const {
            auto i = types_.find(symbol.name());
            if (i == types_.end())
                return nullptr;
            Type * result = i->second.get();
            // check if it is a type alias, and if so, return the base type
            Type::Alias * alias = dynamic_cast<Type::Alias*>(result);
            if (alias != nullptr)
                return alias->base();
            else
                return result;
        }

        /** Determines whether given name is a known type name.
            It's a typename if we find it in the type declarations.
         */
        bool isTypeName(Symbol name) const {
            return getType(name) != nullptr;
        }

        bool isPointer(Type * t) const {
            assert(dynamic_cast<Type::Alias const *>(t) == nullptr);
            return dynamic_cast<Type::Pointer const *>(t) != nullptr;
        }

        bool isPOD(Type * t) const {
            assert(dynamic_cast<Type::Alias const *>(t) == nullptr);
            return t == char_ || t == int_ || t == double_;
        }

        bool convertsToBool(Type * t) const {
            assert(dynamic_cast<Type::Alias const *>(t) == nullptr);
            return isPointer(t) || isPOD(t);
        }

    public: // mutators
        Type::Struct * getOrCreateStructType(Symbol name) {
            // struct types can't have aliases
            auto i = types_.find(name.name());
            if (i == types_.end()) {
                Type::Struct * result = new Type::Struct{nullptr};
                types_.insert(std::make_pair(name.name(), std::unique_ptr<Type>{result}));
                return result;
            } else {
                Type::Struct * result = dynamic_cast<Type::Struct*>(i->second.get());
                return result;
            }
        }

        Type::Class * getOrCreateClassType(Symbol name) {
            // class types can't have aliases
            auto i = types_.find(name.name());
            if (i == types_.end()) {
                auto * result = new Type::Class{nullptr};
                types_.insert(std::make_pair(name.name(), std::unique_ptr<Type>{result}));
                return result;
            } else {
                auto * result = dynamic_cast<Type::Class *>(i->second.get());
                return result;
            }
        }

        Type::Function * getOrCreateFunctionType(std::unique_ptr<Type::Function> type) {
            std::string typeName = type->toString();
            auto i = types_.find(typeName);
            if (i == types_.end())
                i = types_.insert(std::make_pair(typeName, type.release())).first;
            Type::Function * result = dynamic_cast<Type::Function*>(i->second.get());
            assert(result != nullptr && "The type existed, but was something else");
            return result;
        }

        Type::Alias * createTypeAlias(Symbol name, Type * base) {
            assert(types_.find(name.name()) == types_.end());
            Type::Alias * result = new Type::Alias(name, base);
            types_.insert(std::make_pair(name.name(), std::unique_ptr<Type>{ result }));
            return result;
        }

        /** Returns a pointer type to the given base.
         */
        Type * getOrCreatePointerType(Type * base) {
            std::string typeName = base->toString() + "*";
            auto i = types_.find(typeName);
            if (i == types_.end())
                i = types_.insert(std::make_pair(typeName, std::unique_ptr<Type>(new Type::Pointer{base}))).first;
            return i->second.get();
        }
    }; // tinycpp::TypesSpace

} // namespace tinycpp
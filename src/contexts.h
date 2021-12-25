#pragma once

#include <memory>
#include <unordered_map>

// internal
#include "shared.h"
#include "types.h"

namespace tinycpp {

    /** An information about TinyC+ program types.
     */
    class TypesContext {
    private: // data
        std::unordered_map<std::string, std::unique_ptr<Type>> types_;
        Type * int_;
        Type * double_;
        Type * char_;
        Type * void_;

    public: // constructors
        TypesContext() {
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
    }; // tinycpp::TypesContext


    /** An information about TinyC+ program names.
     */
    class NamesContext {
    private:
        class Space {
        public: // data
            Type * returnType;
            std::unordered_map<Symbol, Type *> entities = {};
            // hierarchy information
            Space * parent;
            std::vector<std::unique_ptr<Space>> children = {};
        public:
            Space(Space * parent, Type * returnType)
                :parent{parent}
                ,returnType{returnType}
            { }

            void print(ASTPrettyPrinter & printer) const {
                printer << "[";
                if (parent == nullptr) {
                    printer << "global";
                } else {
                    printer << (size_t)this;
                }
                printer << "]";
                bool isNotEmpty = entities.size() > 0 || children.size() > 0;
                printer.indent();
                if (isNotEmpty) {
                    printer.newline();
                }
                for (auto it = entities.begin(), end = entities.end(); it != end; it++) {
                    printer << " > " << it->second->toString() << " " << it->first.name();
                    printer.newline();
                }
                for (auto & spc : children) {
                    printer << " > ";
                    spc->print(printer);
                }
                printer.dedent();
                if (isNotEmpty) {
                    printer.newline();
                }
            }
        };
        Space global_;
        Space * current_;
    public:
        NamesContext(Type * globalReturnType): global_{nullptr, globalReturnType} {
            current_ = &global_;
        }

    private:
        void enterNewScope(Type * returnType) {
            auto * child = new Space{current_, returnType};
            current_->children.push_back(std::unique_ptr<Space>{child});
            assert(child->parent == current_);
            current_ = child;
        }
    public:
        void enterBlockScope() { enterNewScope(current_->returnType); }

        void enterFunctionScope(Type * returnType) { enterNewScope(returnType); }

        void leaveCurrentScope() {
            current_ = current_->parent;
        }

        bool addVariable(Symbol name, Type * type) {
            // check if the name already exists
            if (current_->entities.find(name) != current_->entities.end())
                return false;
            current_->entities.insert(std::make_pair(name, type));
            return true;
        }

        /** Returns the type of variable.
         */
        Type * getVariable(Symbol name) {
            for (auto * it = current_; it != nullptr; it = it->parent) {
                auto entity = it->entities.find(name);
                if (entity != it->entities.end()) {
                    return entity->second;
                }
            }
            return nullptr;
        }

        Type * currentScopeReturnType() {
            return current_->returnType;
        }

        void print(ASTPrettyPrinter & printer) {
            printer.newline();
            global_.print(printer);
            printer.newline();
        }
    }; // tinycpp::NamesContext

}; // namespace tinycpp
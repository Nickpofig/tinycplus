#pragma once

// standard
#include <string>
#include <variant>

// internal
#include "ast.h"
#include "types.h"
#include "contexts.h"

namespace tinycplus {

    class TypeChecker : public ASTVisitor {
    private: // data
        NamesContext & names_;
        TypesContext & types_;
        std::unordered_map<Type*, bool> typeDefinitionChecks_;
        Type::Class * currentClassType = nullptr;
        bool isProcessingPointerType = false;

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

    public: // parse error checks
        void checkTypeCompletion(Type * type, AST * ast) const {
            if (!type->isFullyDefined()) {
                throw ParserError{
                    STR("Type " << type->toString() << " is not fully defined"),
                    ast->location()
                };
            }
        }
        void updatePartialDecl(Type * type, ASTPartialDecl * ast) {
            auto & it = typeDefinitionChecks_.find(type);
            if (it == typeDefinitionChecks_.end()) {
                typeDefinitionChecks_.insert({type,ast->isDefinition});
            } else if (it->second) {
                throw ParserError{STR("Type " << type->toString() << " has already been fully defined"), ast->location()};
            } else {
                typeDefinitionChecks_[type] = ast->isDefinition;
            }
        }
        bool isDefined(Type * type) {
            auto & it = typeDefinitionChecks_.find(type);
            assert(it != typeDefinitionChecks_.end());
            return it->second;
        }
        // where T is type of AST
        template<typename T> void checkTypeCompletion(Type * type, std::unique_ptr<T> const & ast) const {
            checkTypeCompletion(type, ast.get());
        }
        void checkReturnType(Type::Function * function, Type * returnType, AST * functionAst) const {
            if (returnType != function->returnType()) {
                throw ParserError{
                    STR("Invalid function return type ("
                        << "ast:" << returnType->toString() << " != real:" << function->returnType()->toString()
                        << ") in function: " << function->toString()),
                    functionAst->location()
                };
            }
        }
        void addVariable(AST * ast, Symbol name, Type * type) {
            // std::cout << "DEBUG: addding variable (" << name.name() << ") of type (" << type->toString() << ")" << std::endl;
            if (!names_.addVariable(name, type)) {
                throw ParserError{
                    STR("Name " << name.name() << " already used"),
                    ast->location()
                };
            }
        }
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
        void visit(ASTProgram * ast) override;
        void visit(ASTVarDecl * ast) override;
        void visit(ASTFunDecl * ast) override;
        void visit(ASTFunPtrDecl * ast) override;
        void visit(ASTStructDecl * ast) override;
        void visit(ASTInterfaceDecl * ast) override;
        void visit(ASTClassDecl * ast) override;
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

        void processFunction(ASTFunDecl * ast) {
            // creates function type from ast
            std::unique_ptr<Type::Function> ftype{new Type::Function{visitChild(ast->typeDecl)}};
            checkTypeCompletion(ftype->returnType(), ast->typeDecl);
            // adds argument types
            for (auto & i : ast->args) {
                auto * argType = visitChild(i->type);
                checkTypeCompletion(argType, i);
                auto argClassType = argType->as<Type::Class>();
                if (argClassType != nullptr && argClassType->isAbstract()) throw ParserError {
                    STR("TYPECHECK: Cannot declare value type abstract class instance."),
                    ast->location()
                };
                ftype->addArgument(argType);
            }
            // registers function type
            auto * t = types_.getOrCreateFunctionType(std::move(ftype));
            try {
                addVariable(ast, ast->name.value(), t);
            } catch(std::exception e) {
                // do nothing
            }
            ast->setType(t);
            // enters the context and add all arguments as local variables
            if (ast->body) {
                names_.enterFunctionScope(t->returnType());
                {
                    for (auto & i : ast->args) {
                        names_.addVariable(i->name->name, i->type->getType());
                    }
                    // typecheck the function body
                    auto * actualReturn = visitChild(ast->body);
                    checkReturnType(t, actualReturn, ast);
                }
                // leaves the function context
                names_.leaveCurrentScope();
            }
        }

        void processConstructor(ASTFunDecl * ast) {
            auto context = pop<Context::Complex>();
            auto * classType = context->complexType->as<Type::Class>();
            // creates function type from ast
            std::unique_ptr<Type::Function> ftype{new Type::Function{visitChild(ast->typeDecl)}};
            checkTypeCompletion(ftype->returnType(), ast->typeDecl);
            auto * parsedClassType = ftype->returnType()->as<Type::Class>();
            if (parsedClassType != classType) throw ParserError{
                STR("Typechecking. Class constructor. Supplied type name is not (" << classType->toString() << ") class"),
                ast->location(),
            };
            if (ast->base.has_value()) {
                auto baseClassType = classType->getBase();
                auto parsedBaseClassType = types_.getType(ast->base->getName())->as<Type::Class>();
                if (baseClassType != parsedBaseClassType) throw ParserError{
                    STR("Typechecking. Class constructor."
                        << "Incorrect base class, expected (" << baseClassType->toString()
                        << ") but got (" << parsedBaseClassType->toString() <<") class"),
                    ast->location(),
                };
            }
            // adds argument types
            std::unordered_map<Symbol, Type*> argTypeMap;
            for (auto & i : ast->args) {
                auto * argType = visitChild(i->type);
                argTypeMap.insert({i->name->name, argType});
                checkTypeCompletion(argType, i);
                auto argClassType = argType->as<Type::Class>();
                if (argClassType != nullptr && argClassType->isAbstract()) throw ParserError {
                    STR("TYPECHECK: Cannot declare value type abstract class instance."),
                    ast->location()
                };
                ftype->addArgument(argType);
            }
            // registers function type
            auto * t = types_.getOrCreateFunctionType(std::move(ftype));
            if (ast->base.has_value()) {
                auto baseClassType = classType->getBase();
                std::unique_ptr<Type::Function> baseConstructorFunction {new Type::Function {baseClassType}};
                for (auto & it : ast->base->args) {
                    auto found = argTypeMap.find(it->name);
                    if (found == argTypeMap.end()) throw ParserError{
                        STR("TYPECHECK: Unknwon base constructor argument passed."),
                        ast->location(),
                    };
                    baseConstructorFunction->addArgument(found->second);
                }
                auto baseConstructorType = types_.getOrCreateFunctionType(std::move(baseConstructorFunction));
                if (!baseClassType->hasConstructor(baseConstructorType)) throw ParserError{
                    STR("TYPECHECK: No costructor for given types found in base class"),
                    ast->location(),
                };
                ast->base->name->setType(baseConstructorType);
            }
            try {
                addVariable(ast, ast->name.value(), t);
            } catch(std::exception e) {
                // do nothing
            }
            ast->setType(t);
            classType->addConstructorFunction(t);
            // enters the context and add all arguments as local variables
            if (ast->body) {
                names_.enterFunctionScope(t->returnType());
                {
                    names_.addVariable(symbols::KwThis, types_.getOrCreatePointerType(classType));
                    if (auto * base = classType->getBase()) {
                        names_.addVariable(symbols::KwBase, types_.getOrCreatePointerType(classType->getBase()));
                    }
                    for (auto & i : ast->args) {
                        names_.addVariable(i->name->name, i->type->getType());
                    }
                    // typecheck the function body
                    auto * actualReturn = visitChild(ast->body);
                    if (actualReturn != types_.getTypeVoid()) throw ParserError{
                        STR("Constructor: " << t->toString() << ", must not return anything, but returns: " << actualReturn->toString()),
                        ast->location()
                    };
                }
                // leaves the function context
                names_.leaveCurrentScope();
            }
        }

        void processMethod(ASTFunDecl * ast) {
            // gets context information
            auto context = pop<Context::Complex>();
            auto methodName = ast->name.value();
            auto * classType = context->complexType->as<Type::Class>();
            assert(classType != nullptr);
            // creates function type
            std::unique_ptr<Type::Function> ftype{new Type::Function{visitChild(ast->typeDecl)}};
            checkTypeCompletion(ftype->returnType(), ast->typeDecl);
            // adds argument types
            auto * targetType = types_.getOrCreatePointerType(classType);
            ftype->addArgument(targetType);
            for (auto & i : ast->args) {
                auto * argType = visitChild(i->type);
                checkTypeCompletion(argType, i->type);
                auto argClassType = argType->as<Type::Class>();
                if (argClassType != nullptr && argClassType->isAbstract()) throw ParserError {
                    STR("TYPECHECK: Cannot declare value type abstract class instance."),
                    ast->location()
                };
                ftype->addArgument(argType);
            }
            // registers function type
            auto * functionType = types_.getOrCreateFunctionType(std::move(ftype));
            ast->setType(functionType);
            // registers self as member of the class
            types_.addMethodToClass(ast, classType);
            if (ast->body) {
                // enters the context and add all arguments as local variables
                names_.enterFunctionScope(functionType->returnType());
                {
                    names_.addVariable(symbols::KwThis, types_.getOrCreatePointerType(classType));
                    if (auto * base = classType->getBase()) {
                        names_.addVariable(symbols::KwBase, types_.getOrCreatePointerType(classType->getBase()));
                    }
                    for (auto & i : ast->args) {
                        names_.addVariable(i->name->name, i->type->getType());
                    }
                    // typecheck the method body
                    auto * actualReturn = visitChild(ast->body);
                    checkReturnType(functionType, actualReturn, ast);
                }
                // leaves the method context
                names_.leaveCurrentScope();
            }
        }

        void processInterfaceMethod(ASTFunDecl * ast) {
            // gets context information
            auto context = pop<Context::Complex>();
            auto methodName = ast->name.value();
            auto * interfaceType = context->complexType->as<Type::Interface>();
            assert(interfaceType != nullptr);
            // creates function type
            std::unique_ptr<Type::Function> ftype{new Type::Function{visitChild(ast->typeDecl)}};
            checkTypeCompletion(ftype->returnType(), ast->typeDecl);
            // adds argument types
            ftype->addArgument(types_.getOrCreatePointerType(types_.getTypeVoid()));
            for (auto & i : ast->args) {
                auto * argType = visitChild(i->type);
                checkTypeCompletion(argType, i->type);
                auto argClassType = argType->as<Type::Class>();
                if (argClassType != nullptr && argClassType->isAbstract()) throw ParserError {
                    STR("TYPECHECK: Cannot declare value type abstract class instance."),
                    ast->location()
                };
                ftype->addArgument(argType);
            }
            // registers function type
            auto * functionType = types_.getOrCreateFunctionType(std::move(ftype));
            ast->setType(functionType);
            // registers self as member of the interface
            interfaceType->registerField(methodName, types_.getOrCreatePointerType(functionType), ast);
        }
    }; // tinyc::TypeChecker

} // namespace tinyc
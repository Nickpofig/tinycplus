// internal
#include "typechecker.h"
#include "shared.h"

namespace tinycplus {

    TypeChecker::TypeChecker(TypesContext & space, NamesContext & names)
        :types_{space}
        ,names_{names}
    { }

    Type * TypeChecker::getArithmeticResult(Type * lhs, Type * rhs) const {
        if (lhs == types_.getTypeDouble() && (rhs == types_.getTypeInt() || rhs == types_.getTypeChar() || rhs == types_.getTypeDouble()))
            return types_.getTypeDouble();
        if (rhs == types_.getTypeDouble() && (lhs == types_.getTypeInt() || lhs == types_.getTypeChar() || lhs == types_.getTypeDouble()))
            return types_.getTypeDouble();
        if (lhs == types_.getTypeInt() && (rhs == types_.getTypeChar() || rhs == types_.getTypeInt()))
            return types_.getTypeInt();
        if (rhs == types_.getTypeInt() && (lhs == types_.getTypeChar() || lhs == types_.getTypeInt()))
            return types_.getTypeInt();
        if (lhs == types_.getTypeChar() && types_.getTypeChar())
            return types_.getTypeChar();
        return nullptr;
    }

    Type::Function const * TypeChecker::asFunctionType(Type * t) {
        Type::Pointer const * p = dynamic_cast<Type::Pointer const *>(t);
        if (p != nullptr)
            t = p->base();
        return dynamic_cast<Type::Function const *>(t);
    }

    void TypeChecker::visit(AST * ast) { 
        visitChild(ast);
    }

    void TypeChecker::visit(ASTInteger * ast) { 
        return ast->setType(types_.getTypeInt());
    }

    void TypeChecker::visit(ASTDouble * ast) { 
        return ast->setType(types_.getTypeDouble());
    }

    void TypeChecker::visit(ASTChar * ast) { 
        return ast->setType(types_.getTypeChar());
    }

    void TypeChecker::visit(ASTString * ast) { 
        return ast->setType(types_.getOrCreatePointerType(types_.getTypeChar()));
    }

    void TypeChecker::visit(ASTIdentifier * ast) {
        if (auto context = pop<Context::Member>(); context.has_value()) {
            auto baseType = context.value().memberBaseType;
            auto * t = baseType->getMemberType(ast->name);
            return ast->setType(t);
        } else {
            auto * t = names_.getVariable(ast->name);
            if (t == nullptr) {
                throw ParserError(STR("Unknown variable " << ast->name.name()), ast->location());
            }
            return ast->setType(t);
        }
    }

    void TypeChecker::visit(ASTType * ast) {
        // unreachable
    }

    void TypeChecker::visit(ASTPointerType * ast) {
        isProcessingPointerType = true;
        auto * baseType = visitChild(ast->base);
        isProcessingPointerType = false;
        return ast->setType(types_.getOrCreatePointerType(baseType));
    }

    void TypeChecker::visit(ASTArrayType * ast) {
        // treat array as pointer
        isProcessingPointerType = true;
        auto * baseType = visitChild(ast->base);
        isProcessingPointerType = false;
        return ast->setType(types_.getOrCreatePointerType(baseType));
    }

    void TypeChecker::visit(ASTNamedType * ast) {
        auto * type = types_.getType(ast->name);
        if (!isProcessingPointerType && currentClassType == nullptr) {
            if (type == types_.defaultClassType) throw ParserError {
                STR("TYPECHECK: default object type can be used only as pointer type!"),
                ast->location()
            };
            if (type->as<Type::Interface>()) throw ParserError {
                STR("TYPECHECK: interface type can be used only as pointer type!"),
                ast->location()
            };
        }
        return ast->setType(type);
    }

    void TypeChecker::visit(ASTSequence * ast) { 
        Type * t = nullptr;
        for (auto & i : ast->body)
            t = visitChild(i);
        return ast->setType(t);
    }

    void TypeChecker::visit(ASTBlock * ast) { 
        names_.enterBlockScope();
        // Sets default result type of a block as void if non other type has been set
        auto * returnType = types_.getTypeVoid();
        for (auto & i : ast->body) {
            auto * statementResultType = visitChild(i);
            if (dynamic_cast<ASTReturn*>(i.get())) {
                returnType = statementResultType;
            }
        }
        ast->setType(returnType);
        names_.leaveCurrentScope();
    }

    void TypeChecker::visit(ASTProgram * ast) {
        names_.enterBlockScope();
        names_.addGlobalVariable(symbols::KwNull, types_.getTypeDefaultClassPtr());
        // Sets default result type of a block as void if non other type has been set
        for (auto & i : ast->body) {
            auto * statementResultType = visitChild(i);
        }
        ast->setType(types_.getTypeVoid());
        names_.leaveCurrentScope();
    }

    void TypeChecker::visit(ASTVarDecl * ast) {
        auto * t = visitChild(ast->type);
        checkTypeCompletion(t, ast);
        if (ast->value != nullptr) {
            auto * valueType = visitChild(ast->value);
            if (valueType != t)
                throw ParserError(STR("Value of type " << valueType->toString() << " cannot be assigned to variable of type " << t->toString()), ast->location());
        }
        if (auto context = pop<Context::Complex>(); context.has_value()) {
            context.value().complexType->registerField(ast->name->name, t, ast);
        } else {
            addVariable(ast, ast->name->name, t);
        }
        return ast->setType(t);
    }




    void TypeChecker::visit(ASTFunDecl * ast) {
        if (ast->isClassMethod()) processMethod(ast);
        else if (ast->isClassConstructor()) processConstructor(ast);
        else if (ast->isInterfaceMethod()) processInterfaceMethod(ast);
        else processFunction(ast);
    }




    /** Type checking a structure declaration creates the type.
     */
    void TypeChecker::visit(ASTStructDecl * ast) { 
        Type::Struct * type = types_.getOrCreateStructType(ast->name);
        if (type == nullptr) {
            throw ParserError{STR("Type " << ast->name.name() << " already declared and it is not a struct"), ast->location()};
        }
        updatePartialDecl(type, ast);
        ast->setType(type);
        if (ast->isDefinition) {
            for (auto & i : ast->fields) {
                auto position = push<Context::Complex>({type});
                visitChild(i);
                wipeContext(position);
            }
        }
    }




    void TypeChecker::visit(ASTInterfaceDecl * ast) {
        auto * type = types_.getOrCreateInterfaceType(ast->name);
        ast->setType(type);
        for (auto & i : ast->methods) {
            auto position = push<Context::Complex>({type});
            auto * methodType = visitChild(i)->as<Type::Function>();
            auto methodName = i->name.value();
            wipeContext(position);
            // std::cout << "DEBUG: Adding " << methodName << "(" << methodType->toString() << ") method to interface type: " << type->toString() << std::endl;
            types_.addMethodToInterface(i.get(), type);
        }
    }




    /** Type checking a structure declaration creates the type.
     */
    void TypeChecker::visit(ASTClassDecl * ast) {
        auto * type = types_.getOrCreateClassType(ast->name);
        currentClassType = type;
        // adding default constructor function type
        types_.getOrCreateFunctionType(std::unique_ptr<Type::Function>{new Type::Function{type}});
        updatePartialDecl(type, ast);
        ast->setType(type);
        Type::Class * baseType = nullptr;
        if (ast->baseClass) {
            baseType = visitChild(ast->baseClass)->as<Type::Class>();
            assert(baseType != nullptr);
            if (!isDefined(baseType)) throw ParserError{
                STR("[T2] A base type must be fully defined before inherited."),
                ast->location()
            };
            type->setBase(baseType);
        } else {
            type->setBase(types_.defaultClassType);
        }
        if (ast->isDefinition) {
            for (auto & it : ast->interfaces) {
                auto * interfaceType = visitChild(it)->as<Type::Interface>();
                type->addInterfaceType(interfaceType);
            }

            for (auto & i : ast->fields) {
                auto position = push<Context::Complex>({type});
                visitChild(i);
                wipeContext(position);
            }
            for (auto & i : ast->methods) {
                auto position = push<Context::Complex>({type});
                visitChild(i);
                wipeContext(position);
            }
            // checks constructor reuse of base class constructors
            bool baseConstructorIsUsed = false;
            for (auto & i : ast->constructors) {
                auto position = push<Context::Complex>({type});
                visitChild(i);
                wipeContext(position);
                baseConstructorIsUsed |= i->base.has_value();
                // if (baseConstructorIsUsed && baseType == nullptr) throw ParserError{
                //     STR("TYPECHECK: no suitable base constructor was found."),
                //     ast->location()
                // };
            }

            // removes default constructor from usage
            if (ast->constructors.size() > 0 && !type->hasOverridedDefaultConstructor()) {
                type->constructors.erase(type->defaultConstructorFuncType);
            }

            if (baseType != nullptr) {
                bool baseHasConstructor = baseType->hasExplicitConstructros();
                if (baseHasConstructor && !baseConstructorIsUsed) throw ParserError{
                    STR("TYPECHECK: at least one base constructor must be used."),
                    ast->location()
                };
                if (baseHasConstructor && !type->hasExplicitConstructros()) throw ParserError{
                    STR("TYPECHECK: base constructor must be implemented."),
                    ast->location()
                };
            }
        }
        currentClassType = nullptr;
    }

    /** Typechecking a function pointer declaration creates the type.
        But here is a catch - we can have multiple definitions of the same function type, each time under a different name. In fact we can have a type definition and then a function of that type. These must not be different types, for which the type alias class is needed.
     */
    void TypeChecker::visit(ASTFunPtrDecl * ast) { 
        // check if type with given name already exists
        if (types_.getType(ast->name->name) != nullptr)
            throw ParserError{STR("Type " << ast->name->name.name() << " already exists"), ast->location()};
        // first get the function type
        std::unique_ptr<Type::Function> ftype{new Type::Function{visitChild(ast->returnType)}};
        if (! ftype->returnType()->isFullyDefined())
            throw ParserError{STR("Return type " << ftype->returnType()->toString() << " is not fully defined"), ast->returnType->location()};
        // typecheck all arguments, make sure the argument types are fully defined and add the arguments as local variables
        for (auto & i : ast->args) {
            auto * argType = visitChild(i);
            if (!argType->isFullyDefined())
                throw ParserError(STR("Type " << argType->toString() << " is not fully defined"), i->location());
            ftype->addArgument(argType);
        }
        // now get the proper type and create type alias
        Type * fptr = types_.getOrCreatePointerType(types_.getOrCreateFunctionType(std::move(ftype)));
        return ast->setType(types_.createTypeAlias(ast->name->name, fptr));
    }

    void TypeChecker::visit(ASTIf * ast) { 
        if (! types_.convertsToBool(visitChild(ast->cond)))
            throw ParserError{STR("Condition must convert to bool, but " << ast->cond->getType()->toString() << " found"), ast->cond->location()};
        visitChild(ast->trueCase);
        if (ast->falseCase != nullptr) 
            visitChild(ast->falseCase);
        return ast->setType(types_.getTypeVoid());
    }

    void TypeChecker::visit(ASTSwitch * ast) { 
        if (! types_.convertsToBool(visitChild(ast->cond)))
            throw ParserError{STR("Condition must convert to bool, but " << ast->cond->getType()->toString() << " found"), ast->cond->location()};
        if (ast->defaultCase != nullptr)
            visitChild(ast->defaultCase);
        for (auto & i : ast->cases)
            visitChild(i.second);
        return ast->setType(types_.getTypeVoid());
    }

    void TypeChecker::visit(ASTWhile * ast) {
        if (! types_.convertsToBool(visitChild(ast->cond)))
            throw ParserError{STR("Condition must convert to bool, but " << ast->cond->getType()->toString() << " found"), ast->cond->location()};
        visitChild(ast->body);
        return ast->setType(types_.getTypeVoid());
    }

    void TypeChecker::visit(ASTDoWhile * ast) { 
        if (! types_.convertsToBool(visitChild(ast->cond)))
            throw ParserError{STR("Condition must convert to bool, but " << ast->cond->getType()->toString() << " found"), ast->cond->location()};
        visitChild(ast->body);
        return ast->setType(types_.getTypeVoid());
    }

    void TypeChecker::visit(ASTFor * ast) { 
        visitChild(ast->init);
        if (! types_.convertsToBool(visitChild(ast->cond)))
            throw ParserError{STR("Condition must convert to bool, but " << ast->cond->getType()->toString() << " found"), ast->cond->location()};
        visitChild(ast->increment);
        visitChild(ast->body);
        return ast->setType(types_.getTypeVoid());
    }

    void TypeChecker::visit(ASTBreak * ast) { 
        return ast->setType(types_.getTypeVoid());
    }

    void TypeChecker::visit(ASTContinue * ast) { 
        return ast->setType(types_.getTypeVoid());
    }

    void TypeChecker::visit(ASTReturn * ast) {
        Type * type = nullptr;
        if (ast->value == nullptr)
            type = types_.getTypeVoid();
        else
            type = visitChild(ast->value);
        if (type != names_.currentScopeReturnType())
            throw ParserError{
                STR("Invalid return type, expected " << names_.currentScopeReturnType()->toString() 
                    << ", but " << type->toString() << " found"),
                ast->location()
            };
        return ast->setType(type);
    }

    void TypeChecker::visit(ASTBinaryOp * ast) {
        auto * leftType = visitChild(ast->left);
        auto * rightType = visitChild(ast->right);
        Type * t = nullptr;
        // works on all PODs and pointers
        if (ast->op == Symbol::Add || ast->op == Symbol::Sub) {
            if (types_.isPointer(leftType) && rightType == types_.getTypeInt())
                t = leftType;
            else
                t = getArithmeticResult(leftType, rightType);
        // works on all PODs
        } else if (ast->op == Symbol::Mul || ast->op == Symbol::Div) {
            t = getArithmeticResult(leftType, rightType);
        // works on char and int, result is the type itself, the types must be identical
        } else if (ast->op == Symbol::Mod) {
            if (leftType == rightType && (leftType == types_.getTypeInt() || leftType == types_.getTypeChar()))
                t = leftType;
        // lhs must be char or int, rhs can be char or int, result is lhs
        } else if (ast->op == Symbol::ShiftRight || ast->op == Symbol::ShiftLeft) {
            if ((leftType == types_.getTypeInt() || leftType == types_.getTypeChar()) &&
                (rightType == types_.getTypeInt() || rightType == types_.getTypeChar()))
                t = leftType;
        // works on types convertible to boolean and result is integer (we do not have bool)
        } else if (ast->op == Symbol::And || ast->op == Symbol::Or || ast->op == Symbol::Xor) {
            if (types_.convertsToBool(leftType) && types_.convertsToBool(rightType))
                t = types_.getTypeInt();
        // bitwise and and or operations only work on chars and its, result is the same type
        } else if (ast->op == Symbol::BitAnd || ast->op == Symbol::BitOr) {
            if (leftType == rightType && (leftType == types_.getTypeInt() || leftType == types_.getTypeChar()))
                t = leftType;
        // relational operators work on int, chars,doubles and pointers of same type result is int
        } else if (ast->op == Symbol::Lt || ast->op == Symbol::Gt || ast->op == Symbol::Lte || ast->op == Symbol::Gte) {
            if (leftType == rightType && (types_.isPointer(leftType) || types_.isPOD(leftType)))
                t = types_.getTypeInt();
        // equality and inequality work on all types as long as they match
        } else if(ast->op == Symbol::Eq || ast->op == Symbol::NEq) {
            if (leftType == rightType)
                t = types_.getTypeInt();
        }
        return ast->setType(t);
    }

    void TypeChecker::visit(ASTAssignment * ast) { 
        auto * lvalueType = visitChild(ast->lvalue);
        auto * valueType = visitChild(ast->value);
        if (!ast->lvalue->hasAddress())
            throw ParserError{"Assignment target must have address", ast->location()};
        // we are working with exact types only
        if (lvalueType == valueType)
            return ast->setType(lvalueType);
        else
            return ast->setType(nullptr); // error
    }

    void TypeChecker::visit(ASTUnaryOp * ast) { 
        auto * argt = visitChild(ast->arg);
        Type * t = nullptr;
        // works on all numeric types
        if (ast->op == Symbol::Add || ast->op == Symbol::Sub) {
            t = getArithmeticResult(argt, argt);
        // negation works on char and int only
        } else if (ast->op == Symbol::Neg) {
            if (argt == types_.getTypeInt() || argt == types_.getTypeChar())
                t = argt;
        // not works on any type convertible to boolean
        } else if (ast->op == Symbol::Not) {
            if (types_.convertsToBool(argt))
                t = types_.getTypeInt();
        // works on pointers and arithmetic types
        } else if (ast->op == Symbol::Inc || ast->op == Symbol::Dec) {
            if (!ast->arg->hasAddress())
                throw ParserError("Cannot increment or decrement non l-value", ast->location());
            // not all types can be incremented
            if (types_.isPointer(argt) || types_.isPOD(argt))
                t = argt;
        }
        return ast->setType(t);
    }

    /** For increment and decrement, the result type is identical to its argument, but not all types can be incremented.
     */
    void TypeChecker::visit(ASTUnaryPostOp * ast) { 
        if (!ast->arg->hasAddress())
            throw ParserError("Cannot increment or decrement non l-value", ast->location());
        auto * argt = visitChild(ast->arg);
        // not all types can be incremented
        if (types_.isPointer(argt) || types_.isPOD(argt))
            return ast->setType(argt);
        else
            return ast->setType(nullptr); // error
    }

    /** The interesting feature of the address operator is that not every value in tinyC has an address (only local variables do)
     */
    void TypeChecker::visit(ASTAddress * ast) { 
        auto * t = visitChild(ast->target);
        if (!ast->target->hasAddress())
            throw ParserError("Address can only be taken from a non-temporary value (l-value)", ast->location());
        return ast->setType(types_.getOrCreatePointerType(t));
    }

    void TypeChecker::visit(ASTDeref * ast) { 
        auto * t = visitChild(ast->target);
        auto * p = dynamic_cast<Type::Pointer const *>(t);
        if (p == nullptr)
            throw ParserError{STR("Cannot dereference a non-pointer type " << t->toString()), ast->location()};
        return ast->setType(p->base());
    }

    void TypeChecker::visit(ASTIndex * ast) { 
        auto * baseType = visitChild(ast->base);
        if (! types_.isPointer(baseType))
            throw ParserError{STR("Expected pointer, but " << baseType->toString() << " found"), ast->location()};
        auto * indexType = visitChild(ast->index);
        if (indexType != types_.getTypeInt() && indexType != types_.getTypeChar())
            throw ParserError{STR("Expected int or char, but " << indexType->toString() << " found"), ast->location()};
        return ast->setType(dynamic_cast<Type::Pointer const *>(baseType)->base());
    }

    void TypeChecker::visit(ASTMember * ast) {
        auto * baseType = visitChild(ast->base);
        auto position = push<Context::Member>({baseType->unwrap<Type::Complex>()});
        auto memberType = visitChild(ast->member);
        if (memberType == nullptr) {
            throw ParserError{
                STR("Unknwon expression to access member of type \"" << baseType->toString() << "\""),
                ast->location()
            };
        } else if (ast->op == Symbol::ArrowR && !baseType->isPointer()) {
            throw ParserError{
                STR("Expected pointer type, but target type is \"" << baseType->toString() << "\""),
                ast->base->location()
            };
        } else if (ast->op == Symbol::Dot && baseType->isPointer()) {
            throw ParserError{
                STR("Expected value type, but target type is \"" << baseType->toString() << "\""),
                ast->base->location()
            };
        }
        if (auto * memberAsIdent = ast->member->as<ASTIdentifier>()) {
            auto memberName = memberAsIdent->name;
            if (auto * classType = baseType->unwrap<Type::Class>()) {
                Type::Class * originClassType = nullptr;
                auto access = classType->getMemberAccessMod(memberName, &originClassType);
                switch (access)
                {
                case AccessMod::Private:
                    if (currentClassType == nullptr || currentClassType != originClassType) throw ParserError {
                        STR("TYPECHECK: cant access private memeber: " << memberName),
                        ast->member->location()
                    };
                    break;
                case AccessMod::Protected:
                    // std::cout << "DEBUG: Protected origin class: " << (originClassType != nullptr ? originClassType->toString() : "null") << std::endl;
                    if (currentClassType == nullptr || !currentClassType->inherits(originClassType)) throw ParserError {
                        STR("TYPECHECK: cant access protected memeber: " << memberName),
                        ast->member->location()
                    };
                    break;
                }
            }
        }
        ast->setType(memberType);
        wipeContext(position);
    }

    void TypeChecker::visit(ASTCall * ast) {
        // std::cout << "DEBUG: typechicking call " << std::endl;
        int methodOffset = 0;
        auto context = pop<Context::Member>();
        if (context.has_value()) {
            methodOffset = 1; // both class and interface methods has an implicit first argument.
            if (auto * classType = context->memberBaseType->as<Type::Class>()) {
                auto * ident = dynamic_cast<ASTIdentifier*>(ast->function.get());
                auto methodInfo = classType->getMethodInfo(ident->name);
                if (!methodInfo.has_value()) throw ParserError {
                    STR("TYPECHECK: method (" << ident->name << ") was not found for class: " << classType->name), ast->location(), false
                };
                ident->setType(methodInfo.value().type);
            } else if (auto * interfaceType = context->memberBaseType->as<Type::Interface>()) {
                auto * ident = dynamic_cast<ASTIdentifier*>(ast->function.get());
                auto methodType = interfaceType->getMethod(ident->name);
                // for (auto & it : interfaceType->methods_) {
                //     std::cout << "DEBUG: interface (" << interfaceType->name << ") has method: " << it.first << std::endl;
                // }
                // types_.printAllTypes();
                if (!methodType.has_value()) throw ParserError {
                    STR("TYPECHECK: method [" << ident->name << "] was not found for interface: " << interfaceType->name), ast->location(), false
                };
                ident->setType(methodType.value());
            }
        } else {
            Type::Class * classType = nullptr; // constructor call
            if (auto * namedTypeAst = ast->function->as<ASTNamedType>()) {
                classType = types_.getType(namedTypeAst->name)->as<Type::Class>();
            }
            if (classType != nullptr) {
                std::vector<Type*> argTypes;
                for (size_t i = 0; i < ast->args.size(); ++i) {
                    auto * argType = visitChild(ast->args[i]);
                    argTypes.push_back(argType);
                }
                Type::Function * constructorFunction = nullptr;
                if (classType->constructors.size() == 0) {
                    std::unique_ptr<Type::Function> func{new Type::Function{classType}};
                    constructorFunction = types_.getOrCreateFunctionType(std::move(func));
                }
                if (constructorFunction == nullptr) {
                    for (auto & it : classType->constructors) {
                        bool argsAreEqual = true;
                        auto funcType = it.first;
                        if (argTypes.size() != funcType->numArgs()) continue;
                        for (size_t i = 0; i < funcType->numArgs(); i++) {
                            auto * argType = funcType->argType(i);
                            if (argType != argTypes[i]) {
                                argsAreEqual = false;
                                break;
                            }
                        }
                        if (argsAreEqual) { 
                            constructorFunction = funcType;
                            break;
                        }
                    }
                }
                if (constructorFunction == nullptr) {
                    throw ParserError {
                        STR("TYPECHECK: no matching constructor of class (" << classType->toString() << ") was found."),
                        ast->location()
                    };
                }
                ast->function->setType(constructorFunction);
                return ast->setType(classType);
            }

            visitChild(ast->function);
        }

        Type::Function const * f = asFunctionType(ast->function->getType());
        if (f == nullptr) {
            throw ParserError {
                STR("TYPECHECK: expected function, but value of " << ast->function->getType()->toString() << " found"), ast->location()
            };
        }
        if (ast->args.size() != f->numArgs() - methodOffset) throw ParserError {
            STR("TYPECHECK: function of type " << f->toString() << " requires "
                << f->numArgs() - methodOffset
                << " arguments, but " << ast->args.size() << " given"),
            ast->location()
        };
        for (size_t i = 0; i < ast->args.size(); ++i) {
            auto * argType = visitChild(ast->args[i]);
            auto * expectedArgType = f->argType(i + methodOffset);
            if (argType != expectedArgType) {
                throw ParserError{
                    STR("TYPECHECK: type " << expectedArgType->toString() << " expected for argument " << (i + 1)
                        << ", but " << argType->toString() << " found"),
                    ast->args[i]->location()
                };
            }
        }
        return ast->setType(f->returnType());
    }

    /** Makes sure that incompatible types are not casted.
        The following casts are legal in tinyC:
        - any pointer to any other pointer
        - any pod to any pod
        - integer to any pointer and any pointer to integer
     */
    void TypeChecker::visit(ASTCast * ast) {
        Type * valueType = visitChild(ast->value);
        Type * castType = visitChild(ast->type);
        Type * t = nullptr;
        if (types_.isPointer(castType)) {
            auto * cIntr = castType->unwrap<Type::Interface>();
            auto * cClass = castType->unwrap<Type::Class>();
            if (cIntr != nullptr || cClass != nullptr) {
                auto * vIntr = castType->unwrap<Type::Interface>();
                auto * vClass = castType->unwrap<Type::Class>();
                if (types_.isPointer(valueType) && (vIntr != nullptr || vClass != nullptr)) {
                    t = castType;
                }
            } else if (types_.isPointer(valueType) || valueType == types_.getTypeInt()) {
                t = castType;
            }
        } else if (castType == types_.getTypeInt()) {
            if (types_.isPointer(valueType) || types_.isPOD(valueType)) {
                t = castType;
            }
        } else if (types_.isPOD(castType) && types_.isPOD(valueType)) {
            t = castType;
        } else throw ParserError {
            STR("TYPECHECK: invalid cast type or value type"),
            ast->location()
        };
        return ast->setType(t);
    }

} // namespace tinycplus

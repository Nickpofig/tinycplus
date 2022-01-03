// internal
#include "typechecker.h"
#include "shared.h"

namespace tinycpp {

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
        return ast->setType(types_.getOrCreatePointerType(visitChild(ast->base)));
    }

    void TypeChecker::visit(ASTArrayType * ast) {
        // treat array as pointer
        return ast->setType(types_.getOrCreatePointerType(visitChild(ast->base)));
    }

    void TypeChecker::visit(ASTNamedType * ast) { 
        return ast->setType(types_.getType(ast->name));
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

    void TypeChecker::visit(ASTVarDecl * ast) {
        auto * t = visitChild(ast->type);
        checkTypeCompletion(t, ast);
        if (ast->value != nullptr) {
            auto * valueType = visitChild(ast->value);
            if (valueType != t)
                throw ParserError(STR("Value of type " << valueType->toString() << " cannot be assigned to variable of type " << t->toString()), ast->location());
        }
        if (auto context = pop<Context::Complex>(); context.has_value()) {
            context.value().complexType->registerMember(ast->name->name, t, ast);
        } else {
            addVariable(ast, ast->name->name, t);
        }
        return ast->setType(t);
    }

    void TypeChecker::visit(ASTFunDecl * ast) {
        // creates function type from ast
        std::unique_ptr<Type::Function> ftype{new Type::Function{visitChild(ast->typeDecl)}};
        checkTypeCompletion(ftype->returnType(), ast->typeDecl);
        // adds argument types
        for (auto & i : ast->args) {
            auto * argType = visitChild(i->type);
            checkTypeCompletion(argType, i);
            ftype->addArgument(argType);
        }
        // registers function type
        auto * t = types_.getOrCreateFunctionType(std::move(ftype));
        try {
            addVariable(ast, ast->name, t);
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


    void TypeChecker::visit(ASTMethodDecl * ast) {
        // creates function type
        auto context = pop<Context::Complex>();
        auto * classType = context->complexType->as<Type::Class>();
        std::unique_ptr<Type::Function> ftype{new Type::Function{visitChild(ast->typeDecl)}};
        checkTypeCompletion(ftype->returnType(), ast->typeDecl);
        // adds argument types
        ftype->addArgument(types_.getOrCreatePointerType(classType));
        for (auto & i : ast->args) {
            auto * argType = visitChild(i->type);
            checkTypeCompletion(argType, i->type);
            ftype->addArgument(argType);
        }
        // registers function type
        auto * t = types_.getOrCreateFunctionType(std::move(ftype));
        ast->setType(t);
        auto methodName = ast->name;
        // registers self as member of the class
        if (ast->isVirtual()) {
            if (!classType->hasOwnVirtualTable()) {
                auto vtableName = Symbol{STR("__tinycpp__" << classType->toString() << "__vtable__")};
                classType->setVirtualTable(types_.getOrCreateVTable(vtableName));
            }
            auto * vtable = classType->getVirtualTable();
            auto vtableMemberName = Symbol{STR("__tinycpp__" << classType->toString() << "__vtable__" << methodName.name())};
            auto * vtableMemberType = types_.createTypeAlias(vtableMemberName, types_.getOrCreatePointerType(t));
            if (ast->isOverride()) {
                vtable->overrideMember(methodName, vtableMemberType, ast);
            } else {
                vtable->registerMember(methodName, vtableMemberType, ast);
            }
        }
        if (ast->body) {
            classType->registerMember(methodName, t, ast);
            // enters the context and add all arguments as local variables
            names_.enterFunctionScope(t->returnType());
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
                checkReturnType(t, actualReturn, ast);
            }
            // leaves the method context
            names_.leaveCurrentScope();
        } else {
            classType->registerMember(methodName, t, ast);
        }
    }

    /** Type checking a structure declaration creates the type.
     */
    void TypeChecker::visit(ASTStructDecl * ast) { 
        Type::Struct * type = types_.getOrCreateStructType(ast->name);
        if (type == nullptr) {
            throw ParserError{STR("Type " << ast->name.name() << " already defined and is not a struct"), ast->location()};
        }
        if (type->isFullyDefined()) {
            throw ParserError{STR("Type " << ast->name.name() << " already fully defined"), ast->location()};
        }
        ast->setType(type);
        type->updateDefinition(ast);
        if (ast->isDefinition) {
            for (auto & i : ast->fields) {
                auto position = push<Context::Complex>({type});
                visitChild(i);
                wipeContext(position);
            }
        }
    }

    /** Type checking a structure declaration creates the type.
     */
    void TypeChecker::visit(ASTClassDecl * ast) {
        auto * type = types_.getOrCreateClassType(ast->name);
        if (type == nullptr) {
            throw ParserError{STR("Type " << ast->name.name() << " already defined and is not a class"), ast->location()};
        }
        if (type->isFullyDefined()) {
            throw ParserError{STR("Type " << ast->name.name() << " already fully defined"), ast->location()};
        }
        ast->setType(type);
        if (ast->baseClass) {
            auto * baseType = visitChild(ast->baseClass)->as<Type::Class>();
            assert(baseType != nullptr);
            type->setBase(baseType);
        }
        type->updateDefinition(ast);
        if (ast->isDefinition) {
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
        }
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
        auto position = push<Context::Member>({baseType->getCore<Type::Complex>()});
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
        ast->setType(memberType);
        wipeContext(position);
    }

    void TypeChecker::visit(ASTCall * ast) {
        int methodOffset = 0;
        auto context = pop<Context::Member>();
        if (context.has_value() && context->memberBaseType->getCore<Type::Class>()) {
            methodOffset = 1;
            auto * ident = dynamic_cast<ASTIdentifier*>(ast->function.get());
            auto * methodType = context.value().memberBaseType->getMemberType(ident->name);
            ident->setType(methodType);
        } else {
            if (context.has_value()) push(context.value());
            visitChild(ast->function);
        }
        Type::Function const * f = asFunctionType(ast->function->getType());
        if (f == nullptr)
            throw ParserError{STR("Expected function, but value of " << ast->function->getType()->toString() << " found"), ast->location()};
        if (ast->args.size() != f->numArgs() - methodOffset) {
            throw ParserError{
                STR("Function of type " << f->toString() << " requires "
                    << f->numArgs() - methodOffset
                    << " arguments, but " << ast->args.size() << " given"),
                ast->location()
            };
        }
        for (size_t i = 0; i < ast->args.size(); ++i) {
            auto * argType = visitChild(ast->args[i]);
            auto * expectedArgType = f->argType(i + methodOffset);
            if (argType != expectedArgType) {
                throw ParserError{
                    STR("Type " << expectedArgType->toString() << " expected for argument " << (i + 1)
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
            if (types_.isPointer(valueType) || valueType == types_.getTypeInt())
                t = castType;
        } else if (castType == types_.getTypeInt()) {
            if (types_.isPointer(valueType) || types_.isPOD(valueType))
                t = castType;
        } else if (types_.isPOD(castType) && types_.isPOD(valueType))
            t = castType;
        return ast->setType(t);
    }

} // namespace tinycpp

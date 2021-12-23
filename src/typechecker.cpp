// internal
#include "typechecker.h"
#include "shared.h"

namespace tinycpp {

    TypeChecker::TypeChecker(TypesSpace & space):
        space_{space} {
        // creates the global context
        context_.push_back(Context{space_.getTypeVoid(), {}});
    }

    Type * TypeChecker::getArithmeticResult(Type * lhs, Type * rhs) const {
        if (lhs == space_.getTypeDouble() && (rhs == space_.getTypeInt() || rhs == space_.getTypeChar() || rhs == space_.getTypeDouble()))
            return space_.getTypeDouble();
        if (rhs == space_.getTypeDouble() && (lhs == space_.getTypeInt() || lhs == space_.getTypeChar() || lhs == space_.getTypeDouble()))
            return space_.getTypeDouble();
        if (lhs == space_.getTypeInt() && (rhs == space_.getTypeChar() || rhs == space_.getTypeInt()))
            return space_.getTypeInt();
        if (rhs == space_.getTypeInt() && (lhs == space_.getTypeChar() || lhs == space_.getTypeInt()))
            return space_.getTypeInt();
        if (lhs == space_.getTypeChar() && space_.getTypeChar())
            return space_.getTypeChar();
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
        return ast->setType(space_.getTypeInt());
    }

    void TypeChecker::visit(ASTDouble * ast) { 
        return ast->setType(space_.getTypeDouble());
    }

    void TypeChecker::visit(ASTChar * ast) { 
        return ast->setType(space_.getTypeChar());
    }

    void TypeChecker::visit(ASTString * ast) { 
        return ast->setType(space_.getOrCreatePointerType(space_.getTypeChar()));
    }

    void TypeChecker::visit(ASTIdentifier * ast) {
        Type * t = getVariable(ast->name);
        if (auto member = ast->findParent<ASTMember>(1); member != nullptr && ast->isDescendentOf(member->member.get())) {
            if (auto call = ast->findParent<ASTCall>(0); call == nullptr || ast->isDescendentOf(call->function.get())) {
                auto baseType = member->base->getType();
                if (auto complex = baseType->getCore<Type::Complex>()) {
                    t = complex->getMemberType(ast->name);
                }
            }
        }
        if (t == nullptr)
            throw ParserError(STR("Unknown variable " << ast->name.name()), ast->location());
        return ast->setType(t);
    }

    void TypeChecker::visit(ASTType * ast) {
        // unreachable
    }

    void TypeChecker::visit(ASTPointerType * ast) {
        return ast->setType(space_.getOrCreatePointerType(visitChild(ast->base)));
    }

    void TypeChecker::visit(ASTArrayType * ast) {
        // treat array as pointer
        return ast->setType(space_.getOrCreatePointerType(visitChild(ast->base)));
    }

    void TypeChecker::visit(ASTNamedType * ast) { 
        return ast->setType(space_.getType(ast->name));
    }

    void TypeChecker::visit(ASTSequence * ast) { 
        Type * t = nullptr;
        for (auto & i : ast->body)
            t = visitChild(i);
        return ast->setType(t);
    }

    void TypeChecker::visit(ASTBlock * ast) { 
        enterBlockContext();
        auto * t = space_.getTypeVoid();
        for (auto & i : ast->body) {
            auto * tt = visitChild(i);
            if (dynamic_cast<ASTReturn*>(i.get())) {
                t = tt;
                ast->setType(t);
            }
        }
        // Sets default result type of a block as void if non other type has been set
        if (ast->getType() == nullptr) {
            ast->setType(space_.getTypeVoid());
        }
        leaveContext();
    }

    void TypeChecker::visit(ASTVarDecl * ast) { 
        auto * t = visitChild(ast->type);
        if (!t->isFullyDefined())
            throw ParserError(STR("Type " << t->toString() << " is not fully defined yet"), ast->location());
        if (ast->value != nullptr) {
            auto * valueType = visitChild(ast->value);
            if (valueType != t)
                throw ParserError(STR("Value of type " << valueType->toString() << " cannot be assigned to variable of type " << t->toString()), ast->location());
        }
        addVariable(ast->name->name, t);
        return ast->setType(t);
    }

    void TypeChecker::visit(ASTFunDecl * ast) {
        // first get the function type
        auto * classAst = ast->findParent<ASTClassDecl>();
        std::unique_ptr<Type::Function> ftype{
            classAst == nullptr
                ? new Type::Function{visitChild(ast->typeDecl)}
                : new Type::Method{visitChild(ast->typeDecl), classAst->getType()}
        };
        if (!ftype->returnType()->isFullyDefined())
            throw ParserError{STR("Return type " << ftype->returnType()->toString() << " is not fully defined"), ast->typeDecl->location()};
        // typecheck all arguments, make sure the argument types are fully defined and add the arguments as local variables
        if (classAst != nullptr) {
            ftype->addArgument(space_.getOrCreatePointerType(classAst->getType()));
        }
        for (auto & i : ast->args) {
            auto * argType = visitChild(i->type);
            if (!argType->isFullyDefined())
                throw ParserError(STR("Type " << argType->toString() << " is not fully defined"), i->type->location());
            ftype->addArgument(argType);
        }
        // enter the context and add all arguments as local variables
        enterFunctionContext(ftype->returnType());
        if (classAst) {
            addVariable(symbols::KwThis, space_.getOrCreatePointerType(classAst->getType()));
        }
        for (auto & i : ast->args) {
            addVariable(i->name->name, i->type->getType());
        }
        // set own type as the function type itself
        auto * t = space_.getOrCreateFunctionType(std::move(ftype));
        // this is a trick to allow storing the functions in variables, we create a new global variable of the name of the function and its type. We must do this before the body typecheck so that recursive calls are possible
        auto funName = classAst
            ? Symbol{STR(classAst->name.name() << "_" << ast->name.name())}
            : ast->name;
        if (!addGlobalVariable(funName, t))
            throw ParserError{STR("Name " << ast->name.name() << " already used"), ast->location()};
        ast->setType(t);
        if (classAst) { // registering self as member of the class
            auto classType = dynamic_cast<Type::Class*>(classAst->getType());
            classType->registerMember(ast->name.name(), t, ast);
        }
        // typecheck the function body
        auto * actualReturn = visitChild(ast->body);
        if (actualReturn != t->returnType())
            throw ParserError{STR("Invalid function return type: " << actualReturn->toString()), ast->location()};
        // leave the function context
        leaveContext();
    }

    /** Type checking a structure declaration creates the type.
     */
    void TypeChecker::visit(ASTStructDecl * ast) { 
        Type::Struct * type = space_.getOrCreateStructType(ast->name);
        if (type == nullptr)
            throw ParserError{STR("Type " << ast->name.name() << " already defined and is not a struct"), ast->location()};
        if (ast->isDefinition) {
            for (auto & i : ast->fields) {
                auto * fieldType = visitChild(i->type);
                type->registerMember(i->name->name, fieldType, i->name.get());
            }
        }
        // we have to do this *after* the types so that the struct type itself remains not fully defined while typing its fields
        if (type->ast() == nullptr || ! type->ast()->isDefinition)
            type->updateDefinition(ast);
        else
            throw ParserError{STR("Type " << ast->name.name() << " already fully defined"), ast->location()};
        return ast->setType(type);
    }

    /** Type checking a structure declaration creates the type.
     */
    void TypeChecker::visit(ASTClassDecl * ast) {
        auto * type = space_.getOrCreateClassType(ast->name);
        if (type == nullptr) {
            throw ParserError{STR("Type " << ast->name.name() << " already defined and is not a class"), ast->location()};
        }
        if (type->isFullyDefined()) {
            throw ParserError{STR("Type " << ast->name.name() << " already fully defined"), ast->location()};
        }
        ast->setType(type);
        if (ast->baseClass) {
            auto * baseType = visitChild(ast->baseClass);
            type->setBase(baseType);
        }
        type->updateDefinition(ast);
        if (ast->isDefinition) {
            for (auto & i : ast->fields) {
                auto * fieldType = visitChild(i);
                type->registerMember(i->name->name, fieldType, i.get());
            }
            for (auto & i : ast->methods) {
                auto * methodType = visitChild(i); // method registers themselfs as members of class
            }
        }
    }

    /** Typechecking a function pointer declaration creates the type.
        But here is a catch - we can have multiple definitions of the same function type, each time under a different name. In fact we can have a type definition and then a function of that type. These must not be different types, for which the type alias class is needed.
     */
    void TypeChecker::visit(ASTFunPtrDecl * ast) { 
        // check if type with given name already exists
        if (space_.getType(ast->name->name) != nullptr)
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
        Type * fptr = space_.getOrCreatePointerType(space_.getOrCreateFunctionType(std::move(ftype)));
        return ast->setType(space_.createTypeAlias(ast->name->name, fptr));
    }

    void TypeChecker::visit(ASTIf * ast) { 
        if (! space_.convertsToBool(visitChild(ast->cond)))
            throw ParserError{STR("Condition must convert to bool, but " << ast->cond->getType()->toString() << " found"), ast->cond->location()};
        visitChild(ast->trueCase);
        if (ast->falseCase != nullptr) 
            visitChild(ast->falseCase);
        return ast->setType(space_.getTypeVoid());
    }

    void TypeChecker::visit(ASTSwitch * ast) { 
        if (! space_.convertsToBool(visitChild(ast->cond)))
            throw ParserError{STR("Condition must convert to bool, but " << ast->cond->getType()->toString() << " found"), ast->cond->location()};
        if (ast->defaultCase != nullptr)
            visitChild(ast->defaultCase);
        for (auto & i : ast->cases)
            visitChild(i.second);
        return ast->setType(space_.getTypeVoid());
    }

    void TypeChecker::visit(ASTWhile * ast) { 
        if (! space_.convertsToBool(visitChild(ast->cond)))
            throw ParserError{STR("Condition must convert to bool, but " << ast->cond->getType()->toString() << " found"), ast->cond->location()};
        visitChild(ast->body);
        return ast->setType(space_.getTypeVoid());
    }

    void TypeChecker::visit(ASTDoWhile * ast) { 
        if (! space_.convertsToBool(visitChild(ast->cond)))
            throw ParserError{STR("Condition must convert to bool, but " << ast->cond->getType()->toString() << " found"), ast->cond->location()};
        visitChild(ast->body);
        return ast->setType(space_.getTypeVoid());
    }

    void TypeChecker::visit(ASTFor * ast) { 
        visitChild(ast->init);
        if (! space_.convertsToBool(visitChild(ast->cond)))
            throw ParserError{STR("Condition must convert to bool, but " << ast->cond->getType()->toString() << " found"), ast->cond->location()};
        visitChild(ast->increment);
        visitChild(ast->body);
        return ast->setType(space_.getTypeVoid());
    }

    void TypeChecker::visit(ASTBreak * ast) { 
        return ast->setType(space_.getTypeVoid());
    }

    void TypeChecker::visit(ASTContinue * ast) { 
        return ast->setType(space_.getTypeVoid());
    }

    void TypeChecker::visit(ASTReturn * ast) { 
        Type * t = nullptr;
        if (ast->value == nullptr)
            t = space_.getTypeVoid();
        else
            t = visitChild(ast->value);
        if (t != currentReturnType())
            throw ParserError(STR("Invalid return type, expected " << currentReturnType()->toString() << ", but " << t->toString() << " found"), ast->location());
        return ast->setType(t);
    }

    void TypeChecker::visit(ASTBinaryOp * ast) { 
        auto * leftType = visitChild(ast->left);
        auto * rightType = visitChild(ast->right);
        Type * t = nullptr;
        // works on all PODs and pointers
        if (ast->op == Symbol::Add || ast->op == Symbol::Sub) {
            if (space_.isPointer(leftType) && rightType == space_.getTypeInt())
                t = leftType;
            else
                t = getArithmeticResult(leftType, rightType);
        // works on all PODs
        } else if (ast->op == Symbol::Mul || ast->op == Symbol::Div) {
            t = getArithmeticResult(leftType, rightType);
        // works on char and int, result is the type itself, the types must be identical
        } else if (ast->op == Symbol::Mod) {
            if (leftType == rightType && (leftType == space_.getTypeInt() || leftType == space_.getTypeChar()))
                t = leftType;
        // lhs must be char or int, rhs can be char or int, result is lhs
        } else if (ast->op == Symbol::ShiftRight || ast->op == Symbol::ShiftLeft) {
            if ((leftType == space_.getTypeInt() || leftType == space_.getTypeChar()) &&
                (rightType == space_.getTypeInt() || rightType == space_.getTypeChar()))
                t = leftType;
        // works on types convertible to boolean and result is integer (we do not have bool)
        } else if (ast->op == Symbol::And || ast->op == Symbol::Or || ast->op == Symbol::Xor) {
            if (space_.convertsToBool(leftType) && space_.convertsToBool(rightType))
                t = space_.getTypeInt();
        // bitwise and and or operations only work on chars and its, result is the same type
        } else if (ast->op == Symbol::BitAnd || ast->op == Symbol::BitOr) {
            if (leftType == rightType && (leftType == space_.getTypeInt() || leftType == space_.getTypeChar()))
                t = leftType;
        // relational operators work on int, chars,doubles and pointers of same type result is int
        } else if (ast->op == Symbol::Lt || ast->op == Symbol::Gt || ast->op == Symbol::Lte || ast->op == Symbol::Gte) {
            if (leftType == rightType && (space_.isPointer(leftType) || space_.isPOD(leftType)))
                t = space_.getTypeInt();
        // equality and inequality work on all types as long as they match
        } else if(ast->op == Symbol::Eq || ast->op == Symbol::NEq) {
            if (leftType == rightType)
                t = space_.getTypeInt();
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
            if (argt == space_.getTypeInt() || argt == space_.getTypeChar())
                t = argt;
        // not works on any type convertible to boolean
        } else if (ast->op == Symbol::Not) {
            if (space_.convertsToBool(argt))
                t = space_.getTypeInt();
        // works on pointers and arithmetic types
        } else if (ast->op == Symbol::Inc || ast->op == Symbol::Dec) {
            if (!ast->arg->hasAddress())
                throw ParserError("Cannot increment or decrement non l-value", ast->location());
            // not all types can be incremented
            if (space_.isPointer(argt) || space_.isPOD(argt))
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
        if (space_.isPointer(argt) || space_.isPOD(argt))
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
        return ast->setType(space_.getOrCreatePointerType(t));
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
        if (! space_.isPointer(baseType))
            throw ParserError{STR("Expected pointer, but " << baseType->toString() << " found"), ast->location()};
        auto * indexType = visitChild(ast->index);
        if (indexType != space_.getTypeInt() && indexType != space_.getTypeChar())
            throw ParserError{STR("Expected int or char, but " << indexType->toString() << " found"), ast->location()};
        return ast->setType(dynamic_cast<Type::Pointer const *>(baseType)->base());
    }

    Symbol getMemberName(AST const * ast) {
        if (auto ident = dynamic_cast<ASTIdentifier const *>(ast)) {
            return ident->name;
        } else if (auto call = dynamic_cast<ASTCall const *>(ast)) {
            auto ident = dynamic_cast<ASTIdentifier const *>(call->function.get());
            return ident->name;
        } else {
            throw std::runtime_error("unknown ast member representation");
        }
    }

    void TypeChecker::visit(ASTMember * ast) {
        visitChild(ast->base);
        auto * type = visitChild(ast->member);
        if (type == nullptr) {
            auto memberName = getMemberName(ast->member.get());
            throw ParserError{STR("Member " << memberName.name() << " not defined in struct " << type->toString()), ast->location()};
        }
        return ast->setType(type);
    }

    void TypeChecker::visit(ASTCall * ast) {
        visitChild(ast->function);
        Type::Function const * f = asFunctionType(ast->function->getType());
        if (f == nullptr)
            throw ParserError{STR("Expected function, but value of " << ast->function->getType()->toString() << " found"), ast->location()};
        int methodOffset = dynamic_cast<Type::Method const *>(f) != nullptr ? 1 : 0;
        if (ast->args.size() != f->numArgs() - methodOffset)
            throw ParserError{STR("Function of type " << f->toString() << " requires " << f->numArgs() << " arguments, but " << ast->args.size() << " given"), ast->location()};
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
        if (space_.isPointer(castType)) {
            if (space_.isPointer(valueType) || valueType == space_.getTypeInt())
                t = castType;
        } else if (castType == space_.getTypeInt()) {
            if (space_.isPointer(valueType) || space_.isPOD(valueType))
                t = castType;
        } else if (space_.isPOD(castType) && space_.isPOD(valueType))
            t = castType;
        return ast->setType(t);
    }

} // namespace tinycpp

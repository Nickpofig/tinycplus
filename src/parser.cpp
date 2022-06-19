#include "parser.h"

namespace tinycplus {

    bool Parser::isTypeName(Symbol name) const {
        if (possibleTypes_.find(name) != possibleTypes_.end())
            return true;
        return false;
    }

    AccessMod Parser::ACCESS_MOD() {
        auto whatFirst = top();
        if (condPop(symbols::KwAccessPublic)) {
            return AccessMod::Public;
        } else if (condPop(symbols::KwAccessPrivate)) {
            return AccessMod::Private;
        } else if (condPop(symbols::KwAccessProtected)) {
            return AccessMod::Protected;
        }
        throw ParserError(STR("PARSER: expected access modifier, but " << whatFirst << " found"), whatFirst.location(), eof());
    }

    std::unique_ptr<AST> Parser::FUN_OR_VAR_DECL(std::optional<Symbol> className) {
        // it can be either function or variable declaration now,
        // we just do the dirty trick by first parsing the type and identifier
        // to determine whether we are dealing with a function or variable declaration,
        // then revert the parser and parse the proper nonterminal this time
        Position x = position();
        AccessMod accessMod;
        bool isForClass = className.has_value();
        if (isForClass) {
            accessMod = ACCESS_MOD();
        }
        TYPE(true);
        if (isForClass && top() == Symbol::ParOpen) { // check for class constructor
            revertTo(x);
            return FUN_DECL(FunctionKind::ClassConstructor);
        } else {
            IDENT();
            if (top() == Symbol::ParOpen) { // check for function
                revertTo(x);
                return FUN_DECL(isForClass ? FunctionKind::ClassMethod : FunctionKind::None);
            } else {
                revertTo(x);
                auto varDecl = isForClass ? VAR_DECL(isForClass) : VAR_DECLS();
                pop(Symbol::Semicolon);
                return varDecl;
            }
        }
    }

    /* PROGRAM := { FUN_DECL | VAR_DECLS ';' | STRUCT_DECL | FUNPTR_DECL }
        TODO the simple try & fail & try something else produces ugly error messages.
        */
    std::unique_ptr<AST> Parser::PROGRAM() {
        std::unique_ptr<ASTProgram> result{new ASTProgram{top()}};
        while (! eof()) {
            if (top() == Symbol::KwStruct) {
                result->body.push_back(STRUCT_DECL());
            } else if (top() == symbols::KwClass) {
                result->body.push_back(CLASS_DECL());
            } else if (top() == symbols::KwInterface) {
                result->body.push_back(INTERFACE_DECL());
            } else if (top() == Symbol::KwTypedef) {
                result->body.push_back(FUNPTR_DECL());
            } else {
                result->body.push_back(FUN_OR_VAR_DECL(std::nullopt));
            }
        }
        return result;
    }

    /*
        FUN_ARG := TYPE identifier
        FUN_HEAD := TYPE_FUN_RET identifier '(' [ FUN_ARG { ',' FUN_ARG } ] ')'
        FUN_DECL := FUN_HEAD [ BLOCK_STMT | ';' ]
        METHOD_DECL := FUN_HEAD [ [ 'virtual' | 'override' ] [ BLOCK_STMT | ';' ] | 'abstract' ';' ]
    */
    std::unique_ptr<AST> Parser::FUN_DECL(FunctionKind kind) {
        auto accessMod = AccessMod::Public;
        bool isForClass = kind == FunctionKind::ClassMethod || kind == FunctionKind::ClassConstructor;
        if (isForClass) {
            accessMod = ACCESS_MOD();
        }
        auto token = top();
        std::unique_ptr<ASTType> type{TYPE_FUN_RET()};
        if (!isIdentifier(top())) {
            throw ParserError(STR("PARSER: expected class type as constructor name, but " << top() << " found"), top().location(), eof());
        }
        bool isConstructor = kind == FunctionKind::ClassConstructor;
        if (isConstructor) {
        } else {
            token = pop();
        }
        std::unique_ptr<ASTFunDecl> result{new ASTFunDecl{token, std::move(type)}};
        result->kind = kind;
        result->name = token.valueSymbol();
        pop(Symbol::ParOpen);
        if (top() != Symbol::ParClose) {
            do {
                std::unique_ptr<ASTVarDecl> arg{new ASTVarDecl{top(), TYPE()}};
                arg->name = IDENT();
                // check that the name is unique
                for (auto & i : result->args) {
                    if (i->name.get() == arg->name.get()) {
                        throw ParserError(STR("Function argument " << arg->name->name.name() << " altready defined"), arg->name->location(), false);
                    }
                }
                result->args.push_back(std::move(arg));
            } while (condPop(Symbol::Comma));
        }
        pop(Symbol::ParClose);
        if (kind == FunctionKind::ClassMethod) {
            bool isAbstract_ = false;
            // defines method virtuality
            if (condPop(symbols::KwVirtual)) {
                result->virtuality = ASTFunDecl::Virtuality::Virtual;
            } else if (condPop(symbols::KwOverride)) {
                result->virtuality = ASTFunDecl::Virtuality::Override;
            } else if (condPop(symbols::KwAbstract)) {
                result->virtuality = ASTFunDecl::Virtuality::Abstract;
            } else {
                result->virtuality = ASTFunDecl::Virtuality::None;
            }
            if (!isAbstract_) {
                result->body = BLOCK_STMT();
            } else if (!condPop(Symbol::Semicolon)) {
                throw ParserError {
                    STR("PARSER: expected semincolon but " << top() << " found. Remember that an abstract method cannot have a body."),
                    top().location(), false
                };
            }
        }
        else if (isConstructor) {
            if (condPop(Symbol::Colon)) { // inheriting constructor
                result->base = ASTFunDecl::Base { TYPE(false) };
                pop(Symbol::ParOpen);
                if (top() != Symbol::ParClose) {
                    do {
                        result->base->args.push_back(IDENT());
                    } while(condPop(Symbol::Comma));
                }
                pop(Symbol::ParClose);
            }
            result->body = BLOCK_STMT();
        }
        else {
            // if there is body, parse it, otherwise leave empty as it is just a declaration
            if (top() == Symbol::CurlyOpen) {
                result->body = BLOCK_STMT();
            } else if (!condPop(Symbol::Semicolon)) {
                throw ParserError(STR("PARSER: expected semicolon after method forward declartion"), top().location(), false);
            }
        }
        return result;
    }

    // Statements -----------------------------------------------------------------------------------------------------

    /* STATEMENT := BLOCK_STMT | IF_STMT | SWITCH_STMT | WHILE_STMT | DO_WHILE_STMT | FOR_STMT | BREAK_STMT | CONTINUE_STMT | RETURN_STMT | EXPR_STMT
        */
    std::unique_ptr<AST> Parser::STATEMENT() {
        if (top() == Symbol::CurlyOpen)
            return BLOCK_STMT();
        else if (top() == Symbol::KwIf)
            return IF_STMT();
        else if (top() == Symbol::KwSwitch)
            return SWITCH_STMT();
        else if (top() == Symbol::KwWhile)
            return WHILE_STMT();
        else if (top() == Symbol::KwDo)
            return DO_WHILE_STMT();
        else if (top() == Symbol::KwFor)
            return FOR_STMT();
        else if (top() == Symbol::KwBreak)
            return BREAK_STMT();
        else if (top() == Symbol::KwContinue)
            return CONTINUE_STMT();
        else if (top() == Symbol::KwReturn)
            return RETURN_STMT();
        else
            // TODO this would produce not especially nice error as we are happy with statements too
            return EXPR_STMT();
    }

    /* BLOCK_STMT := '{' { STATEMENT } '}'
        */
    std::unique_ptr<AST> Parser::BLOCK_STMT() {
        std::unique_ptr<ASTBlock> result{new ASTBlock{pop(Symbol::CurlyOpen)}};
        while (!condPop(Symbol::CurlyClose))
            result->body.push_back(STATEMENT());
        return result;
    }

    /* IF_STMT := if '(' EXPR ')' STATEMENT [ else STATEMENT ]
        */
    std::unique_ptr<ASTIf> Parser::IF_STMT() {
        std::unique_ptr<ASTIf> result{new ASTIf{pop(Symbol::KwIf)}};
        pop(Symbol::ParOpen);
        result->cond = EXPR();
        pop(Symbol::ParClose);
        result->trueCase = STATEMENT();
        if (condPop(Symbol::KwElse))
            result->falseCase = STATEMENT();
        return result;
    }

    /* SWITCH_STMT := switch '(' EXPR ')' '{' { CASE_STMT } [ default ':' CASE_BODY ] { CASE_STMT } '}'
        CASE_STMT := case integer_literal ':' CASE_BODY
        */
    std::unique_ptr<AST> Parser::SWITCH_STMT() {
        std::unique_ptr<ASTSwitch> result{new ASTSwitch{pop(Symbol::KwSwitch)}};
        pop(Symbol::ParOpen);
        result->cond = EXPR();
        pop(Symbol::ParClose);
        pop(Symbol::CurlyOpen);
        while (!condPop(Symbol::CurlyClose)) {
            if (top() == Symbol::KwDefault) {
                if (result->defaultCase.get() != nullptr)
                    throw ParserError("Default case already provided", top().location(), false);
                pop();
                pop(Symbol::Colon);
                result->defaultCase = CASE_BODY();
            } else if (condPop(Symbol::KwCase)) {
                Token const & t = top();
                int value = pop(Token::Kind::Integer).valueInt();
                if (result->cases.find(value) != result->cases.end())
                    throw ParserError(STR("Case " << value << " already provided"), t.location(), false);
                pop(Symbol::Colon);
                result->cases.insert(std::make_pair(value, CASE_BODY()));
            } else {
                throw ParserError(STR("Expected case or default keyword but " << top() << " found"), top().location(), eof());
            }
        }
        return result;
    }

    /* CASE_BODY := { STATEMENT }
        Can be empty if followed by case, default, ot `}`.
        */
    std::unique_ptr<AST> Parser::CASE_BODY() {
        std::unique_ptr<ASTBlock> result{ new ASTBlock{top()}};
        while (top() != Symbol::KwCase && top() != Symbol::KwDefault && top() != Symbol::CurlyClose) {
            result->body.push_back(STATEMENT());
        }
        return result;
    }

    /* WHILE_STMT := while '(' EXPR ')' STATEMENT
        */
    std::unique_ptr<AST> Parser::WHILE_STMT() {
        std::unique_ptr<ASTWhile> result{new ASTWhile{pop(Symbol::KwWhile)}};
        pop(Symbol::ParOpen);
        result->cond = EXPR();
        pop(Symbol::ParClose);
        result->body = STATEMENT();
        return result;
    }

    /* DO_WHILE_STMT := do STATEMENT while '(' EXPR ')' ';'
        */
    std::unique_ptr<AST> Parser::DO_WHILE_STMT() {
        std::unique_ptr<ASTDoWhile> result{new ASTDoWhile{pop(Symbol::KwDo)}};
        result->body = STATEMENT();
        pop(Symbol::KwWhile);
        pop(Symbol::ParOpen);
        result->cond = EXPR();
        pop(Symbol::ParClose);
        pop(Symbol::Semicolon);
        return result;
    }

    /* FOR_STMT := for '(' [ EXPR_OR_VAR_DECL ] ';' [ EXPR ] ';' [ EXPR ] ')' STATEMENT
        */
    std::unique_ptr<AST> Parser::FOR_STMT() {
        std::unique_ptr<ASTFor> result{new ASTFor{pop(Symbol::KwFor)}};
        pop(Symbol::ParOpen);
        if (top() != Symbol::Semicolon)
            result->init = EXPR_OR_VAR_DECL();
        pop(Symbol::Semicolon);
        if (top() != Symbol::Semicolon)
            result->cond = EXPR();
        pop(Symbol::Semicolon);
        if (top() != Symbol::ParClose)
            result->increment = EXPR();
        pop(Symbol::ParClose);
        result->body = STATEMENT();
        return result;
    }

    /* BREAK_STMT := break ';'
        The parser allows break statement even when there is no loop or switch around it. This has to be fixed in the translator.
        */
    std::unique_ptr<AST> Parser::BREAK_STMT() {
        std::unique_ptr<AST> result{new ASTBreak{pop(Symbol::KwBreak)}};
        pop(Symbol::Semicolon);
        return result;
    }

    /* CONTINUE_STMT := continue ';'
        The parser allows continue statement even when there is no loop around it. This has to be fixed in the translator.
        */
    std::unique_ptr<AST> Parser::CONTINUE_STMT() {
        std::unique_ptr<AST> result{new ASTContinue{pop(Symbol::KwContinue)}};
        pop(Symbol::Semicolon);
        return result;
    }

    /* RETURN_STMT := return [ EXPR ] ';'
        */
    std::unique_ptr<ASTReturn> Parser::RETURN_STMT() {
        std::unique_ptr<ASTReturn> result{new ASTReturn{pop(Symbol::KwReturn)}};
        if (top() != Symbol::Semicolon)
            result->value = EXPR();
        pop(Symbol::Semicolon);
        return result;
    }

    /* EXPR_STMT := EXPR_OR_VAR_DECL ';'
'         */
    std::unique_ptr<AST> Parser::EXPR_STMT() {
        std::unique_ptr<AST> result{EXPR_OR_VAR_DECL()};
        pop(Symbol::Semicolon);
        return result;
    }

    // Types ----------------------------------------------------------------------------------------------------------

    /* TYPE := (int | double | char | identifier) { * }
            |= void * { * }
        The identifier must be a typename.
        */
    std::unique_ptr<ASTType> Parser::TYPE(bool canBeVoid) {
        std::unique_ptr<ASTType> result;
        if (top() == Symbol::KwVoid) {
            result.reset(new ASTNamedType{pop()});
            // if it can't be void, it must be void*
            if (!canBeVoid)
                result.reset(new ASTPointerType{pop(Symbol::Mul), std::move(result)});
        } else {
            if (top() == Symbol::KwInt || top() == Symbol::KwChar || top() == Symbol::KwDouble)
                result.reset(new ASTNamedType{pop()});
            else if (isIdentifier(top()) && isTypeName(top().valueSymbol()))
                result.reset(new ASTNamedType{pop()});
            else {
                throw ParserError(STR("Expected type, but " << top() << " found"), top().location(), eof());
            }
        }
        // deal with pointers to pointers
        while (top() == Symbol::Mul)
            result.reset(new ASTPointerType{pop(Symbol::Mul), std::move(result)});
        return result;
    }

    /* TYPE_FUN_RET := void | TYPE
        */
    std::unique_ptr<ASTType> Parser::TYPE_FUN_RET() {
        return TYPE(true);
    }

    // Type Declarations ----------------------------------------------------------------------------------------------

    /* STRUCT_TYPE_DECL := struct identifier [ '{' { TYPE identifier ';' } '}' ] ';'
        */
    std::unique_ptr<ASTStructDecl> Parser::STRUCT_DECL() {
        Token const & start = pop(Symbol::KwStruct);
        std::unique_ptr<ASTStructDecl> decl{new ASTStructDecl{start, pop(Token::Kind::Identifier).valueSymbol()}};
        addTypeName(decl->name);
        if (condPop(Symbol::CurlyOpen)) {
            while (! condPop(Symbol::CurlyClose)) {
                decl->fields.push_back(VAR_DECL(false));
                pop(Symbol::Semicolon);
            }
            decl->isDefinition = true;
        }
        pop(Symbol::Semicolon);
        return decl;
    }

    /* FUNPTR_TYPE_DECL := 'typedef' TYPE_FUN_RET '(' '*' identifier ')' '(' [ TYPE { ',' TYPE } ] ')' ';'
        */
    std::unique_ptr<ASTFunPtrDecl> Parser::FUNPTR_DECL() {
        Token const & start = pop(Symbol::KwTypedef);
        std::unique_ptr<ASTType> returnType{TYPE_FUN_RET()};
        pop(Symbol::ParOpen);
        pop(Symbol::Mul);
        std::unique_ptr<ASTIdentifier> name{IDENT()};
        addTypeName(name->name);
        pop(Symbol::ParClose);
        pop(Symbol::ParOpen);
        std::unique_ptr<ASTFunPtrDecl> result{new ASTFunPtrDecl{start, std::move(name), std::move(returnType)}};
        if (top() != Symbol::ParClose) {
            result->args.push_back(TYPE());
            while (condPop(Symbol::Comma))
                result->args.push_back(TYPE());
        }
        pop(Symbol::ParClose);
        pop(Symbol::Semicolon);
        return result;
    }

    std::unique_ptr<ASTInterfaceDecl> Parser::INTERFACE_DECL() {
        auto const & start = pop(symbols::KwInterface);
        auto interfaceName = pop(Token::Kind::Identifier).valueSymbol();
        std::unique_ptr<ASTInterfaceDecl> interfaceDecl {new ASTInterfaceDecl{start, interfaceName}};
        addTypeName(interfaceName);
        if (condPop(Symbol::CurlyOpen)) {
            while (! condPop(Symbol::CurlyClose)) {
                // parsing field and method
                auto member = FUN_DECL(FunctionKind::InterfaceMethod);
                if (auto * method = member->as<ASTFunDecl>()) {
                    auto methodName = method->name.value().name();
                    if (method->body) throw ParserError {
                        STR("Interface's method: " << methodName << " must not have a body."),
                        method->location(), eof()
                    };
                    interfaceDecl->methods.push_back(std::unique_ptr<ASTFunDecl>(method));
                }
                member.release();
            }
        }
        pop(Symbol::Semicolon);
        return interfaceDecl;
    }

    /* CLASS_DECL := 'class' identifier [ ':' identifier { ',' identifier } ] [ '{' { TYPE identifier ';' | FUN_DECL } '}' ] ';'
        */
    std::unique_ptr<ASTClassDecl> Parser::CLASS_DECL() {
        auto const & start = pop(symbols::KwClass);
        auto className = pop(Token::Kind::Identifier).valueSymbol();
        this->className = className;
        std::unique_ptr<ASTClassDecl> classDecl{new ASTClassDecl{start, className}};
        addTypeName(className);
        // Parses base class
        if (condPop(Symbol::Colon)) {
            classDecl->baseClass = TYPE();
            while(condPop(Symbol::Comma)) {
                classDecl->interfaces.push_back(TYPE());
            }
        }
        // Parses body
        if (condPop(Symbol::CurlyOpen)) {
            classDecl->isDefinition = true;
            // std::unordered_set<ASTMethodDecl*> undefinedMethods {};
            while (! condPop(Symbol::CurlyClose)) {
                // parsing field and method
                auto member = FUN_OR_VAR_DECL(className);
                if (auto * field = member->as<ASTVarDecl>()) {
                    classDecl->fields.push_back(std::unique_ptr<ASTVarDecl>(field));
                } else if (auto * func = member->as<ASTFunDecl>()) {
                    if (func->isClassConstructor()) { // saving as constructor
                        classDecl->constructors.push_back(std::unique_ptr<ASTFunDecl>(func));
                    } else {  // saving as method
                        classDecl->methods.push_back(std::unique_ptr<ASTFunDecl>(func));
                        if (!func->body && !func->isAbstract()) {
                            // undefinedMethods.insert(method);
                            auto methodName = func->name.value().name();
                            throw ParserError {
                                STR("Method: " << methodName << " was declared but its body was not defined"),
                                func->location()
                            };
                        }
                    }
                }
                member.release();
            }
            // if (!undefinedMethods.empty()) {
            //     auto * ast = *undefinedMethods.begin();
            // }
        }
        pop(Symbol::Semicolon);
        this->className = std::nullopt;
        return classDecl;
    }

    // Expressions ----------------------------------------------------------------------------------------------------

    /* EXPR_OR_VAR_DECL := ( EXPR | VAR_DECL)  { ',' ( EXPR | VAR_DECL ) }
        We can either be smart and play with FIRST and FOLLOW sets, or we can be lazy and just do the simple thing - try TYPE first and if it fails, try EXPR.
        I am lazy.
        */
    std::unique_ptr<AST> Parser::EXPR_OR_VAR_DECL() {
        Position x = position();
        try {
            TYPE();
        } catch (...) {
            revertTo(x);
            return EXPRS();
        }
        revertTo(x);
        return VAR_DECLS();
    }

    /* VAR_DECL := TYPE identifier [ '[' E9 ']' ] [ '=' EXPR ]
        */
    std::unique_ptr<ASTVarDecl> Parser::VAR_DECL(bool isField) {
        Token const & start = top();
        auto accessMod = isField ? ACCESS_MOD() : AccessMod::Public;
        std::unique_ptr<ASTVarDecl> decl{new ASTVarDecl{start, TYPE()}};
        decl->name = IDENT();
        decl->access = accessMod;
        if (condPop(Symbol::SquareOpen)) {
            std::unique_ptr<AST> index{E9()};
            pop(Symbol::SquareClose);
            // now we have to update the type
            decl->type.reset(new ASTArrayType{start, std::move(decl->type), std::move(index) });
        }
        if (condPop(Symbol::Assign)) {
            decl->value = EXPR();
        }
        return decl;
    }

    /* VAR_DECLS := VAR_DECL { ',' VAR_DECL }
        */
    std::unique_ptr<AST> Parser::VAR_DECLS() {
        std::unique_ptr<ASTSequence> result{new ASTSequence{top()}};
        result->body.push_back(VAR_DECL(false));
        while (condPop(Symbol::Comma))
            result->body.push_back(VAR_DECL(false));
        return result;
    }

    /* EXPR := E9 [ '=' EXPR ]
        Note that assignment is right associative.
        */
    std::unique_ptr<AST> Parser::EXPR() {
        std::unique_ptr<AST> result{E9()};
        if (top() == Symbol::Assign) {
            Token const & op = pop();
            result.reset(new ASTAssignment{op, std::move(result), EXPR()});
        }
        return result;
    }

    /* EXPRS := EXPR { ',' EXPR }
        */
    std::unique_ptr<AST> Parser::EXPRS() {
        std::unique_ptr<ASTSequence> result{new ASTSequence{top()}};
        result->body.push_back(EXPR());
        while (condPop(Symbol::Comma))
            result->body.push_back(EXPR());
        return result;
    }


    /* E9 := E8 { '||' E8 }
        */
    std::unique_ptr<AST> Parser::E9() {
        std::unique_ptr<AST> result{E8()};
        while (top() == Symbol::Or) {
            Token const & op = pop();
            result.reset(new ASTBinaryOp{op, std::move(result), E8()});
        }
        return result;
    }

    /* E8 := E7 { '&&' E7 }
        */
    std::unique_ptr<AST> Parser::E8() {
        std::unique_ptr<AST> result{E7()};
        while (top() == Symbol::And) {
            Token const & op = pop();
            result.reset(new ASTBinaryOp{op, std::move(result), E7()});
        }
        return result;
    }

    /* E7 := E6 { '|' E6 }
        */
    std::unique_ptr<AST> Parser::E7() {
        std::unique_ptr<AST> result{E6()};
        while (top() == Symbol::BitOr) {
            Token const & op = pop();
            result.reset(new ASTBinaryOp{op, std::move(result), E6()});
        }
        return result;
    }

    /* E6 := E5 { '&' E5 }
        */
    std::unique_ptr<AST> Parser::E6() {
        std::unique_ptr<AST> result{E5()};
        while (top() == Symbol::BitAnd) {
            Token const & op = pop();
            result.reset(new ASTBinaryOp{op, std::move(result), E5()});
        }
        return result;
    }

    /* E5 := E4 { ('==' | '!=') E4 }
        */
    std::unique_ptr<AST> Parser::E5() {
        std::unique_ptr<AST> result{E4()};
        while (top() == Symbol::Eq || top() == Symbol::NEq) {
            Token const & op = pop();
            result.reset(new ASTBinaryOp{op, std::move(result), E4()});
        }
        return result;
    }

    /* E4 := E3 { ('<' | '<=' | '>' | '>=') E3 }
        */
    std::unique_ptr<AST> Parser::E4() {
        std::unique_ptr<AST> result{E3()};
        while (top() == Symbol::Lt || top() == Symbol::Lte || top() == Symbol::Gt || top() == Symbol::Gte) {
            Token const & op = pop();
            result.reset(new ASTBinaryOp{op, std::move(result), E3()});
        }
        return result;
    }

    /* E3 := E2 { ('<<' | '>>') E2 }
        */
    std::unique_ptr<AST> Parser::E3() {
        std::unique_ptr<AST> result{E2()};
        while (top() == Symbol::ShiftLeft || top() == Symbol::ShiftRight) {
            Token const & op = pop();
            result.reset(new ASTBinaryOp{op, std::move(result), E2()});
        }
        return result;
    }

    /* E2 := E1 { ('+' | '-') E1 }
        */
    std::unique_ptr<AST> Parser::E2() {
        std::unique_ptr<AST> result{E1()};
        while (top() == Symbol::Add || top() == Symbol::Sub) {
            Token const & op = pop();
            result.reset(new ASTBinaryOp{op, std::move(result), E1()});
        }
        return result;
    }

    /* E1 := E_UNARY_PRE { ('*' | '/' | '%' ) E_UNARY_PRE }
        */
    std::unique_ptr<AST> Parser::E1() {
        std::unique_ptr<AST> result{E_UNARY_PRE()};
        while (top() == Symbol::Mul || top() == Symbol::Div || top() == Symbol::Mod) {
            Token const & op = pop();
            result.reset(new ASTBinaryOp{op, std::move(result), E_UNARY_PRE()});
        }
        return result;
    }

    /* E_UNARY_PRE := { '+' | '-' | '!' | '~' | '++' | '--' | '*' | '&' } E_CALL_INDEX_MEMBER_POST
        */
    std::unique_ptr<AST> Parser::E_UNARY_PRE() {
        if (top() == Symbol::Add || top() == Symbol::Sub
                || top() == Symbol::Not
                || top() == Symbol::Neg
                || top() == Symbol::Inc
                || top() == Symbol::Dec) {
            Token const & op = pop();
            return std::unique_ptr<AST>{new ASTUnaryOp{op, E_UNARY_PRE()}};
        } else if (top() == Symbol::Mul) {
            Token const & op = pop();
            return std::unique_ptr<AST>{new ASTDeref{op, E_UNARY_PRE()}};
        } else if (top() == Symbol::BitAnd) {
            Token const & op = pop();
            return std::unique_ptr<AST>{new ASTAddress{op, E_UNARY_PRE()}};
        } else {
            return E_CALL_INDEX_MEMBER_POST();
        }
    }

    std::unique_ptr<AST> Parser::E_CALL(std::unique_ptr<AST> & functionName) {
        // std::cout << "DEBUG: member call." << std::endl;
        std::unique_ptr<ASTCall> call{new ASTCall{pop(), std::move(functionName)}};
        if (top() != Symbol::ParClose) {
            call->args.push_back(EXPR());
            while (condPop(Symbol::Comma))
                call->args.push_back(EXPR());
        }
        pop(Symbol::ParClose);
        return call;
    }

    /* E_CALL_INDEX_MEMBER_POST := F { E_CALL | E_INDEX | E_MEMBER | E_POST }
        E_CALL := '(' [ EXPR { ',' EXPR } ] ')'
        E_INDEX := '[' EXPR ']'
        E_MEMBER := ('.' | '->') identifier { E_CALL }
        E_POST := '++' | '--'
        */
    std::unique_ptr<AST> Parser::E_CALL_INDEX_MEMBER_POST() {
        auto beforeCheck = position();
        bool isConstructorCall = isIdentifier(top()) && isTypeName(top().valueSymbol());
        if (isConstructorCall) {
            TYPE();
            isConstructorCall = condPop(Symbol::ParOpen);
            revertTo(beforeCheck);
            // throw ParserError {
            //     STR("Seeking constructor call. Expected '(' but " << top() << " found"),
            //     top().location()
            // };
        }
        std::unique_ptr<AST> result{isConstructorCall ? TYPE() : F()};
        while (true) {
            if (top() == Symbol::ParOpen) {
                auto call = E_CALL(result);
                result.reset(call.release());
            } else if (top() == Symbol::SquareOpen) {
                Token const & op = pop();
                result.reset(new ASTIndex{op, std::move(result), EXPR()});
                pop(Symbol::SquareClose);
            } else if (top() == Symbol::Dot || top() == Symbol::ArrowR) {
                Token const & op = pop();
                std::unique_ptr<AST> memberName {IDENT().release()};
                if (top() == Symbol::ParOpen) { // method call
                    memberName = E_CALL(memberName);
                }
                result.reset(new ASTMember{op, std::move(result), std::move(memberName)});
            } else if (top() == Symbol::Inc || top() == Symbol::Dec) {
                Token const & op = pop();
                result.reset(new ASTUnaryPostOp{op, std::move(result)});
            } else {
                break;
            }
        }
        return result;
    }

    /* F := integer | double | char | string | identifier | '(' EXPR ')' | E_CAST
        E_CAST := cast '<' TYPE '>' '(' EXPR ')'
        */
    std::unique_ptr<AST> Parser::F() {
        if (top() == Token::Kind::Integer) {
            return std::unique_ptr<AST>{new ASTInteger{pop()}};
        } else if (top() == Token::Kind::Double) {
            return std::unique_ptr<AST>{new ASTDouble{pop()}};
        } else if (top() == Token::Kind::StringSingleQuoted) {
            return std::unique_ptr<AST>{new ASTChar{pop()}};
        } else if (top() == Token::Kind::StringSingleQuoted) {
            return std::unique_ptr<AST>{new ASTString{pop()}};
        } else if (top() == Symbol::KwCast) {
            Token op = pop();
            pop(Symbol::Lt);
            std::unique_ptr<ASTType> type{TYPE()};
            pop(Symbol::Gt);
            pop(Symbol::ParOpen);
            std::unique_ptr<AST> expr(EXPR());
            pop(Symbol::ParClose);
            return std::unique_ptr<AST>{new ASTCast{op, std::move(expr), std::move(type)}};
        } else if (top() == Token::Kind::Identifier) {
            return IDENT();
        } else if (condPop(Symbol::ParOpen)) {
            return EXPR();
            pop(Symbol::ParClose);
        } else {
            throw ParserError(STR("Expected literal, (expr) or cast, but " << top() << " found"), top().location(), eof());
        }
    }

    std::unique_ptr<ASTIdentifier> Parser::IDENT() {
        if (!isIdentifier(top()) || isTypeName(top().valueSymbol()))
            throw ParserError(STR("Expected identifier, but " << top() << " found"), top().location(), eof());
        return std::unique_ptr<ASTIdentifier>{new ASTIdentifier{pop()}};
    }
}
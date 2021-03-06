#pragma once

// standard
#include <unordered_set>
#include <optional>

// internal
#include "shared.h"
#include "ast.h"

namespace tinycplus {

    class Parser : public ParserBase {
    public:
        static std::unique_ptr<AST> ParseFile(std::string const & filename) {
            Parser p{Lexer::TokenizeFile(filename)};
            std::unique_ptr<AST> result{p.PROGRAM()};
            p.pop(Token::Kind::EoF);
            return result;
        }

    protected:

        std::optional<Symbol> className = std::nullopt;

        Parser(std::vector<Token> && tokens): ParserBase{std::move(tokens)} { }


        /** Determines if given token is a language keyword.
         */
        bool isKeyword(Token const & t) {
            return 
                // tinycplus keywords
                t == Symbol::KwBreak
                || t == Symbol::KwCase
                || t == Symbol::KwCast
                || t == Symbol::KwChar
                || t == Symbol::KwContinue
                || t == Symbol::KwDefault
                || t == Symbol::KwDo
                || t == Symbol::KwDouble
                || t == Symbol::KwElse
                || t == Symbol::KwFor
                || t == Symbol::KwIf
                || t == Symbol::KwInt
                || t == Symbol::KwReturn
                || t == Symbol::KwStruct
                || t == Symbol::KwSwitch
                || t == Symbol::KwTypedef
                || t == Symbol::KwVoid
                || t == Symbol::KwWhile
                // tinycplus keywords
                || symbols::isParsebleKeyword(t.valueSymbol());
        }

        /** Determines if given token is a valid user identifier.
         */
        bool isIdentifier(Token const & t) {
            return t.kind() == Token::Kind::Identifier && !isKeyword(t);
        }

        /** \name Types Ambiguity

            tinyC shares the unfortunate problem of C and C++ grammars which makes it impossible to determine whether an expression is type declaration or an expression simply from the grammar. take for instance

                foo * a;

            Is this declaration of variable of name `a` with type `foo*`, or is this multiplication of two variables `foo` and `a`. Ideally this ambiguity should be solved at the grammar level such as introducing `var` keyword, or some such, but for educational purposes we have decided to keep this "feature" in the language.

            The way to fix this is to make the parser track all possible type names so that an identifier can be resolved as being either variable, or a type, thus removing the ambiguity. In tiny's case this is further complicated by the parser being state-full in the repl mode.
         */

        std::unordered_set<Symbol> possibleTypes_;
        std::vector<Symbol> possibleTypesStack_;

        /** Returns true if given symbol is a type.

            Checks both own tentative records and the frontend's valid records.
         */
        bool isTypeName(Symbol name) const;

        /** Adds given symbol as a tentative typename.

            Note that same typename can be added multiple times for forward declared structs.
         */
        void addTypeName(Symbol name) {
            possibleTypes_.insert(name);
            possibleTypesStack_.push_back(name);
        }

        /** We need new position because when reverting, the tentative types names that were created *after* the savepoint must be unrolled as well.
         */
        class Position {
        private:
            friend class Parser;

            ParserBase::Position position_;
            size_t possibleTypesSize_;

            Position(ParserBase::Position position, size_t typesSize):
                position_{position},
                possibleTypesSize_{typesSize} {
            }
        };

        Position position() {
            return Position{ParserBase::position(), possibleTypesStack_.size()};
        }

        void revertTo(Position const & p) {
            ParserBase::revertTo(p.position_);
            while (possibleTypesStack_.size() > p.possibleTypesSize_) {
                possibleTypes_.erase(possibleTypesStack_.back());
                possibleTypesStack_.pop_back();
            }
        }

        /*  Parsing

            Nothing fancy here, just a very simple recursive descent parser built on the basic framework.
         */
        AccessMod ACCESS_MOD();
        std::unique_ptr<AST> PROGRAM();
        std::unique_ptr<AST> FUN_DECL(FunctionKind kind);
        std::unique_ptr<AST> STATEMENT();
        std::unique_ptr<AST> BLOCK_STMT();
        std::unique_ptr<ASTIf> IF_STMT();
        std::unique_ptr<AST> SWITCH_STMT();
        std::unique_ptr<AST> CASE_BODY();
        std::unique_ptr<AST> WHILE_STMT();
        std::unique_ptr<AST> DO_WHILE_STMT();
        std::unique_ptr<AST> FOR_STMT();
        std::unique_ptr<AST> BREAK_STMT();
        std::unique_ptr<AST> CONTINUE_STMT();
        std::unique_ptr<ASTReturn> RETURN_STMT();
        std::unique_ptr<AST> EXPR_STMT();
        std::unique_ptr<ASTType> TYPE(bool canBeVoid = false);
        std::unique_ptr<ASTType> TYPE_FUN_RET();
        std::unique_ptr<ASTStructDecl> STRUCT_DECL();
        std::unique_ptr<ASTFunPtrDecl> FUNPTR_DECL();
        std::unique_ptr<ASTInterfaceDecl> Parser::INTERFACE_DECL();
        std::unique_ptr<ASTClassDecl> CLASS_DECL();
        std::unique_ptr<AST> EXPR_OR_VAR_DECL();
        std::unique_ptr<ASTVarDecl> VAR_DECL(bool isField);
        std::unique_ptr<AST> VAR_DECLS();
        std::unique_ptr<AST> FUN_OR_VAR_DECL(std::optional<Symbol> className);
        std::unique_ptr<AST> EXPR();
        std::unique_ptr<AST> EXPRS();
        std::unique_ptr<AST> E9();
        std::unique_ptr<AST> E8();
        std::unique_ptr<AST> E7();
        std::unique_ptr<AST> E6();
        std::unique_ptr<AST> E5();
        std::unique_ptr<AST> E4();
        std::unique_ptr<AST> E3();
        std::unique_ptr<AST> E2();
        std::unique_ptr<AST> E1();
        std::unique_ptr<AST> E_UNARY_PRE();
        std::unique_ptr<AST> E_CALL(std::unique_ptr<AST> & functionName);
        std::unique_ptr<AST> E_CALL_INDEX_MEMBER_POST();
        std::unique_ptr<AST> F();
        std::unique_ptr<ASTIdentifier> IDENT();

    }; // tiny::Parser

} // namespace tiny

#pragma once

// standard
#include <memory>
#include <unordered_set>

// internal
#include "tinycpp/shared.h"
#include "tinycpp/ast.h"


namespace tinycpp {

    using ParserBase = tiny::ParserBase;
    using ParserError = tiny::ParserError;

    namespace symbols {
        static Symbol KwBase {"base"};
        static Symbol KwClass {"class"};
        static Symbol KwIs {"is"};
        static Symbol KwPrivate {"private"};
        static Symbol KwProtected {"protected"};
        static Symbol KwPublic {"public"};
        static Symbol KwThis {"this"};
        static Symbol KwTrait {"trait"};

        bool isKeyword(Symbol const & s) {
            return
                s == KwBase
                || s == KwClass 
                || s == KwIs
                || s == KwPrivate
                || s == KwProtected
                || s == KwPublic
                || s == KwThis
                || s == KwTrait
                ;
        }
    }

    class Parser : public ParserBase {
    public:
        static uptr<AST> ParseFile(std::string const & filename) {
            Parser p{Lexer::TokenizeFile(filename)};
            p.addTypeName(Symbol::KwInt);
            p.addTypeName(Symbol::KwChar);
            uptr<AST> result{p.parseProgram()};
            p.pop(Token::Kind::EoF);
            return result;
        }
    protected:
        Parser(std::vector<Token> && tokens):
            ParserBase{std::move(tokens)} {
        }

        /** Determines if given token is a language keyword.
         */
        bool isKeyword(Token const & t) {
            return 
                // tinyc keywords
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
                // tinycpp keywords
                || symbols::isKeyword(t.valueSymbol());
        }

        /** Determines if given token is a valid user identifier.
         */
        bool isIdentifier(Token const & t) {
            return t.kind() == Token::Kind::Identifier && !isKeyword(t);
        }

        std::unordered_set<Symbol> possibleTypes_;
        std::vector<Symbol> possibleTypesStack_;

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

        // === Case Rollback ===
        /// TODO: possibleTypesStack, position(), revretTo(): are helpful if a case could fail,
        ///       so we may return and try another case.
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
        // =====================


        void throwError(const std::string & message) const {
            throw ParserError{message, top().location(), eof()};
        }

        Symbol popIdentifierAsNewType() {
            auto token = pop(Token::Kind::Identifier);
            auto symbol = token.valueSymbol();
            possibleTypes_.insert(symbol);
            return symbol;
        }

        Symbol popIdentifier(bool asType) {
            auto token = pop(Token::Kind::Identifier);
            auto symbol = token.valueSymbol();
            bool isType = possibleTypes_.find(symbol) != possibleTypes_.end();
            if (asType && !isType) {
                throwError(STR("Dont know type with name: " << symbol));
            }
            else if (!asType && isType) {
                throwError(STR("Identifier cant be a type: " << symbol));
            }
            return symbol;
        }

        int popInteger(bool isSigned) {
            auto token = pop(Token::Kind::Integer);
            int value = token.valueInt();
            if (!isSigned && value < 0) {
                throwError(STR("Expected unsigned integer, but got signed: " << value));
            }
            return value;
        }

        bool nextIsField(uptr<AST> & result) {
            if (condPop(Symbol::KwTypedef)) { // function ptr
                assert(false && "not implemented");
            } else {
                auto * ast = new AST::Raw(top());
                ast->add(top()); popIdentifier(true);
                auto t = top();
                while(condPop(Symbol::Mul)) {
                    ast->add(t);
                }
                ast->add(top()); popIdentifier(false);
                t = top();
                if (condPop(Symbol::SquareOpen)) {
                    ast->add(t);
                    ast->add(pop(Token::Kind::Integer));
                    ast->add(pop(Symbol::SquareClose));
                }
                ast->add(pop(Symbol::Semicolon));
                result.reset(ast);
                return true;
            }
            return false;
        }

        bool nextIsClass(uptr<AST> & result) {
            if (!condPop(symbols::KwClass)) {
                return false;
            }
            auto astClass = new AST::Class{top()};
            auto className = popIdentifierAsNewType();
            pop(Symbol::CurlyOpen);
            while(!condPop(Symbol::CurlyClose)) {
                uptr<AST> astMember;
                if (nextIsField(astMember)) {
                    astClass->takeAsField(astMember);
                }
            }
            pop(Symbol::Semicolon);
            result.reset(astClass);
            return true;
        }

        uptr<AST> parseProgram() {
            auto rootScope = new AST::Scope{top()};
            AST::Raw * skippedPart = nullptr;
            auto flushSkippedPart = [&rootScope, &skippedPart] {
                if (skippedPart != nullptr) {
                    rootScope->take(uptr<AST>{skippedPart});
                    skippedPart = nullptr;
                }
            };
            while (!eof()) {
                uptr<AST> ast;
                if (nextIsClass(ast)) {
                    flushSkippedPart();
                    rootScope->take(ast);
                } else {
                    if (skippedPart == nullptr) {
                        skippedPart = new AST::Raw{top()};
                    }
                    skippedPart->add(pop());
                }
            }
            return uptr<AST>{rootScope};
        }

    }; // class Parser

} // namespace tinycpp
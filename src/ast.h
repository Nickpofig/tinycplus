#pragma once

// standard
#include <unordered_map>
#include <memory>
#include <optional>

// internal
#include "shared.h"

namespace tinycpp {
    class Type;
    class ASTVisitor;

    // AST custom user Data support

    class AST : public ASTBase {
    protected:
        AST(Token const & t): ASTBase{t} { }
    public:
        /** Returns true if the result of the expression has an address.
            A bit simplified version of l-values from C++. This is important for two things:
            1) an address (ASTAddress) can only be obtained of expressions that have address themselves.
            2) only elements that have address can be assigned to
         */
        virtual bool hasAddress() const {
            return false;
        }

    // [*] Hierarchy information
    public: // data
        AST * parentAST = nullptr;
        template<typename T>
        T * findParent(std::optional<int> depth = std::nullopt) {
            if (parentAST == nullptr) return nullptr;
            if (auto result = dynamic_cast<T*>(parentAST)) return result;
            if (depth.has_value()) {
                if (depth.value() > 0) return parentAST->findParent<T>(depth.value() - 1);
                else return nullptr;
            }
            return parentAST->findParent<T>();
        }

        bool isDescendentOf(AST * ancestor) {
            if (this == ancestor) return true;
            if (parentAST != nullptr) return parentAST->isDescendentOf(ancestor);
            return false;
        }

    // [*] Type information
    private:
        Type * type_ = nullptr;
    public: // methods
        /** Returns the backend type of the AST expression.
            After a successful type checking this must never be nullptr.
        */
        Type * getType() const {
            return type_;
        }
        /** Sets the type for the expression in the AST node. 
            The type must *not* be nullptr. Setting type twice is an error unless the type is identical.
        */
        void setType(Type * t) {
            if (t == nullptr)
                throw ParserError("Incorrect types", location());
            if (type_ != nullptr && type_ != t)
                throw ParserError("Different type already set", location());
            type_ = t;
        }


    // [*] AST Visitor support
    protected:
        friend class ASTVisitor;
        virtual void accept(ASTVisitor * v) = 0;
    };

    class ASTInteger : public AST {
    public:
        int64_t value;

        ASTInteger(Token const & t):
            AST{t},
            value{t.valueInt()} {
        }

        void print(ASTPrettyPrinter & p) const override {
            p << p.numberLiteral << value;
        }

    protected:
        void accept(ASTVisitor * v) override;

    };

    class ASTDouble : public AST {
    public:
        double value;

        ASTDouble(Token const & t):
            AST{t},
            value{t.valueDouble()} {
        }

        void print(ASTPrettyPrinter & p) const override {
            p << p.numberLiteral << value;
        }

    protected:
        void accept(ASTVisitor * v) override;

    };

    class ASTChar : public AST {
    public:
        char value;

        ASTChar(Token const & t):
            AST{t} {
            std::string const & s{t.valueString()};
            if (t == Token::Kind::StringDoubleQuoted)
                throw ParserError(STR("Expected character (single quote), but string \"" << s << "\" (double quote) found"), t.location(), false);
            if (s.size() != 1)
                throw ParserError(STR("Expected single character, but " << s.size() << " characters found in '" << s << "'"), t.location(), false);
            value = s[0];
        }

        void print(ASTPrettyPrinter & p) const override {
            std::stringstream ss;
            ss << '\'' << value << '\'';
            p << p.charLiteral << ss.str();
        }

    protected:
        void accept(ASTVisitor * v) override;

    };

    class ASTString : public AST {
    public:
        std::string value;

        ASTString(Token const & t):
            AST{t} {
            std::string const & s{t.valueString()};
            if (t == Token::Kind::StringDoubleQuoted)
                throw ParserError(STR("Expected string (double quote), but character '" << s << "' (single quote) found"), t.location(), false);
            value = s;
        }


        void print(ASTPrettyPrinter & p) const override {
            p << p.stringLiteral << value;
        }

    protected:
        void accept(ASTVisitor * v) override;

    };

    class ASTIdentifier : public AST {
    public:
        Symbol name;

        ASTIdentifier(Token const & t):
            AST{t},
            name{t.valueSymbol()} {
        }

        /** An identifier in read position is a variable read and all variables have addresses.
         */
        bool hasAddress() const override {
            return true;
        }


        void print(ASTPrettyPrinter & p) const override {
            p << p.identifier << name.name();
        }

    protected:
        void accept(ASTVisitor * v) override;

    };

    /** Base class for all types.
     */
    class ASTType : public AST {
    public:
        std::string toString() const {
            std::stringstream ss;
            buildStringRepresentation(ss);
            return ss.str();
        }

    protected:
        ASTType(Token const & t):
            AST{t} {
        }

    protected:
        virtual void buildStringRepresentation(std::ostream & s) const = 0;

        // friendship is not inherited, but methods are
        void toString(ASTType const * type, std::ostream & s) const {
            type->buildStringRepresentation(s);
        }

        void accept(ASTVisitor * v) override;

    };

    class ASTPointerType : public ASTType {
    public:
        std::unique_ptr<ASTType> base;

        ASTPointerType(Token const & t, std::unique_ptr<ASTType> base):
            ASTType{t},
            base{std::move(base)} {
        }

        void print(ASTPrettyPrinter & p) const override {
            p << (*base) << p.symbol << "*";
        }

    protected:
        void buildStringRepresentation(std::ostream & s) const override {
            toString(base.get(), s);
            s << "*";
        };

        void accept(ASTVisitor * v) override;

    };

    class ASTArrayType : public ASTType {
    public:
        std::unique_ptr<ASTType> base;
        std::unique_ptr<AST> size;

        ASTArrayType(Token const & t, std::unique_ptr<ASTType> base, std::unique_ptr<AST> size):
            ASTType{t},
            base{std::move(base)},
            size{std::move(size)} {
        }

        void print(ASTPrettyPrinter & p) const override {
            p << (*base) << p.symbol << "[" << *size << p.symbol << "]";
        }

    protected:
        void buildStringRepresentation(std::ostream & s) const override {
            toString(base.get(), s);
            s << "[]";
        };

        void accept(ASTVisitor * v) override;

    };

    class ASTNamedType : public ASTType {
    public:
        Symbol name;

        ASTNamedType(Token const & t) :
            ASTType{t},
            name{t.valueSymbol()} {
        }

        void print(ASTPrettyPrinter & p) const override {
            p << p.type << name.name();
        }

    protected:
        void buildStringRepresentation(std::ostream & s) const override {
            s << name.name();
        };

        void accept(ASTVisitor * v) override;

    };

    // comma separated, single line
    class ASTSequence : public AST {
    public:
        std::vector<std::unique_ptr<AST>> body;

        ASTSequence(Token const & t):
            AST{t} {
        }

        /** The result of sequence has address if its last element has address as the last element is what is returned.
         */
        bool hasAddress() const override {
            if (body.empty())
                return false;
            return body.back()->hasAddress();
        }


        void print(ASTPrettyPrinter & p) const override {
            auto i = body.begin();
            if (i != body.end()) {
                p << **i;
                while (++i != body.end())
                    p << p.symbol << ", " << **i;
            }
        }

    protected:
        void accept(ASTVisitor * v) override;

    };

    // new line separated with {}
    class ASTBlock : public ASTSequence {
    public:
        ASTBlock(Token const & t):
            ASTSequence{t} {
        }

        void print(ASTPrettyPrinter & p) const override {
            p << p.symbol << "{";
            p.indent();
            for (auto & i : body) {
                p.newline();
                p << *i;
            }
            p.dedent();
            p.newline();
            p << p.symbol << "}";
            p.newline();
        }

    protected:
        void accept(ASTVisitor * v) override;

    };

    class ASTVarDecl : public AST {
    public:
        std::unique_ptr<ASTType> type;
        std::unique_ptr<ASTIdentifier> name;
        std::unique_ptr<AST> value;

        ASTVarDecl(Token const & t, std::unique_ptr<ASTType> type):
            AST{t},
            type{std::move(type)} {
        }

        void print(ASTPrettyPrinter & p) const override {
            p << (*type) << " " << (*name);
            if (value.get() != nullptr) {
                p << p.symbol << " = " << (*value);
            }
        }

    protected:
        void accept(ASTVisitor * v) override;

    };


    class ASTFunDecl : public AST {
    public:
        std::unique_ptr<ASTType> typeDecl;
        Symbol name;
        std::vector<std::unique_ptr<ASTVarDecl>> args;
        std::unique_ptr<AST> body;

        ASTFunDecl(Token const & t, std::unique_ptr<ASTType> type):
            AST{t},
            typeDecl{std::move(type)},
            name{t.valueSymbol()} {
        }

        void print(ASTPrettyPrinter & p) const override {
            p << (*typeDecl) << " " << p.identifier << name.name() << p.symbol << "(";
            auto arg = args.begin();
            if (arg != args.end()) {
                p << *arg[0];
                while (++arg != args.end())
                    p << p.symbol << ", " << *arg[0];
            }
            p << p.symbol << ")" << (*body);
        }

    protected:
        void accept(ASTVisitor * v) override;

    };

    class ASTStructDecl : public AST {
    public:
        Symbol name;
        std::vector<std::unique_ptr<ASTVarDecl>> fields;

        /** If true the struct has also field definitions. Extra flag is necessary because empty fields can also mean an empty struct.
         */
        bool isDefinition = false;

        ASTStructDecl(Token const & t, Symbol name):
            AST{t},
            name{name} {
        }


        void print(ASTPrettyPrinter & p) const override {
            p << p.keyword << "struct " << p.identifier << name.name();
            if (isDefinition) {
                p << p.symbol << "{";
                p.indent();
                for (auto & i : fields) {
                    p.newline();
                    i->print(p);
                }
                p.dedent();
                p.newline();
                p << p.symbol << "}";
            }
        }

    protected:
        void accept(ASTVisitor * v) override;

    };


    class ASTClassDecl : public AST {
    public:
        Symbol name;
        std::unique_ptr<ASTType> baseClass;
        std::vector<std::unique_ptr<ASTVarDecl>> fields;
        std::vector<std::unique_ptr<ASTFunDecl>> methods;

        /** If true the struct has also field definitions. Extra flag is necessary because empty fields can also mean an empty struct.
         */
        bool isDefinition = false;

    public:
        ASTClassDecl(Token const & t, Symbol name):
            AST{t},
            name{name} {
        }

    public:
        void print(ASTPrettyPrinter & p) const override {
            p << p.keyword << "class " << p.identifier << name.name();
            if (isDefinition) {
                p << p.symbol << "{";
                p.indent();
                for (auto & i : fields) {
                    p.newline();
                    i->print(p);
                }
                for (auto & i : methods) {
                    p.newline();
                    i->print(p);
                }
                p.dedent();
                p.newline();
                p << p.symbol << "}";
            }
        }

    protected:
        void accept(ASTVisitor * v) override;

    }; // class ASTClassDecl

    class ASTFunPtrDecl : public AST {
    public:
        std::unique_ptr<ASTIdentifier> name;
        std::vector<std::unique_ptr<ASTType>> args;
        std::unique_ptr<ASTType> returnType;

        ASTFunPtrDecl(Token const & t, std::unique_ptr<ASTIdentifier> name, std::unique_ptr<ASTType> returnType):
            AST{t},
            name{std::move(name)},
            returnType{std::move(returnType)} {
        }


        void print(ASTPrettyPrinter & p) const override {
            p << p.keyword << "typedef " << (*returnType) << p.symbol << "( *" << (*name) << p.symbol << ")(";
            auto i = args.begin();
            if (i != args.end()) {
                p << **i;
                while (++i != args.end())
                    p << p.symbol << ", " << **i;
            }
            p << p.symbol << ")";
        }

    protected:
        void accept(ASTVisitor * v) override;

    };

    class ASTIf : public AST {
    public:
        std::unique_ptr<AST> cond;
        std::unique_ptr<AST> trueCase;
        std::unique_ptr<AST> falseCase;

        ASTIf(Token const & t):
            AST{t} {
        }


        void print(ASTPrettyPrinter & p) const override {
            p << p.keyword << "if " << p.symbol << "(" << *cond << p.symbol << ")" << *trueCase;
            if (falseCase.get() != nullptr)
                p << p.keyword << "else" << *falseCase;
        }

    protected:
        void accept(ASTVisitor * v) override;

    };

    class ASTSwitch : public AST {
    public:
        std::unique_ptr<AST> cond;
        std::unique_ptr<AST> defaultCase;
        std::unordered_map<int, std::unique_ptr<AST>> cases;

        ASTSwitch(Token const & t):
            AST{t} {
        }


        void print(ASTPrettyPrinter & p) const override {
            p << p.keyword << "switch " << p.symbol << "(" << *cond << p.symbol << ") {";
            p.indent();
            for (auto & i : cases) {
                p.newline();
                p << p.keyword << "case " << p.numberLiteral << i.first << p.symbol << ":" << *i.second;
            }
            if (defaultCase.get() != nullptr) {
                p.newline();
                p << p.keyword << "default" << p.symbol << ":" << *defaultCase;
            }
            p.dedent();
            p.newline();
            p << p.symbol << "}";
        }

    protected:
        void accept(ASTVisitor * v) override;

    };

    class ASTWhile : public AST {
    public:
        std::unique_ptr<AST> cond;
        std::unique_ptr<AST> body;

        ASTWhile(Token const & t):
            AST{t} {
        }


        void print(ASTPrettyPrinter & p) const override {
            p << p.keyword << "while " << p.symbol << "(" << *cond << p.symbol << ")" << *body;
        }

    protected:
        void accept(ASTVisitor * v) override;

    };

    class ASTDoWhile : public AST {
    public:
        std::unique_ptr<AST> body;
        std::unique_ptr<AST> cond;

        ASTDoWhile(Token const & t):
            AST{t} {
        }


        void print(ASTPrettyPrinter & p) const override {
            p << p.keyword << "do" << *body << p.keyword << "while " << p.symbol << "(" << *cond << p.symbol << ")";
        }

    protected:
        void accept(ASTVisitor * v) override;

    };

    class ASTFor : public AST {
    public:
        std::unique_ptr<AST> init;
        std::unique_ptr<AST> cond;
        std::unique_ptr<AST> increment;
        std::unique_ptr<AST> body;

        ASTFor(Token const & t):
            AST{t} {
        }


        void print(ASTPrettyPrinter & p) const override {
            p << p.keyword << "for " << p.symbol << "(";
            if (init.get() != nullptr)
                p << *init;
            p << p.symbol << ";";
            if (cond.get() != nullptr)
                p << *cond;
            p << p.symbol << ";";
            if (increment.get() != nullptr)
                p << *increment;
            p << p.symbol << ")";
            p << *body;
        }

    protected:
        void accept(ASTVisitor * v) override;

    };

    class ASTBreak : public AST {
    public:
        ASTBreak(Token const & t):
            AST{t} {
        }


        void print(ASTPrettyPrinter & p) const override {
            p << p.keyword << "break";
        }

    protected:
        void accept(ASTVisitor * v) override;

    };

    class ASTContinue : public AST {
    public:
        ASTContinue(Token const & t):
            AST{t} {
        }


        void print(ASTPrettyPrinter & p) const override {
            p << p.keyword << "continue";
        }

    protected:
        void accept(ASTVisitor * v) override;

    };

    class ASTReturn : public AST {
    public:
        std::unique_ptr<AST> value;
        ASTReturn(Token const & t):
            AST{t} {
        }


        void print(ASTPrettyPrinter & p) const override {
            p << p.keyword << "return";
            if (value.get() != nullptr)
                p << " " << *value;
        }

    protected:
        void accept(ASTVisitor * v) override;

    };

    class ASTBinaryOp : public AST {
    public:
        Symbol op;
        std::unique_ptr<AST> left;
        std::unique_ptr<AST> right;

        ASTBinaryOp(Token const & t, std::unique_ptr<AST> left, std::unique_ptr<AST> right):
            AST{t},
            op{t.valueSymbol()},
            left{std::move(left)},
            right{std::move(right)} {
        }

        /** Whether a result of binary operator has an address depends on the operation and operands.
         */
        bool hasAddress() const override;


        void print(ASTPrettyPrinter & p) const override {
            p << *left << " " << p.symbol << op.name() << " " << *right;
        }

    protected:
        void accept(ASTVisitor * v) override;

    };

    class ASTAssignment : public AST {
    public:
        Symbol op;
        std::unique_ptr<AST> lvalue;
        std::unique_ptr<AST> value;

        ASTAssignment(Token const & t, std::unique_ptr<AST> lvalue, std::unique_ptr<AST> value):
            AST{t},
            op{t.valueSymbol()},
            lvalue{std::move(lvalue)},
            value{std::move(value)} {
        }

        /** Assignment result always has address of the lvalue it assigns to.
         */
        bool hasAddress() const override {
            return true;
        }


        void print(ASTPrettyPrinter & p) const override {
            p << *lvalue << " " << p.symbol << op.name() << " " << *value;
        }

    protected:
        void accept(ASTVisitor * v) override;

    };

    class ASTUnaryOp : public AST {
    public:
        Symbol op;
        std::unique_ptr<AST> arg;

        ASTUnaryOp(Token const & t, std::unique_ptr<AST> arg):
            AST{t},
            op{t.valueSymbol()},
            arg{std::move(arg)} {
        }

        /** Whether a result of unary operator has an address depends on the operation and operands.
         */
        bool hasAddress() const override;


        void print(ASTPrettyPrinter & p) const override {
            p << p.symbol << op.name() << *arg;
        }

    protected:
        void accept(ASTVisitor * v) override;

    };

    /** Post-increment and post-decrement.
     */
    class ASTUnaryPostOp : public AST {
    public:
        Symbol op;
        std::unique_ptr<AST> arg;

        ASTUnaryPostOp(Token const & t, std::unique_ptr<AST> arg):
            AST{t},
            op{t.valueSymbol()},
            arg{std::move(arg)} {
        }

        /** As the result of post-increment or decrement is the previous value, it is now a temporary and therefore does not have an address.
            NOTE: The default behavior is identical, but this method is included for clarity.
         */
        bool hasAddress() const override {
            return false;
        }


        void print(ASTPrettyPrinter & p) const override {
            p << *arg << p.symbol << op.name();
        }

    protected:
        void accept(ASTVisitor * v) override;

    };

    class ASTAddress : public AST {
    public:
        std::unique_ptr<AST> target;

        ASTAddress(Token const & t, std::unique_ptr<AST> target):
            AST{t},
            target{std::move(target)} {
        }


        void print(ASTPrettyPrinter & p) const override {
            p << p.symbol << "&" << *target;
        }

    protected:
        void accept(ASTVisitor * v) override;

    };

    class ASTDeref : public AST {
    public:
        std::unique_ptr<AST> target;

        ASTDeref(Token const & t, std::unique_ptr<AST> target):
            AST{t},
            target{std::move(target)} {
        }

        /** The dereferenced item always has address as it was obtained by following a pointer in the first place.
         */
        bool hasAddress() const override {
            return true;
        }


        void print(ASTPrettyPrinter & p) const override {
            p << p.symbol << "*" << *target;
        }

    protected:
        void accept(ASTVisitor * v) override;

    };

    class ASTIndex : public AST {
    public:
        std::unique_ptr<AST> base;
        std::unique_ptr<AST> index;

        ASTIndex(Token const & t, std::unique_ptr<AST> base, std::unique_ptr<AST> index):
            AST{t},
            base{std::move(base)},
            index{std::move(index)} {
        }

        /** If the base has address, then its element must have address too.
         */
        bool hasAddress() const override {
            return base->hasAddress();
        }


        void print(ASTPrettyPrinter & p) const override {
            p << *base << p.symbol << "[" << *index << p.symbol << "]";
        }

    protected:
        void accept(ASTVisitor * v) override;

    };

    class ASTMember : public AST {
    public:
        const Symbol op;
        std::unique_ptr<AST> base;
        std::unique_ptr<AST> member;

        ASTMember(Token const & t, std::unique_ptr<AST> base, std::unique_ptr<AST> member):
            AST{t},
            op{t.valueSymbol()},
            base{std::move(base)},
            member{std::move(member)} {
        }

        /** If the base has address, then its element must have address too.
         */
        bool hasAddress() const override {
            return base->hasAddress();
        }

        void print(ASTPrettyPrinter & p) const override {
            p << *base << p.symbol << op.name() << p.identifier << *member;
        }

    protected:
        void accept(ASTVisitor * v) override;

    };

    class ASTCall : public AST {
    public:
        std::unique_ptr<AST> function;
        std::vector<std::unique_ptr<AST>> args;

        ASTCall(Token const & t, std::unique_ptr<AST> function):
            AST{t},
            function{std::move(function)} {
        }


        void print(ASTPrettyPrinter & p) const override {
            p << *function << p.symbol << "(";
            auto i = args.begin();
            if (i != args.end()) {
                p << **i;
                while (++i != args.end())
                    p << p.symbol << **i;
            }
            p << p.symbol << ")";
        }

    protected:
        void accept(ASTVisitor * v) override;

    };

    class ASTCast : public AST {
    public:
        std::unique_ptr<AST> value;
        std::unique_ptr<ASTType> type;

        ASTCast(Token const & t, std::unique_ptr<AST> value, std::unique_ptr<ASTType> type):
            AST{t},
            value{std::move(value)},
            type{std::move(type)} {
        }

        /** Casts can only appear on right hand side of assignments.
         */
        bool hasAddress() const override {
            return false;
        }


        void print(ASTPrettyPrinter & p) const override {
            p << p.keyword << "cast" << p.symbol << "<" << (*type) << p.symbol << ">(" << *value << p.symbol << ")";
        }

    protected:
        void accept(ASTVisitor * v) override;

    };

    class ASTVisitor {
    public:

        virtual void visit(AST * ast) = 0;
        virtual void visit(ASTInteger * ast) = 0;
        virtual void visit(ASTDouble * ast) = 0;
        virtual void visit(ASTChar * ast) = 0;
        virtual void visit(ASTString * ast) = 0;
        virtual void visit(ASTIdentifier * ast) = 0;
        virtual void visit(ASTType * ast) = 0;
        virtual void visit(ASTPointerType * ast) = 0;
        virtual void visit(ASTArrayType * ast) = 0;
        virtual void visit(ASTNamedType * ast) = 0;
        virtual void visit(ASTSequence * ast) = 0;
        virtual void visit(ASTBlock * ast) = 0;
        virtual void visit(ASTVarDecl * ast) = 0;
        virtual void visit(ASTFunDecl * ast) = 0;
        virtual void visit(ASTStructDecl * ast) = 0;
        virtual void visit(ASTClassDecl * ast) = 0;
        virtual void visit(ASTFunPtrDecl * ast) = 0;
        virtual void visit(ASTIf * ast) = 0;
        virtual void visit(ASTSwitch * ast) = 0;
        virtual void visit(ASTWhile * ast) = 0;
        virtual void visit(ASTDoWhile * ast) = 0;
        virtual void visit(ASTFor * ast) = 0;
        virtual void visit(ASTBreak * ast) = 0;
        virtual void visit(ASTContinue * ast) = 0;
        virtual void visit(ASTReturn * ast) = 0;
        virtual void visit(ASTBinaryOp * ast) = 0;
        virtual void visit(ASTAssignment * ast) = 0;
        virtual void visit(ASTUnaryOp * ast) = 0;
        virtual void visit(ASTUnaryPostOp * ast) = 0;
        virtual void visit(ASTAddress * ast) = 0;
        virtual void visit(ASTDeref * ast) = 0;
        virtual void visit(ASTIndex * ast) = 0;
        virtual void visit(ASTMember * ast) = 0;
        virtual void visit(ASTCall * ast) = 0;
        virtual void visit(ASTCast * ast) = 0;

    protected:

        void visitChild(AST * child) {
            child->accept(this);
        }

    }; // tinycpp::ASTVisitor

    inline void AST::accept(ASTVisitor * v) { v->visit(this); }
    inline void ASTInteger::accept(ASTVisitor * v) { v->visit(this); }
    inline void ASTDouble::accept(ASTVisitor* v) { v->visit(this); }
    inline void ASTChar::accept(ASTVisitor * v) { v->visit(this); }
    inline void ASTString::accept(ASTVisitor * v) { v->visit(this); }
    inline void ASTIdentifier::accept(ASTVisitor * v) { v->visit(this); }
    inline void ASTType::accept(ASTVisitor * v) { v->visit(this); }
    inline void ASTPointerType::accept(ASTVisitor * v) { v->visit(this); }
    inline void ASTArrayType::accept(ASTVisitor * v) { v->visit(this); }
    inline void ASTNamedType::accept(ASTVisitor * v) { v->visit(this); }
    inline void ASTSequence::accept(ASTVisitor * v) { v->visit(this); }
    inline void ASTBlock::accept(ASTVisitor * v) { v->visit(this); }
    inline void ASTVarDecl::accept(ASTVisitor * v) { v->visit(this); }
    inline void ASTFunDecl::accept(ASTVisitor * v) { v->visit(this); }
    inline void ASTClassDecl::accept(ASTVisitor * v) { v->visit(this); }
    inline void ASTStructDecl::accept(ASTVisitor * v) { v->visit(this); }
    inline void ASTFunPtrDecl::accept(ASTVisitor * v) { v->visit(this); }
    inline void ASTIf::accept(ASTVisitor * v) { v->visit(this); }
    inline void ASTSwitch::accept(ASTVisitor * v) { v->visit(this); }
    inline void ASTWhile::accept(ASTVisitor * v) { v->visit(this); }
    inline void ASTDoWhile::accept(ASTVisitor * v) { v->visit(this); }
    inline void ASTFor::accept(ASTVisitor * v) { v->visit(this); }
    inline void ASTBreak::accept(ASTVisitor * v) { v->visit(this); }
    inline void ASTContinue::accept(ASTVisitor * v) { v->visit(this); }
    inline void ASTReturn::accept(ASTVisitor * v) { v->visit(this); }
    inline void ASTBinaryOp::accept(ASTVisitor * v) { v->visit(this); }
    inline void ASTAssignment::accept(ASTVisitor * v) { v->visit(this); }
    inline void ASTUnaryOp::accept(ASTVisitor * v) { v->visit(this); }
    inline void ASTUnaryPostOp::accept(ASTVisitor * v) { v->visit(this); }
    inline void ASTAddress::accept(ASTVisitor * v) { v->visit(this); }
    inline void ASTDeref::accept(ASTVisitor * v) { v->visit(this); }
    inline void ASTIndex::accept(ASTVisitor * v) { v->visit(this); }
    inline void ASTMember::accept(ASTVisitor * v) { v->visit(this); }
    inline void ASTCall::accept(ASTVisitor * v) { v->visit(this); }
    inline void ASTCast::accept(ASTVisitor * v) { v->visit(this); }

} // namespace tinycpp
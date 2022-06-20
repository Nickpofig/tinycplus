#pragma once

// standard
#include <unordered_map>
#include <memory>
#include <optional>

// internal
#include "shared.h"

namespace tinycplus {

    class Type;
    class ASTVisitor;

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
        template<typename T> T * as() {
            return dynamic_cast<T*>(this);
        }
        virtual void print(ASTPrettyPrinter & p) const = 0;
        // {
        //     /// NOTE: In this transpiler [print] function of ASTBase will not be used
        //     throw std::runtime_error("AST pretty print is not implemented");
        // };
    // -----Type support----
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
    // ----AST Visitor support----
    protected:
        friend class ASTVisitor;
        virtual void accept(ASTVisitor * v) = 0;
    };




    class ASTInteger : public AST {
    public:
        int64_t value;
    public:
        ASTInteger(Token const & t):
            AST{t},
            value{t.valueInt()} {
        }
    protected:
        void accept(ASTVisitor * v) override;
        void print(ASTPrettyPrinter & p) const override {
            p << value;
        }
    };




    class ASTDouble : public AST {
    public:
        double value;
    public:
        ASTDouble(Token const & t):
            AST{t},
            value{t.valueDouble()} {
        }
    protected:
        void accept(ASTVisitor * v) override;
        void print(ASTPrettyPrinter & p) const override {
            p << value;
        }
    };




    class ASTChar : public AST {
    public:
        char value;
    public:
        ASTChar(Token const & t):
            AST{t} {
            std::string const & s{t.valueString()};
            if (t == Token::Kind::StringDoubleQuoted)
                throw ParserError(STR("Expected character (single quote), but string \"" << s << "\" (double quote) found"), t.location(), false);
            if (s.size() != 1)
                throw ParserError(STR("Expected single character, but " << s.size() << " characters found in '" << s << "'"), t.location(), false);
            value = s[0];
        }
    public:
        void print(ASTPrettyPrinter & p) const override {
            p << value;
        }
    protected:
        void accept(ASTVisitor * v) override;
    };




    class ASTString : public AST {
    public:
        std::string value;
    public:
        ASTString(Token const & t):
            AST{t} {
            std::string const & s{t.valueString()};
            if (t == Token::Kind::StringDoubleQuoted)
                throw ParserError(STR("Expected string (double quote), but character '" << s << "' (single quote) found"), t.location(), false);
            value = s;
        }
    protected:
        void accept(ASTVisitor * v) override;
    public:
        void print(ASTPrettyPrinter & p) const override {
            p << value;
        }
    };




    class ASTIdentifier : public AST {
    public:
        Symbol name;

        ASTIdentifier(Token const & t):
            AST{t},
            name{t.valueSymbol()} {
        }
    public:
        /** An identifier in read position is a variable read and all variables have addresses.
         */
        bool hasAddress() const override {
            return true;
        }
        void print(ASTPrettyPrinter & p) const override {
            p << name.name();
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
    public:
        ASTPointerType(Token const & t, std::unique_ptr<ASTType> base):
            ASTType{t},
            base{std::move(base)} {
        }
    public:
        void print(ASTPrettyPrinter & p) const override {
            base->print(p);
            p << "*";
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
    public:
        ASTArrayType(Token const & t, std::unique_ptr<ASTType> base, std::unique_ptr<AST> size):
            ASTType{t},
            base{std::move(base)},
            size{std::move(size)} {
        }
    public:
        void print(ASTPrettyPrinter & p) const override {
            base->print(p);
            p << "[]";
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
    public:
        ASTNamedType(Token const & t) :
            ASTType{t},
            name{t.valueSymbol()} {
        }
    public:
        void print(ASTPrettyPrinter & p) const override {
            p << name.name();
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
    public:
        ASTSequence(Token const & t):
            AST{t} {
        }
    public:
        /** The result of sequence has address if its last element has address as the last element is what is returned.
         */
        bool hasAddress() const override {
            return body.empty()
                ? false
                : body.back()->hasAddress();
        }
        void print(ASTPrettyPrinter & p) const override {
            p.newline();
            p.indent();
            for (auto & it : body) {
                it->print(p); p.newline();
                std::cout.flush();
            }
            p.dedent();
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
    protected:
        void accept(ASTVisitor * v) override;
    };




    class ASTProgram : public ASTSequence {
    public:
        ASTProgram(Token const & t): ASTSequence{t} { }
    public:
        void print(ASTPrettyPrinter & p) const override {
            for (auto & it : body) {
                it->print(p); p.newline();
            }
        }
    protected:
        void accept(ASTVisitor * v) override;
    };




    enum class AccessMod {
        None,
        Public,
        Private,
        Protected,
    };

    class ASTVarDecl : public AST {
    public:
        std::unique_ptr<ASTType> type;
        std::unique_ptr<ASTIdentifier> name;
        std::unique_ptr<AST> value;
        AccessMod access = AccessMod::None;
    public:
        ASTVarDecl(Token const & t, std::unique_ptr<ASTType> type):
            AST{t},
            type{std::move(type)} {
        }
    public:
        void print(ASTPrettyPrinter & p) const override {
            switch(access) {
                case AccessMod::Public: p << "public "; break;
                case AccessMod::Private: p << "private "; break;
                case AccessMod::Protected: p << "protected "; break;
            }
            p << "variable ("; name->print(p); p << "):";
            p.newline();
            p.indent();
            {
                p << "type: "; type->print(p); p.newline();
                if (value) {
                    p << "value: "; value->print(p); p.newline();
                }
            }
            p.dedent();
        }
    protected:
        void accept(ASTVisitor * v) override;
    };




    class ASTFunPtrDecl : public AST {
    public:
        std::unique_ptr<ASTIdentifier> name;
        std::vector<std::unique_ptr<ASTType>> args;
        std::unique_ptr<ASTType> returnType;
    public:
        ASTFunPtrDecl(Token const & t, std::unique_ptr<ASTIdentifier> name, std::unique_ptr<ASTType> returnType):
            AST{t},
            name{std::move(name)},
            returnType{std::move(returnType)} {
        }
    public:
        void print(ASTPrettyPrinter & p) const override {
            p << "function pointer type ("; name->print(p); p << "):";
            p.newline();
            p.indent();
            {
                p << "return type: "; returnType->print(p); p.newline();
                if (args.size() > 0) {
                    p << "args:";
                    p.indent();
                    p.newline();
                    for (auto & it : args) {
                        it->print(p); p.newline();
                    }
                    p.dedent();
                } else {
                    p << "[no args]"; p.newline();
                }
            }
            p.dedent();
        }
    protected:
        void accept(ASTVisitor * v) override;
    };




    class ASTPartialDecl : public AST {
    public:
        /**
         * false -> is declaration only
         * true -> is declartion with definition
        **/
        bool isDefinition = false;
    public:
        ASTPartialDecl(Token const & t): AST{t} { }
    };




    class ASTFunDecl : public ASTPartialDecl {
    public:
        struct Base {
            std::unique_ptr<ASTType> name;
            std::vector<std::unique_ptr<ASTIdentifier>> args;
            Symbol getName() const { return name->as<ASTNamedType>()->name; }
        };
        enum class Virtuality {
            None,
            Virtual,
            Abstract,
            Override,
        };
        Virtuality virtuality;
    public:
        FunctionKind kind;
        AccessMod access = AccessMod::None;
        std::unique_ptr<ASTType> typeDecl;
        std::vector<std::unique_ptr<ASTVarDecl>> args;
        std::unique_ptr<AST> body;
        std::optional<Symbol> name;
        std::optional<Base> base;
    public:
        ASTFunDecl(Token const & t, std::unique_ptr<ASTType> type)
            :ASTPartialDecl{t}
            ,typeDecl{std::move(type)}
        { }
    public:
        bool isClassMethod() const { return kind == FunctionKind::ClassMethod; }
        bool isClassConstructor() const { return kind == FunctionKind::ClassConstructor; }
        bool isInterfaceMethod() const { return kind == FunctionKind::InterfaceMethod; }
        bool isPureFunction() const { return !isClassMethod() && !isClassConstructor(); }
        bool isAbstract() const { return virtuality == Virtuality::Abstract; }
        bool isVirtual() const { return virtuality == Virtuality::Virtual; }
        bool isOverride() const { return virtuality == Virtuality::Override; }
        bool isVirtualized() const { return isVirtual() || isOverride() || isAbstract(); }

        void print(ASTPrettyPrinter & p) const override {
            switch (virtuality)
            {
                case Virtuality::Abstract: p << "abstract"; break;
                case Virtuality::Virtual: p << "virtual"; break;
                case Virtuality::Override: p << "override"; break;
            }
            p << " ";
            switch (kind)
            {
                case FunctionKind::ClassMethod: p << "class method"; break;
                case FunctionKind::ClassConstructor: p << "class constructor"; break;
                case FunctionKind::InterfaceMethod: p << "interface method"; break;
                case FunctionKind::None: p << "function"; break;
            }
            p << " (" << name->name() << "):";
            p.newline();
            p.indent();
            {
                p << "return type: "; typeDecl->print(p); p.newline();
                if (base) {
                    p << "base ("; base->name->print(p); p << "):";
                    p.indent();
                    p.newline();
                    for (auto & it: base->args) {
                        it->print(p);
                        p.newline();
                    }
                    p.dedent();
                }
                if (args.size() > 0) {
                    p << "args:";
                    p.indent();
                    p.newline();
                    for (auto & it : args) {
                        it->print(p); p.newline();
                    }
                    p.dedent();
                } else {
                    p << "[no args]"; p.newline();
                    p.newline();
                }
                if (body) { 
                    p << "body: ";
                    p.indent();
                    p.newline();
                    body->print(p);
                    p.dedent();
                }
            }
            p.dedent();
        }
    protected:
        void accept(ASTVisitor * v) override;
    };




    class ASTStructDecl : public ASTPartialDecl {
    public:
        Symbol name;
        std::vector<std::unique_ptr<ASTVarDecl>> fields;
    public:
        ASTStructDecl(Token const & t, Symbol name):
            ASTPartialDecl{t},
            name{name} {
        }
    public:
        void print(ASTPrettyPrinter & p) const override {
            p << "struct (" << name.name() << "):";
            p.newline();
            p.indent();
            {
                // methods
                if (fields.size() > 0) {
                    p << "fields: ";
                    p.indent();
                    p.newline();
                    for (auto & it : fields) {
                        it->print(p); p.newline();
                    }
                    p.dedent();
                } else {
                    p << "[no fields]"; p.newline();
                }
            }
            p.dedent();
        }
    protected:
        void accept(ASTVisitor * v) override;
    };




    class ASTInterfaceDecl : public ASTPartialDecl {
    public:
        Symbol name;
        std::vector<std::unique_ptr<ASTFunDecl>> methods;
    public:
        ASTInterfaceDecl(Token const & t, Symbol name):
            ASTPartialDecl{t},
            name{name}
        { }
    public:
        void print(ASTPrettyPrinter & p) const override {
            p << "interface (" << name.name() << "):";
            p.newline();
            p.indent();
            {
                // methods
                if (methods.size() > 0) {
                    p << "methods: ";
                    p.indent();
                    p.newline();
                    for (auto & it : methods) {
                        it->print(p); p.newline();
                    }
                    p.dedent();
                } else {
                    p << "[no methods]"; p.newline();
                }
            }
            p.dedent();
        }
    protected:
        void accept(ASTVisitor * v) override;
    };




    class ASTClassDecl : public ASTPartialDecl {
    public:
        Symbol name;
        std::unique_ptr<ASTType> baseClass;
        std::vector<std::unique_ptr<ASTType>> interfaces;
        std::vector<std::unique_ptr<ASTVarDecl>> fields;
        std::vector<std::unique_ptr<ASTFunDecl>> methods;
        std::vector<std::unique_ptr<ASTFunDecl>> constructors;
    public:
        ASTClassDecl(Token const & t, Symbol name):
            ASTPartialDecl{t},
            name{name} {
        }
    public:
        void print(ASTPrettyPrinter & p) const override {
            p << "class (" << name.name() << "):";
            p.newline();
            p.indent();
            {
                if (baseClass) {
                    p << "base: "; baseClass->print(p); p.newline();
                }
                // interfaces
                if (interfaces.size() > 0) {
                    p << "interfaces: ";
                    p.indent();
                    p.newline();
                    for (auto & it : interfaces) {
                        it->print(p); p.newline();
                    }
                    p.dedent();
                } else {
                    p << "[no interfaces]"; p.newline();
                }
                if (constructors.size() > 0) {
                    // constructors
                    p << "fields: ";
                    p.indent();
                    p.newline();
                    for (auto & it : fields) {
                        it->print(p); p.newline();
                    }
                    p.dedent();
                } else {
                    p << "[no fields]"; p.newline();
                }
                if (fields.size() > 0) {
                    // constructors
                    p << "constructors: ";
                    p.indent();
                    p.newline();
                    for (auto & it : constructors) {
                        it->print(p); p.newline();
                    }
                    p.dedent();
                } else {
                    p << "[no constructors]"; p.newline();
                }
                if (methods.size() > 0) {
                    // methods
                    p << "methods: ";
                    p.indent();
                    p.newline();
                    for (auto & it : methods) {
                        it->print(p); p.newline();
                    }
                    p.dedent();
                } else {
                    p << "[no methods]"; p.newline();
                }
            }
            p.dedent();
        }
    protected:
        void accept(ASTVisitor * v) override;

    }; // class ASTClassDecl




    class ASTIf : public AST {
    public:
        std::unique_ptr<AST> cond;
        std::unique_ptr<AST> trueCase;
        std::unique_ptr<AST> falseCase;
    public:
        ASTIf(Token const & t):
            AST{t} {
        }
    public:
        void print(ASTPrettyPrinter & p) const override {
            p << "if:";
            p.newline();
            p.indent();
            {
                p << "cond: "; cond->print(p); p.newline();
                p << "true case: "; trueCase->print(p); p.newline();
                p << "false case: "; trueCase->print(p); p.newline();
            }
            p.dedent();
        }
    protected:
        void accept(ASTVisitor * v) override;
    };




    class ASTSwitch : public AST {
    public:
        std::unique_ptr<AST> cond;
        std::unique_ptr<AST> defaultCase;
        std::unordered_map<int, std::unique_ptr<AST>> cases;
    public:
        ASTSwitch(Token const & t):
            AST{t} {
        }
    public:
        void print(ASTPrettyPrinter & p) const override {
            p << "switch:";
            p.newline();
            p.indent();
            {
                p << "default case:"; defaultCase->print(p); p.newline();
                p << "cond: "; cond->print(p); p.newline();
                p.indent();
                for (auto & it : cases) {
                    p << "case " << it.first << ": ";
                    it.second->print(p); p.newline();
                }
                p.dedent();
            }
            p.dedent();
        }
    protected:
        void accept(ASTVisitor * v) override;
    };




    class ASTWhile : public AST {
    public:
        std::unique_ptr<AST> cond;
        std::unique_ptr<AST> body;
    public:
        ASTWhile(Token const & t):
            AST{t} {
        }
    public:
        void print(ASTPrettyPrinter & p) const override {
            p << "while:";
            p.newline();
            p.indent();
            {
                p << "cond: "; cond->print(p); p.newline();
                p << "body: "; body->print(p); p.newline();
            }
            p.dedent();
        }
    protected:
        void accept(ASTVisitor * v) override;
    };




    class ASTDoWhile : public AST {
    public:
        std::unique_ptr<AST> body;
        std::unique_ptr<AST> cond;
    public:
        ASTDoWhile(Token const & t):
            AST{t} {
        }
    public:
        void print(ASTPrettyPrinter & p) const override {
            p << "dowhile:";
            p.newline();
            p.indent();
            {
                p << "cond: "; cond->print(p); p.newline();
                p << "body: "; body->print(p); p.newline();
            }
            p.dedent();
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
    public:
        ASTFor(Token const & t):
            AST{t} {
        }
    public:
        void print(ASTPrettyPrinter & p) const override {
            p << "for:";
            p.newline();
            p.indent();
            {
                p << "init: "; init->print(p); p.newline();
                p << "cond: "; cond->print(p); p.newline();
                p << "body: "; body->print(p); p.newline();
            }
            p.dedent();
        }
    protected:
        void accept(ASTVisitor * v) override;
    };




    class ASTBreak : public AST {
    public:
        ASTBreak(Token const & t):
            AST{t} {
        }
    public:
        void print(ASTPrettyPrinter & p) const override {
            p << "break";
        }
    protected:
        void accept(ASTVisitor * v) override;
    };




    class ASTContinue : public AST {
    public:
        ASTContinue(Token const & t):
            AST{t} {
        }
    public:
        void print(ASTPrettyPrinter & p) const override {
            p << "continue";
        }
    protected:
        void accept(ASTVisitor * v) override;
    };




    class ASTReturn : public AST {
    public:
        std::unique_ptr<AST> value;
    public:
        ASTReturn(Token const & t):
            AST{t} {
        }
    public:
        void print(ASTPrettyPrinter & p) const override {
            p << "return ";
            value->print(p);
        }
    protected:
        void accept(ASTVisitor * v) override;
    };




    class ASTBinaryOp : public AST {
    public:
        Symbol op;
        std::unique_ptr<AST> left;
        std::unique_ptr<AST> right;
    public:
        ASTBinaryOp(Token const & t, std::unique_ptr<AST> left, std::unique_ptr<AST> right):
            AST{t},
            op{t.valueSymbol()},
            left{std::move(left)},
            right{std::move(right)} {
        }
    public:
        /** Whether a result of binary operator has an address depends on the operation and operands.
         */
        bool hasAddress() const override;
        void print(ASTPrettyPrinter & p) const override {
            p << "binary op (" << op.name() << ")";
            p.newline();
            p.indent();
            {
                p << "left: "; left->print(p); p.newline();
                p << "right: "; right->print(p); p.newline();
            }
            p.dedent();
        }
    protected:
        void accept(ASTVisitor * v) override;
    };




    class ASTAssignment : public AST {
    public:
        Symbol op;
        std::unique_ptr<AST> lvalue;
        std::unique_ptr<AST> value;
    public:
        ASTAssignment(Token const & t, std::unique_ptr<AST> lvalue, std::unique_ptr<AST> value):
            AST{t},
            op{t.valueSymbol()},
            lvalue{std::move(lvalue)},
            value{std::move(value)} {
        }
    public:
        /** Assignment result always has address of the lvalue it assigns to.
         */
        bool hasAddress() const override {
            return true;
        }
        void print(ASTPrettyPrinter & p) const override {
            p << "assingment (" << op.name() << ")";
            p.newline();
            p.indent();
            {
                p << "lvalue: "; lvalue->print(p); p.newline();
                p << "value: "; value->print(p); p.newline();
            }
            p.dedent();
        }
    protected:
        void accept(ASTVisitor * v) override;
    };




    class ASTUnaryOp : public AST {
    public:
        Symbol op;
        std::unique_ptr<AST> arg;
    public:
        ASTUnaryOp(Token const & t, std::unique_ptr<AST> arg):
            AST{t},
            op{t.valueSymbol()},
            arg{std::move(arg)} {
        }
    public:
        /** Whether a result of unary operator has an address depends on the operation and operands.
         */
        bool hasAddress() const override;
        void print(ASTPrettyPrinter & p) const override {
            p << "unary op (" << op.name() << ")";
            p.newline();
            p.indent();
            {
                p << "arg: "; arg->print(p); p.newline();
            }
            p.dedent();
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
    public:
        ASTUnaryPostOp(Token const & t, std::unique_ptr<AST> arg):
            AST{t},
            op{t.valueSymbol()},
            arg{std::move(arg)} {
        }
    public:
        /** As the result of post-increment or decrement is the previous value, it is now a temporary and therefore does not have an address.
            NOTE: The default behavior is identical, but this method is included for clarity.
         */
        bool hasAddress() const override {
            return false;
        }
        void print(ASTPrettyPrinter & p) const override {
            p << "unary op post (" << op.name() << ")";
            p.newline();
            p.indent();
            {
                p << "arg: "; arg->print(p); p.newline();
            }
            p.dedent();
        }
    protected:
        void accept(ASTVisitor * v) override;
    };




    class ASTAddress : public AST {
    public:
        std::unique_ptr<AST> target;
    public:
        ASTAddress(Token const & t, std::unique_ptr<AST> target):
            AST{t},
            target{std::move(target)} {
        }
    public:
        void print(ASTPrettyPrinter & p) const override {
            p << "get address";
            p.newline();
            p.indent();
            {
                p << "target: "; target->print(p); p.newline();
            }
            p.dedent();
        }
    protected:
        void accept(ASTVisitor * v) override;
    };




    class ASTDeref : public AST {
    public:
        std::unique_ptr<AST> target;
    public:
        ASTDeref(Token const & t, std::unique_ptr<AST> target):
            AST{t},
            target{std::move(target)} {
        }
    public:
        /** The dereferenced item always has address as it was obtained by following a pointer in the first place.
         */
        bool hasAddress() const override {
            return true;
        }
        void print(ASTPrettyPrinter & p) const override {
            p << "get value from addres";
            p.newline();
            p.indent();
            {
                p << "target: "; target->print(p); p.newline();
            }
            p.dedent();
        }
    protected:
        void accept(ASTVisitor * v) override;
    };




    class ASTIndex : public AST {
    public:
        std::unique_ptr<AST> base;
        std::unique_ptr<AST> index;
    public:
        ASTIndex(Token const & t, std::unique_ptr<AST> base, std::unique_ptr<AST> index):
            AST{t},
            base{std::move(base)},
            index{std::move(index)} {
        }
    public:
        /** If the base has address, then its element must have address too.
         */
        bool hasAddress() const override {
            return base->hasAddress();
        }
        void print(ASTPrettyPrinter & p) const override {
            p << "get at index";
            p.newline();
            p.indent();
            {
                p << "from: "; base->print(p); p.newline();
                p << "index: "; index->print(p); p.newline();
            }
            p.dedent();
        }
    protected:
        void accept(ASTVisitor * v) override;
    };




    class ASTMember : public AST {
    public:
        const Symbol op;
        std::unique_ptr<AST> base;
        std::unique_ptr<AST> member;
    public:
        ASTMember(Token const & t, std::unique_ptr<AST> base, std::unique_ptr<AST> member):
            AST{t},
            op{t.valueSymbol()},
            base{std::move(base)},
            member{std::move(member)} {
        }
    public:
        /** If the base has address, then its element must have address too.
         */
        bool hasAddress() const override {
            return base->hasAddress();
        }
        void print(ASTPrettyPrinter & p) const override {
            p << "access (" << op.name() << ")";
            p.newline();
            p.indent();
            {
                p << "from: "; base->print(p); p.newline();
                p << "member: "; member->print(p); p.newline();
            }
            p.dedent();
        }
    protected:
        void accept(ASTVisitor * v) override;
    };




    class ASTCall : public AST {
    public:
        std::unique_ptr<AST> function;
        std::vector<std::unique_ptr<AST>> args;
    public:
        ASTCall(Token const & t, std::unique_ptr<AST> function):
            AST{t},
            function{std::move(function)} {
        }
    public:
        void print(ASTPrettyPrinter & p) const override {
            p << "call";
            p.newline();
            p.indent();
            {
                p << "function: "; function->print(p); p.newline();
                if (args.size() > 0) {
                    p << "args: ";
                    p.indent();
                    for (auto & it : args){
                        p << "1: ";
                        it->print(p);
                        p.newline();
                    }
                    p.dedent();
                } else {
                    p << "[no args]"; p.newline();
                }
            }
            p.dedent();
        }
    protected:
        void accept(ASTVisitor * v) override;
    };




    class ASTCast : public AST {
    public:
        std::unique_ptr<AST> value;
        std::unique_ptr<ASTType> type;
    public:
        ASTCast(Token const & t, std::unique_ptr<AST> value, std::unique_ptr<ASTType> type):
            AST{t},
            value{std::move(value)},
            type{std::move(type)} {
        }
    public:
        /** Casts can only appear on right hand side of assignments.
         */
        bool hasAddress() const override {
            return false;
        }
        void print(ASTPrettyPrinter & p) const override {
            p << "cast";
            p.newline();
            p.indent();
            {
                p << "what: "; value->print(p); p.newline();
                p << "toType: "; type->print(p); p.newline();
            }
            p.dedent();
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
        virtual void visit(ASTProgram * ast) = 0;
        virtual void visit(ASTVarDecl * ast) = 0;
        virtual void visit(ASTFunDecl * ast) = 0;
        virtual void visit(ASTFunPtrDecl * ast) = 0;
        virtual void visit(ASTStructDecl * ast) = 0;
        virtual void visit(ASTInterfaceDecl * ast) = 0;
        virtual void visit(ASTClassDecl * ast) = 0;
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
    }; // tinycplus::ASTVisitor

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
    inline void ASTProgram::accept(ASTVisitor * v) { v->visit(this); }
    inline void ASTVarDecl::accept(ASTVisitor * v) { v->visit(this); }
    inline void ASTFunDecl::accept(ASTVisitor * v) { v->visit(this); }
    inline void ASTFunPtrDecl::accept(ASTVisitor * v) { v->visit(this); }
    inline void ASTStructDecl::accept(ASTVisitor * v) { v->visit(this); }
    inline void ASTInterfaceDecl::accept(ASTVisitor * v) { v->visit(this); }
    inline void ASTClassDecl::accept(ASTVisitor * v) { v->visit(this); }
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

} // namespace tinycplus
module;

#include <memory>
#include <vector>
#include <variant>

export module frontend.ast;

import frontend.lexicals;

export namespace DerkJS {
    enum class AstOp : uint8_t {
        ast_op_precent,
        ast_op_times,
        ast_op_slash,
        ast_op_plus,
        ast_op_minus,
        ast_op_bang,
        ast_op_equal,
        ast_op_bang_equal,
        ast_op_strict_equal,
        ast_op_strict_bang_equal,
        ast_op_less,
        ast_op_less_equal,
        ast_op_greater,
        ast_op_greater_equal,
        ast_op_assign,
        ast_op_times_assign,
        ast_op_slash_assign,
        ast_op_plus_assign,
        ast_op_minus_assign,
    };

    template <typename ... ExprKind>
    struct Expr {
        std::variant<ExprKind...> data;
        int src_id;
        int text_begin;
        int text_length;
    };

    struct Primitive;
    struct Unary;
    struct Binary;
    struct Assign;
    struct Call;

    using ExprPtr = std::unique_ptr<Expr<Primitive, Unary, Binary, Assign, Call>>;

    struct Primitive {
        Token token;
    };

    struct Unary {
        ExprPtr inner;
        AstOp op;
    };

    struct Binary {
        ExprPtr lhs;
        ExprPtr rhs;
        AstOp op;
    };

    struct Assign {
        ExprPtr lhs;
        ExprPtr rhs;
    };

    struct Call {
        std::vector<ExprPtr> args;
        ExprPtr callee;
    };

    template <typename ... StmtKind>
    struct Stmt {
        std::variant<StmtKind...> data;
        int src_id;
        int text_begin;
        int text_length;
    };

    struct ExprStmt;
    struct VarDecl;
    struct Variables;
    struct Return;
    struct Block;
    struct FunctionDecl;

    using StmtPtr = std::unique_ptr<Stmt<ExprStmt, VarDecl, Variables, Return, Block, FunctionDecl>>;

    struct ExprStmt {
        ExprPtr expr;
    };

    struct VarDecl {
        Token name;
        ExprPtr rhs;
    };
    
    struct Variables {
        std::vector<VarDecl> vars;
    };

    struct Return {
        ExprPtr result;
    };

    struct Block {
        std::vector<StmtPtr> items;
    };

    struct FunctionDecl {
        std::vector<Token> params;
        Token name;
        StmtPtr body;
    };



    struct SourcedAst {
        StmtPtr decl;
        int src_id;
    };

    using ASTUnit = std::vector<SourcedAst>;
}
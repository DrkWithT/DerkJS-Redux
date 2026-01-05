module;

#include <memory>
#include <vector>
#include <variant>

export module frontend.ast;

import frontend.lexicals;

export namespace DerkJS {
    enum class AstOp : uint8_t {
        ast_op_noop,
        ast_op_new, // 'new' for objects
        ast_op_dot_access, // access member by `'.'`
        ast_op_index_access, // access member by `<lhs>[...]`
        ast_op_percent,
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
        ast_op_and,
        ast_op_or,
        ast_op_assign,
        ast_op_times_assign,
        ast_op_slash_assign,
        ast_op_plus_assign,
        ast_op_minus_assign,
    };

    template <typename ... ExprKind>
    struct ExprNode {
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

    using Expr = ExprNode<Primitive, Unary, Binary, Assign, Call>;
    using ExprPtr = std::unique_ptr<Expr>;

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
    struct StmtNode {
        std::variant<StmtKind...> data;
        int src_id;
        int text_begin;
        int text_length;
    };

    struct ExprStmt;
    struct VarDecl;
    struct Variables;
    struct If;
    struct Return;
    struct While;
    struct Block;
    struct FunctionDecl;

    using Stmt = StmtNode<ExprStmt, Variables, If, Return, While, Block, FunctionDecl>;
    using StmtPtr = std::unique_ptr<Stmt>;

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

    struct While {
        ExprPtr check;
        StmtPtr body;
    };

    struct If {
        ExprPtr check;
        StmtPtr body_true;
        StmtPtr body_false;
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
        std::string source_filename;
        StmtPtr decl;
        int src_id;
    };

    using ASTUnit = std::vector<SourcedAst>;
}
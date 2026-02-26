module;

#include <memory>
#include <vector>
#include <variant>

export module frontend.ast;

export import frontend.lexicals;

export namespace DerkJS {
    /// NOTE: runtime tag of AST expression types for looking up bytecode generation handlers in `bc_generate.ixx`!
    enum class ExprNodeTag : uint8_t {
        primitive, object_literal, array_literal, lambda_literal,
        member_access,
        unary, binary,
        assign, call,
        last
    };

    enum class StmtNodeTag : uint8_t {
        stmt_expr_stmt,
        stmt_variables,
        stmt_if, stmt_return, stmt_while, stmt_break, stmt_continue, stmt_block,
        stmt_throw, stmt_try_catch,
        last
    };

    enum class AstOp : uint8_t {
        ast_op_noop,
        ast_op_prefix_inc,      // prefix '++' operator
        ast_op_prefix_dec,      // prefix '--' operator
        ast_op_new,             // 'new' for objects
        ast_op_void,            // 'void' for discarded evaluations
        ast_op_typeof,          // `typeof` for getting a typename string of exprs.
        ast_op_dot_access,      // access member by `'.'`
        ast_op_index_access,    // access member by `<lhs>[...]`
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

    /// BEGIN FORWARD DECLS ///

    template <typename ... StmtKind>
    struct StmtNode {
        std::variant<StmtKind...> data;
        int src_id;
        int text_begin;
        int text_length;
        StmtNodeTag tag;
    };

    struct ExprStmt;
    struct VarDecl;
    struct Variables;
    struct If;
    struct Return;
    struct While;
    struct Break;
    struct Continue;
    struct Block;
    struct Throw;
    struct TryCatch;

    using Stmt = StmtNode<ExprStmt, Variables, If, Return, While, Break, Continue, Block, Throw, TryCatch>;
    using StmtPtr = std::unique_ptr<Stmt>;

    template <typename ... ExprKind>
    struct ExprNode {
        std::variant<ExprKind...> data;
        int src_id;
        int text_begin;
        int text_length;
        ExprNodeTag tag;
    };

    struct Primitive;
    struct ObjectLiteral;
    struct ArrayLiteral;
    struct LambdaLiteral;
    struct MemberAccess;
    struct Unary;
    struct Binary;
    struct Assign;
    struct Call;

    using Expr = ExprNode<Primitive, ObjectLiteral, ArrayLiteral, LambdaLiteral, MemberAccess, Unary, Binary, Assign, Call>;
    using ExprPtr = std::unique_ptr<Expr>;

    /// BEGIN NODE DEFINITIONS ///

    struct Primitive {
        Token token;
        bool is_key; // is the name an accessed property?
    };

    struct ObjectField {
        Token name;
        ExprPtr value;
    };

    struct ObjectLiteral {
        std::vector<ObjectField> fields;
    };

    struct ArrayLiteral {
        std::vector<ExprPtr> items;
    };

    struct LambdaLiteral {
        std::vector<Token> params;
        StmtPtr body;
    };

    struct MemberAccess {
        ExprPtr target;
        ExprPtr key;
        bool pass_key_raw; // true if target[key] syntax is used
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

    struct Break {};

    struct Continue {};

    struct If {
        ExprPtr check;
        StmtPtr body_true;
        StmtPtr body_false;
    };

    struct Block {
        std::vector<StmtPtr> items;
    };

    struct Throw {
        ExprPtr error_expr;
    };

    struct TryCatch {
        Token error_name;
        StmtPtr body_try;
        StmtPtr body_catch;
    };

    /// BEGIN AST WRAPPER ///

    struct SourcedAst {
        std::string source_filename;
        StmtPtr decl;
        int src_id;
    };

    using ASTUnit = std::vector<SourcedAst>;
}
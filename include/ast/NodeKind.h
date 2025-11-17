#pragma once

namespace pycc::ast {
    enum class NodeKind {
        Module,
        FunctionDef,
        ReturnStmt,
        AssignStmt,
        ExprStmt,
        IfStmt,
        IntLiteral,
        BoolLiteral,
        FloatLiteral,
        StringLiteral,
        Name,
        Call,
        BinaryExpr,
        UnaryExpr,
        TupleLiteral,
        ListLiteral,
        ObjectLiteral,
        NoneLiteral,
        // Appended node kinds (do not reorder existing entries)
        Attribute,
        Subscript,
        BytesLiteral,
        EllipsisLiteral,
        DelStmt,
        WhileStmt,
        ForStmt,
        BreakStmt,
        ContinueStmt,
        PassStmt,
        TryStmt,
        ExceptHandler,
        WithItem,
        WithStmt,
        Import,
        ImportFrom,
        Alias,
        ClassDef,
        DefStmt,
        NamedExpr
        ,MatchStmt
        ,MatchCase
        ,PatternWildcard
        ,PatternName
        ,PatternLiteral
        ,PatternOr
        ,PatternAs
        ,PatternClass
        ,IfExpr
        ,LambdaExpr
        ,DictLiteral
        ,SetLiteral
        ,AugAssignStmt
        ,RaiseStmt
        ,GlobalStmt
        ,NonlocalStmt
        ,AssertStmt
        ,YieldExpr
        ,AwaitExpr
        ,ListComp
        ,SetComp
        ,DictComp
        ,GeneratorExpr
        ,Compare
        ,PatternSequence
        ,PatternMapping
        ,PatternStar
        ,ImagLiteral
        ,FStringLiteral
    };

    inline const char *to_string(const NodeKind element) {
        switch (element) {
            case NodeKind::Module: return "Module";
            case NodeKind::FunctionDef: return "FunctionDef";
            case NodeKind::ReturnStmt: return "ReturnStmt";
            case NodeKind::AssignStmt: return "AssignStmt";
            case NodeKind::IfStmt: return "IfStmt";
            case NodeKind::ExprStmt: return "ExprStmt";
            case NodeKind::IntLiteral: return "IntLiteral";
            case NodeKind::BoolLiteral: return "BoolLiteral";
            case NodeKind::FloatLiteral: return "FloatLiteral";
            case NodeKind::StringLiteral: return "StringLiteral";
            case NodeKind::Name: return "Name";
            case NodeKind::Call: return "Call";
            case NodeKind::BinaryExpr: return "BinaryExpr";
            case NodeKind::UnaryExpr: return "UnaryExpr";
            case NodeKind::TupleLiteral: return "TupleLiteral";
            case NodeKind::ListLiteral: return "ListLiteral";
            case NodeKind::ObjectLiteral: return "ObjectLiteral";
            case NodeKind::NoneLiteral: return "NoneLiteral";
            default: return "unknown";
        }
    }
} // namespace pycc::ast

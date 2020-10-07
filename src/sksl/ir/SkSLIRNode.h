/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SKSL_IRNODE
#define SKSL_IRNODE

#include "src/sksl/SkSLASTNode.h"
#include "src/sksl/SkSLLexer.h"
#include "src/sksl/SkSLModifiersPool.h"
#include "src/sksl/SkSLString.h"

#include <algorithm>
#include <vector>

namespace SkSL {

struct Expression;
class ExternalValue;
struct FunctionDeclaration;
struct Statement;
class Symbol;
class SymbolTable;
class Type;
class Variable;

/**
 * Represents a node in the intermediate representation (IR) tree. The IR is a fully-resolved
 * version of the program (all types determined, everything validated), ready for code generation.
 */
class IRNode {
public:
    virtual ~IRNode();

    IRNode& operator=(const IRNode& other) {
        // Need to have a copy assignment operator because Type requires it, but can't use the
        // default version until we finish migrating away from std::unique_ptr children. For now,
        // just assert that there are no children (we could theoretically clone them, but we never
        // actually copy nodes containing children).
        SkASSERT(other.fExpressionChildren.empty());
        fKind = other.fKind;
        fOffset = other.fOffset;
        fData = other.fData;
        return *this;
    }

    virtual String description() const = 0;

    // character offset of this element within the program being compiled, for error reporting
    // purposes
    int fOffset;

    const Type& type() const {
        switch (fData.fKind) {
            case NodeData::Kind::kBoolLiteral:
                return *this->boolLiteralData().fType;
            case NodeData::Kind::kExternalValue:
                return *this->externalValueData().fType;
            case NodeData::Kind::kField:
                return *this->fieldData().fType;
            case NodeData::Kind::kFloatLiteral:
                return *this->floatLiteralData().fType;
            case NodeData::Kind::kFunctionCall:
                return *this->functionCallData().fType;
            case NodeData::Kind::kIntLiteral:
                return *this->intLiteralData().fType;
            case NodeData::Kind::kSymbol:
                return *this->symbolData().fType;
            case NodeData::Kind::kType:
                return *this->typeData();
            case NodeData::Kind::kTypeToken:
                return *this->typeTokenData().fType;
            case NodeData::Kind::kVariable:
                return *this->variableData().fType;
            default:
                SkUNREACHABLE;
        }
    }

protected:
    struct BlockData {
        std::shared_ptr<SymbolTable> fSymbolTable;
        // if isScope is false, this is just a group of statements rather than an actual
        // language-level block. This allows us to pass around multiple statements as if they were a
        // single unit, with no semantic impact.
        bool fIsScope;
    };

    struct BoolLiteralData {
        const Type* fType;
        bool fValue;
    };

    struct EnumData {
        StringFragment fTypeName;
        std::shared_ptr<SymbolTable> fSymbols;
        bool fIsBuiltin;
    };

    struct ExternalValueData {
        const Type* fType;
        const ExternalValue* fValue;
    };

    struct FieldData {
        StringFragment fName;
        const Type* fType;
        const Variable* fOwner;
        int fFieldIndex;
    };

    struct FloatLiteralData {
        const Type* fType;
        float fValue;
    };

    struct ForStatementData {
        std::shared_ptr<SymbolTable> fSymbolTable;
    };

    struct FunctionCallData {
        const Type* fType;
        const FunctionDeclaration* fFunction;
    };

    struct IntLiteralData {
        const Type* fType;
        int64_t fValue;
    };

    struct SymbolData {
        StringFragment fName;
        const Type* fType;
    };

    struct SymbolAliasData {
        StringFragment fName;
        Symbol* fOrigSymbol;
    };

    struct TypeTokenData {
        const Type* fType;
        Token::Kind fToken;
    };

    struct VariableData {
        StringFragment fName;
        const Type* fType;
        const Expression* fInitialValue = nullptr;
        ModifiersPool::Handle fModifiersHandle;
        // Tracks how many sites read from the variable. If this is zero for a non-out variable (or
        // becomes zero during optimization), the variable is dead and may be eliminated.
        mutable int16_t fReadCount;
        // Tracks how many sites write to the variable. If this is zero, the variable is dead and
        // may be eliminated.
        mutable int16_t fWriteCount;
        /*Variable::Storage*/int8_t fStorage;
        bool fBuiltin;
    };

    struct NodeData {
        enum class Kind {
            kBlock,
            kBoolLiteral,
            kEnum,
            kExternalValue,
            kField,
            kFloatLiteral,
            kForStatement,
            kFunctionCall,
            kIntLiteral,
            kString,
            kSymbol,
            kSymbolAlias,
            kType,
            kTypeToken,
            kVariable,
        } fKind = Kind::kType;
        // it doesn't really matter what kind we default to, as long as it's a POD type

        union Contents {
            BlockData fBlock;
            BoolLiteralData fBoolLiteral;
            EnumData fEnum;
            ExternalValueData fExternalValue;
            FieldData fField;
            FloatLiteralData fFloatLiteral;
            ForStatementData fForStatement;
            FunctionCallData fFunctionCall;
            IntLiteralData fIntLiteral;
            String fString;
            SymbolData fSymbol;
            SymbolAliasData fSymbolAlias;
            const Type* fType;
            TypeTokenData fTypeToken;
            VariableData fVariable;

            Contents() {}

            ~Contents() {}
        } fContents;

        NodeData(const BlockData& data)
            : fKind(Kind::kBlock) {
            *(new(&fContents) BlockData) = data;
        }

        NodeData(const BoolLiteralData& data)
            : fKind(Kind::kBoolLiteral) {
            *(new(&fContents) BoolLiteralData) = data;
        }

        NodeData(const EnumData& data)
            : fKind(Kind::kEnum) {
            *(new(&fContents) EnumData) = data;
        }

        NodeData(const ExternalValueData& data)
            : fKind(Kind::kExternalValue) {
            *(new(&fContents) ExternalValueData) = data;
        }

        NodeData(const FieldData& data)
            : fKind(Kind::kField) {
            *(new(&fContents) FieldData) = data;
        }

        NodeData(const FloatLiteralData& data)
            : fKind(Kind::kFloatLiteral) {
            *(new(&fContents) FloatLiteralData) = data;
        }

        NodeData(const ForStatementData& data)
            : fKind(Kind::kForStatement) {
            *(new(&fContents) ForStatementData) = data;
        }

        NodeData(const FunctionCallData& data)
            : fKind(Kind::kFunctionCall) {
            *(new(&fContents) FunctionCallData) = data;
        }

        NodeData(IntLiteralData data)
            : fKind(Kind::kIntLiteral) {
            *(new(&fContents) IntLiteralData) = data;
        }

        NodeData(const String& data)
            : fKind(Kind::kString) {
            *(new(&fContents) String) = data;
        }

        NodeData(const SymbolData& data)
            : fKind(Kind::kSymbol) {
            *(new(&fContents) SymbolData) = data;
        }

        NodeData(const SymbolAliasData& data)
            : fKind(Kind::kSymbolAlias) {
            *(new(&fContents) SymbolAliasData) = data;
        }

        NodeData(const Type* data)
            : fKind(Kind::kType) {
            *(new(&fContents) const Type*) = data;
        }

        NodeData(const TypeTokenData& data)
            : fKind(Kind::kTypeToken) {
            *(new(&fContents) TypeTokenData) = data;
        }

        NodeData(const VariableData& data)
            : fKind(Kind::kVariable) {
            *(new(&fContents) VariableData) = data;
        }

        NodeData(const NodeData& other) {
            *this = other;
        }

        NodeData& operator=(const NodeData& other) {
            this->cleanup();
            fKind = other.fKind;
            switch (fKind) {
                case Kind::kBlock:
                    *(new(&fContents) BlockData) = other.fContents.fBlock;
                    break;
                case Kind::kBoolLiteral:
                    *(new(&fContents) BoolLiteralData) = other.fContents.fBoolLiteral;
                    break;
                case Kind::kEnum:
                    *(new(&fContents) EnumData) = other.fContents.fEnum;
                    break;
                case Kind::kExternalValue:
                    *(new(&fContents) ExternalValueData) = other.fContents.fExternalValue;
                    break;
                case Kind::kField:
                    *(new(&fContents) FieldData) = other.fContents.fField;
                    break;
                case Kind::kFloatLiteral:
                    *(new(&fContents) FloatLiteralData) = other.fContents.fFloatLiteral;
                    break;
                case Kind::kForStatement:
                    *(new(&fContents) ForStatementData) = other.fContents.fForStatement;
                    break;
                case Kind::kFunctionCall:
                    *(new(&fContents) FunctionCallData) = other.fContents.fFunctionCall;
                    break;
                case Kind::kIntLiteral:
                    *(new(&fContents) IntLiteralData) = other.fContents.fIntLiteral;
                    break;
                case Kind::kString:
                    *(new(&fContents) String) = other.fContents.fString;
                    break;
                case Kind::kSymbol:
                    *(new(&fContents) SymbolData) = other.fContents.fSymbol;
                    break;
                case Kind::kSymbolAlias:
                    *(new(&fContents) SymbolAliasData) = other.fContents.fSymbolAlias;
                    break;
                case Kind::kType:
                    *(new(&fContents) const Type*) = other.fContents.fType;
                    break;
                case Kind::kTypeToken:
                    *(new(&fContents) TypeTokenData) = other.fContents.fTypeToken;
                    break;
                case Kind::kVariable:
                    *(new(&fContents) VariableData) = other.fContents.fVariable;
                    break;
            }
            return *this;
        }

        ~NodeData() {
            this->cleanup();
        }

    private:
        void cleanup() {
            switch (fKind) {
                case Kind::kBlock:
                    fContents.fBlock.~BlockData();
                    break;
                case Kind::kBoolLiteral:
                    fContents.fBoolLiteral.~BoolLiteralData();
                    break;
                case Kind::kEnum:
                    fContents.fEnum.~EnumData();
                    break;
                case Kind::kExternalValue:
                    fContents.fExternalValue.~ExternalValueData();
                    break;
                case Kind::kField:
                    fContents.fField.~FieldData();
                    break;
                case Kind::kFloatLiteral:
                    fContents.fFloatLiteral.~FloatLiteralData();
                    break;
                case Kind::kForStatement:
                    fContents.fForStatement.~ForStatementData();
                    break;
                case Kind::kFunctionCall:
                    fContents.fFunctionCall.~FunctionCallData();
                    break;
                case Kind::kIntLiteral:
                    fContents.fIntLiteral.~IntLiteralData();
                    break;
                case Kind::kString:
                    fContents.fString.~String();
                    break;
                case Kind::kSymbol:
                    fContents.fSymbol.~SymbolData();
                    break;
                case Kind::kSymbolAlias:
                    fContents.fSymbolAlias.~SymbolAliasData();
                    break;
                case Kind::kType:
                    break;
                case Kind::kTypeToken:
                    fContents.fTypeToken.~TypeTokenData();
                    break;
                case Kind::kVariable:
                    fContents.fVariable.~VariableData();
                    break;
            }
        }
    };

    IRNode(int offset, int kind, const BlockData& data,
           std::vector<std::unique_ptr<Statement>> stmts);

    IRNode(int offset, int kind, const BoolLiteralData& data);

    IRNode(int offset, int kind, const EnumData& data);

    IRNode(int offset, int kind, const ExternalValueData& data);

    IRNode(int offset, int kind, const FieldData& data);

    IRNode(int offset, int kind, const FloatLiteralData& data);

    IRNode(int offset, int kind, const ForStatementData& data);

    IRNode(int offset, int kind, const FunctionCallData& data);

    IRNode(int offset, int kind, const IntLiteralData& data);

    IRNode(int offset, int kind, const String& data);

    IRNode(int offset, int kind, const SymbolData& data);

    IRNode(int offset, int kind, const SymbolAliasData& data);

    IRNode(int offset, int kind, const Type* data = nullptr);

    IRNode(int offset, int kind, const TypeTokenData& data);

    IRNode(int offset, int kind, const VariableData& data);

    Expression& expressionChild(int index) const {
        SkASSERT(index >= 0 && index < (int) fExpressionChildren.size());
        return *fExpressionChildren[index];
    }

    std::unique_ptr<Expression>& expressionPointer(int index) {
        SkASSERT(index >= 0 && index < (int) fExpressionChildren.size());
        return fExpressionChildren[index];
    }

    const std::unique_ptr<Expression>& expressionPointer(int index) const {
        SkASSERT(index >= 0 && index < (int) fExpressionChildren.size());
        return fExpressionChildren[index];
    }

    int expressionChildCount() const {
        return fExpressionChildren.size();
    }


    Statement& statementChild(int index) const {
        SkASSERT(index >= 0 && index < (int) fStatementChildren.size());
        return *fStatementChildren[index];
    }

    std::unique_ptr<Statement>& statementPointer(int index) {
        SkASSERT(index >= 0 && index < (int) fStatementChildren.size());
        return fStatementChildren[index];
    }

    const std::unique_ptr<Statement>& statementPointer(int index) const {
        SkASSERT(index >= 0 && index < (int) fStatementChildren.size());
        return fStatementChildren[index];
    }

    int statementChildCount() const {
        return fStatementChildren.size();
    }

    BlockData& blockData() {
        SkASSERT(fData.fKind == NodeData::Kind::kBlock);
        return fData.fContents.fBlock;
    }

    const BlockData& blockData() const {
        SkASSERT(fData.fKind == NodeData::Kind::kBlock);
        return fData.fContents.fBlock;
    }

    const BoolLiteralData& boolLiteralData() const {
        SkASSERT(fData.fKind == NodeData::Kind::kBoolLiteral);
        return fData.fContents.fBoolLiteral;
    }

    const EnumData& enumData() const {
        SkASSERT(fData.fKind == NodeData::Kind::kEnum);
        return fData.fContents.fEnum;
    }

    const ExternalValueData& externalValueData() const {
        SkASSERT(fData.fKind == NodeData::Kind::kExternalValue);
        return fData.fContents.fExternalValue;
    }

    const FieldData& fieldData() const {
        SkASSERT(fData.fKind == NodeData::Kind::kField);
        return fData.fContents.fField;
    }

    const FloatLiteralData& floatLiteralData() const {
        SkASSERT(fData.fKind == NodeData::Kind::kFloatLiteral);
        return fData.fContents.fFloatLiteral;
    }

    const ForStatementData& forStatementData() const {
        SkASSERT(fData.fKind == NodeData::Kind::kForStatement);
        return fData.fContents.fForStatement;
    }

    const FunctionCallData& functionCallData() const {
        SkASSERT(fData.fKind == NodeData::Kind::kFunctionCall);
        return fData.fContents.fFunctionCall;
    }

    const IntLiteralData& intLiteralData() const {
        SkASSERT(fData.fKind == NodeData::Kind::kIntLiteral);
        return fData.fContents.fIntLiteral;
    }

    const String& stringData() const {
        SkASSERT(fData.fKind == NodeData::Kind::kString);
        return fData.fContents.fString;
    }

    SymbolData& symbolData() {
        SkASSERT(fData.fKind == NodeData::Kind::kSymbol);
        return fData.fContents.fSymbol;
    }

    const SymbolData& symbolData() const {
        SkASSERT(fData.fKind == NodeData::Kind::kSymbol);
        return fData.fContents.fSymbol;
    }

    const SymbolAliasData& symbolAliasData() const {
        SkASSERT(fData.fKind == NodeData::Kind::kSymbolAlias);
        return fData.fContents.fSymbolAlias;
    }

    const Type* typeData() const {
        SkASSERT(fData.fKind == NodeData::Kind::kType);
        return fData.fContents.fType;
    }

    const TypeTokenData& typeTokenData() const {
        SkASSERT(fData.fKind == NodeData::Kind::kTypeToken);
        return fData.fContents.fTypeToken;
    }

    VariableData& variableData() {
        SkASSERT(fData.fKind == NodeData::Kind::kVariable);
        return fData.fContents.fVariable;
    }

    const VariableData& variableData() const {
        SkASSERT(fData.fKind == NodeData::Kind::kVariable);
        return fData.fContents.fVariable;
    }

    int fKind;

    NodeData fData;

    // Needing two separate vectors is a temporary issue. Ideally, we'd just be able to use a single
    // vector of nodes, but there are various spots where we take pointers to std::unique_ptr<>,
    // and it isn't safe to pun std::unique_ptr<IRNode> to std::unique_ptr<Statement / Expression>.
    // And we can't update the call sites to expect std::unique_ptr<IRNode> while there are still
    // old-style nodes around.
    // When the transition is finished, we'll be able to drop the unique_ptrs and just handle
    // <IRNode> directly.
    std::vector<std::unique_ptr<Expression>> fExpressionChildren;
    // it's important to keep fStatements defined after (and thus destroyed before) fData,
    // because destroying statements can modify reference counts in a SymbolTable contained in fData
    std::vector<std::unique_ptr<Statement>> fStatementChildren;
};

}  // namespace SkSL

#endif

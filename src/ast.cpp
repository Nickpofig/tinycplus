#include "ast.h"

namespace tinycpp {

    bool ASTBinaryOp::hasAddress() const {
        return false;
    }

    bool ASTUnaryOp::hasAddress() const {
        return false;
    }

} // namespace tinycpp
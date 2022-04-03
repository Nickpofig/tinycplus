#include "ast.h"

namespace tinycplus {

    bool ASTBinaryOp::hasAddress() const {
        return false;
    }

    bool ASTUnaryOp::hasAddress() const {
        return false;
    }

} // namespace tinycplus
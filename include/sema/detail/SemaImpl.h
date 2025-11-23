/***
 * Name: sema_check_impl, inferExprType (declarations)
 * Purpose: Provide internal Sema implementation entry points and helpers
 *          split from monolithic sources for modular compilation and reuse.
 * Notes:
 *   - These are internal APIs used by Sema::check and friends. They live in
 *     a detail header to avoid polluting the public Sema interface.
 */
#pragma once

// Aggregator for Sema impl declarations
#include "sema/detail/impl/SemaCheckImpl.h"
#include "sema/detail/impl/InferExprType.h"

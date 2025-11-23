/**
 * @file
 * @brief Minimal runtime introspection helpers for internal handlers.
 */
#pragma once

#include "runtime/All.h"

namespace pycc::rt {

// Expose object type tag for internal helpers.
TypeTag type_of_public(void* obj);

}


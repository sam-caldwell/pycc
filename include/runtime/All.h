/***
 * Name: pycc::rt umbrella header
 * Purpose: Aggregate runtime API headers. Transitional umbrella during header
 *          modularization; includes group headers and the legacy Runtime.h for
 *          declarations not yet split.
 */
#pragma once

#include "runtime/TypeTag.h"
#include "runtime/GCStats.h"
#include "runtime/GC.h"
#include "runtime/Runtime.h" // transitional; will be removed once fully split


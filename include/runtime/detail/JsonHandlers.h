/**
 * @file
 * @brief JSON dumping handlers for List and Dict objects.
 */
#pragma once

#include <string>
#include <vector>
#include "runtime/All.h"
#include "runtime/detail/JsonTypes.h"

namespace pycc::rt::detail {

/**
 * Dump a List object to JSON, handling indentation and separators.
 */
void json_dump_list(void* obj, std::string& out, const DumpOpts& opts, int depth, DumpRecFn rec);

/**
 * Dump a Dict object to JSON, handling optional key sorting and separators.
 * Only string keys are supported per runtime constraints.
 */
void json_dump_dict(void* obj, std::string& out, const DumpOpts& opts, int depth, DumpRecFn rec);

} // namespace pycc::rt::detail


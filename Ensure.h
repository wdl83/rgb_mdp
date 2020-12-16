#pragma once

#include "mdp/Ensure.h"

using TagFormatError = EXCEPTION(std::runtime_error);
using TagMissingError = EXCEPTION(std::runtime_error);
using TagValueRangeError = EXCEPTION(std::runtime_error);

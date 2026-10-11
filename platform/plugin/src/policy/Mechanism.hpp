#pragma once

// The mechanism API the policy calls (contracts/mechanism-api.md 1.0). Form P: the patch series declares it in the
// headers installed with the patched prefix, and the patched binary exports it. Form S: the plugin's mechanism part
// declares and defines the same names and signatures (src/mechanism/Api.hpp). The policy itself compiles unchanged.

#ifdef HYPRVX_FORM_P
#include <src/hyprvx/MonitorCamera.hpp>
#include <src/hyprvx/WindowOffset.hpp>
#else
#include "mechanism/Api.hpp"
#endif

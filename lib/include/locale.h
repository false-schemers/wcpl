/* Localization */

#pragma once

/* stub. can be used in dual-use code to align non-WASM
 * locale with WASM locale via setlocale(LC_ALL, ".utf8") */

#define LC_ALL 0
#define setlocale(lc, loc) ((void)0)

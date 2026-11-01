// Hooks for Sonic Heroes' default.xbe.
//
// A strong definition replaces the recompiled function at that address, for
// example:
//
// #include "xdk/hle.h"
//
// X86_EXTERN_FUNC(__imp__sub_00012345);
// X86_FUNC(sub_00012345) {
//     __imp__sub_00012345(ctx, base);  // the original
// }
//
// The addresses depend on the exact executable. Use the log's "unknown
// function" reports and the OGlue notes in patches/README.md to identify the
// ones that need a host replacement. Keep every workaround behind an
// environment variable so the default build stays faithful to the original.

#pragma once

/*
 * PSP builds do not compile the PS2 disc-read branches guarded by
 * `HE_CPP_PLATFORM_PS2`, but shared generated core currently includes
 * `<libcdvd.h>` unconditionally. This compatibility shim keeps the PSP
 * translation units buildable until generated-core normalization makes the
 * include itself conditional.
 */

# PSP texture formats

The PSP cooker and both native renderers support all eleven GU pixel encodings. The platform publishes 43 native settings and retains four existing generic settings, for 47 settings in total. Native textures use `TextureAssetColorFormat.PspNative` (serialized value 15), with a checked `PGT1` descriptor and separate CLUT data.

## Settings

| Encoding | GU code | Bits per texel | Storage orders | Alpha settings |
| --- | ---: | ---: | --- | --- |
| `Rgb565` | 0 | 16 | Linear, Swizzled | Opaque |
| `Rgba5551` | 1 | 16 | Linear, Swizzled | Opaque, Binary |
| `Rgba4444` | 2 | 16 | Linear, Swizzled | Opaque, A4 |
| `Rgba8888` | 3 | 32 | Linear, Swizzled | Opaque, A8 |
| `T4` | 4 | 4 | Linear, Swizzled | Determined by CLUT |
| `T8` | 5 | 8 | Linear, Swizzled | Determined by CLUT |
| `T16` | 6 | 16 | Linear, Swizzled | Determined by CLUT |
| `T32` | 7 | 32 | Linear, Swizzled | Determined by CLUT |
| `Dxt1` | 8 | 4 | Linear | Opaque, Binary |
| `Dxt3` | 9 | 8 | Linear | Opaque, A4 |
| `Dxt5` | 10 | 8 | Linear | Opaque, A8 |

Direct and compressed IDs follow `Psp.<order>.<encoding>`, for example `Psp.Swizzled.Rgba4444` and `Psp.Linear.Dxt5`.

Indexed IDs append `.Clut<encoding>`, for example `Psp.Swizzled.T8.ClutRgba8888`. Each indexed encoding supports all four direct CLUT encodings: `Rgb565`, `Rgba5551`, `Rgba4444`, and `Rgba8888`. The alpha settings match the selected CLUT encoding. T4 has 16 entries; T8, T16, and T32 have 256 entries. The canonical T16/T32 cooker uses a low-eight-bit CLUT mask with zero shift and start. Nonzero upper index bits are rejected in PGT1 payloads.

This gives eight direct settings, 32 indexed settings, and three DXT settings. Swizzled DXT settings are not published. Unsupported IDs, alpha combinations, palette indices, and resolution limits fail explicitly.

The existing `Rgba4444`, `Indexed8`, `Rgba32`, and `Indexed4` IDs retain their generic payloads and legacy runtime conversion. Existing defaults remain `Rgba4444`/A4 with maximum resolution 512 for images and `Indexed8`/A8 with no resize limit for fonts. Select a `Psp.*` ID to retain encoded native storage on the GPU. A cooked texture must fit within 512 × 512 even when resizing is disabled.

## Import and cooking

`.gim` is registered in the shared import-extension catalog, Windows editor importer factory, and portable PSP source processor. The importer accepts little-endian `MIG.00.1PSP` containers, all eleven pixel encodings, all four CLUT encodings, and linear or PSP faster/swizzled order for uncompressed data. The default overload selects picture zero; an explicit picture-index overload is also available.

Import produces an RGBA32 preview from the first frame and base mip level. Animation, remaining mip levels, and original container bytes are not retained. Big-endian GIM variants, extended format codes, swizzled DXT, and multiple image blocks within one picture are rejected. Blocks, index tables, frame offsets, native dimensions, and base-level pixel bounds are validated before decoding. The importer caps buffered input at 64 MiB and supports nonseekable streams.

The PSP cooker resizes imported pixels before native encoding. Indexed formats use the existing quantization method selected in texture settings; T4 quantizes to 16 colors and the other indexed formats to 256. Native direct channels use little-endian PSP words with red in the least significant bits. DXT shares the existing BC compressor, then rearranges each block into PSP byte order; it does not treat PSP DXT blocks as standard desktop BC blocks.

## Native payload

PGT1 is single-level 2D storage. Every header field is a little-endian 32-bit word:

| Byte offset | Field |
| ---: | --- |
| 0 | Magic `0x31544750` (`PGT1`) |
| 4 | Version: 1 |
| 8 | GU encoding: 0–10 |
| 12 | Swizzle flag: 0 or 1 |
| 16 | Authored width |
| 20 | Authored height |
| 24 | Power-of-two storage width / GU buffer width |
| 28 | Power-of-two storage height |
| 32 | Native texel byte count |
| 36 | CLUT encoding: 0–3; zero for nonindexed data |
| 40 | CLUT entry count: 0, 16, or 256 |
| 44 | CLUT byte count |

Native texels start at byte 48 of `TextureAsset.Colors`. Native CLUT bytes live in `TextureAsset.PaletteColors`. Runtime and managed readers require exact lengths and canonical storage dimensions.

Storage extents round up to powers of two. Uncompressed rows occupy at least 16 bytes; swizzled textures have at least eight rows. DXT extents have a minimum of 4 × 4. Padding repeats edge texels. Uncompressed swizzling maps logical bytes into 16-byte × 8-row blocks. Authored dimensions remain separate for UV calculation.

PSP DXT1 stores color selectors before endpoints. PSP DXT3 stores its color block before explicit alpha. PSP DXT5 stores its color block, then the alpha-index groups, then alpha endpoints. Managed previews and the independent native CPU decoder implement the same PSP ordering.

## Rendering and lifetime

`PspRuntimeTexture` owns 16-byte-aligned texel and CLUT allocations. Both 2D and 3D texture binds select the native GU format and swizzle flag, load the CLUT when required, submit the native texels, and flush the texture cache. CLUT loads use 32-byte blocks. GPU byte accounting includes encoded texels plus CLUT bytes; it excludes the PGT1 header, allocation alignment slack, and CPU previews.

Native textures retain compressed/indexed storage for GPU drawing. The existing CPU font cache obtains an ABGR8888 surface lazily and indexes it using the padded row stride. Static fonts can therefore use native atlas formats while preserving the existing CPU composition path. Dynamic font atlases continue through the existing generic path.

Texel and palette allocations enter the existing deferred-release queue and remain alive until the renderer synchronizes pending GPU reads. Editor/file readers and runtime readers accept the native texture tag for both ordinary assets and embedded font atlases.

## Validation

Validation artifacts are in `builds/helengine-psp/texture-formats-20261008` under the workspace root:

- 115 PSP builder tests cover all 43 native settings, all 43 GIM variants, HELE file and runtime readers, alpha policies, malformed headers/offsets, resizing, real texture/font-atlas cooking and build staging, metadata, and existing builder behavior.
- One Windows importer-registration test verifies `.gim` registration and extension aggregation.
- One embedded-font test verifies both font readers retain swizzled PSP texels/CLUT bytes and produce correct CPU coverage.
- The native parser/decoder matches actual managed payloads for all 43 settings and rejects 13 malformed headers/payloads per fixture.
- Full core C++ generation succeeds with the PSP settings and platform-owned cooked-material resolution enabled.
- Five affected native units compile against that newly generated core with the PSP SDK using C++20, optimization, and warnings treated as errors.
- The four new managed PSP codec types compile with the PSP SDK from the full generated core. An isolated real-source generation probe also verified these types independently.

A complete linked player build and hardware/emulator rendering have not been validated. The SDK checks compile the affected integration units and generated PSP codecs; they do not produce a complete PBP.

## References

- [PSPSDK GU API and encoding constants](https://github.com/pspdev/pspsdk/blob/master/src/gu/pspgu.h).
- [PPSSPP PSP DXT block structures](https://github.com/hrydgard/ppsspp/blob/master/GPU/Common/TextureDecoder.h).
- [PPSSPP texture buffer-width alignment and swizzling](https://github.com/hrydgard/ppsspp/blob/master/GPU/Common/TextureDecoder.cpp).
- [PPSSPP CLUT load size and index controls](https://github.com/hrydgard/ppsspp/blob/master/GPU/GPUState.h).
- [GIMSplit container/block metadata implementation](https://github.com/adeyblue/GIMSplit/blob/master/Program.cs).

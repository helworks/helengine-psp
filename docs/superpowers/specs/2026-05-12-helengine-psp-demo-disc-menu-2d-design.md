# PSP Demo Disc Menu 2D Design

## Goal

Make `DemoDiscMainMenu` render and behave interactively on PSP by adding a reusable PSP 2D rendering slice that supports the menu primitives the engine already emits.

This is not a demo-disc-specific renderer. The PSP backend should implement the existing 2D drawable contracts well enough for the current menu, while staying small enough that we do not overbuild unrelated UI features yet.

## Problem Statement

The current PSP boot path can load the demo disc main menu scene, but the menu does not render. The immediate root cause is that the PSP 2D renderer currently drops all supported menu primitives:

- `DrawRoundedRect(...)` is a no-op.
- `DrawSprite(...)` is a no-op.
- `DrawText(...)` is a no-op.

That means the menu runtime can be instantiated correctly while the PSP backend still produces a blank result.

## Scope

### In Scope

- Reusable PSP 2D rendering support for the current menu feature set
- Solid-color 2D surface rendering
- Rounded-rectangle fallback rendering for menu panels and blockers
- Sprite quad rendering through the PSP texture path
- Bitmap font text rendering from cooked font assets
- Correct 2D draw ordering for menu background, surfaces, labels, descriptions, and highlight states
- Full menu interaction through existing runtime menu logic and PSP input delivery
- End-to-end scene action validation from the menu into a playable scene and back

### Out Of Scope

- Full desktop/editor UI parity
- General-purpose clipping or masking beyond what the menu strictly requires
- Advanced text shaping or rich typography features
- New menu-specific renderer code paths outside the existing drawable interfaces
- Large-scale 2D framework refactoring unrelated to PSP bring-up

## Recommended Approach

Implement a small general PSP 2D capability layer rather than a menu-specific shortcut.

Why:

- The current failure is a backend capability gap, not a content bug.
- The menu gives a concrete acceptance target while still driving reusable engine-facing primitives.
- A narrow PSP 2D layer keeps future menus and simple HUD work viable without pretending we need the full UI stack today.

Alternatives considered:

1. Demo-disc-specific menu drawing
   - Fastest short-term
   - Wrong abstraction and likely throwaway work

2. Small general PSP 2D layer
   - Recommended
   - Reusable and still scoped

3. Full engine UI parity now
   - Too broad for the milestone
   - High schedule risk

## Architecture

### Renderer Contract

The PSP backend should continue implementing the engine's current 2D drawable interfaces. Menu runtime code should not fork into a PSP-specific path.

The PSP renderer must gain working implementations for:

- `DrawRoundedRect(...)`
- `DrawSprite(...)`
- `DrawText(...)`

This keeps authored UI content and runtime menu logic unchanged while lifting PSP to the minimum viable 2D feature level.

### Primitive Strategy

#### Solid And Rounded Surfaces

Panel surfaces, blockers, and highlight regions should render through PSP-friendly 2D geometry. The first milestone does not need perfect high-segment rounded corners; it needs a stable rounded-rect fallback that preserves the authored menu look closely enough to be readable and visually correct.

Implementation can use tessellated simple geometry or another GU-friendly fallback, but it must remain renderer-general rather than hardcoded to one menu layout.

#### Sprites

Sprite drawing should submit textured PSP quads through the existing texture cache/runtime texture path. The draw path must respect source texture identity, tint/color modulation, and render ordering.

#### Text

Text rendering should use cooked bitmap font assets and emit glyph quads against the corresponding atlas texture. The milestone only needs the current menu’s font usage to render correctly; it does not need generalized advanced typography behavior.

## Interaction Model

The demo disc main menu must become fully interactive on PSP.

Expected ownership:

- PSP renderer owns visual primitive rendering.
- Existing engine menu runtime owns menu layout, selection state, actions, and panel transitions.
- PSP input backend owns button state delivery only.

The backend work should not move menu logic into PSP platform code. If interaction fails after visuals land, the fix should target the real runtime or input contract gap.

## Feature Slice

The milestone should be executed in this order:

1. Make menu primitives visible
   - Rounded/surface rects
   - Sprites
   - Text
2. Validate 2D layering/order
   - Background
   - Surface
   - Text
   - Highlight/selection
3. Validate PSP input-driven menu navigation
4. Validate menu actions
   - Open panel
   - Back
   - Load scene
   - Return from scene to menu

This order keeps visual bring-up separate from input debugging and makes failures easier to isolate.

## Verification

### Primary Verification Target

- `DemoDiscMainMenu`

### Success Criteria

- Menu title, surfaces, rows, and descriptive text render on screen
- Selection/highlight state is visibly distinct
- PSP controls can move selection
- Confirm activates the selected row
- Subpanels open and close correctly
- Scene launch from the menu works
- Returning from a scene back to the menu works
- No regression to the current PSP startup path

### Runtime Verification Flow

1. Rebuild/export the PSP payload for `DemoDiscMainMenu`
2. Install cooked payload correctly into PPSSPP memstick
3. Copy rebuilt `EBOOT.PBP` last
4. Launch PPSSPP
5. Stop and ask the user what they see
6. Only then inspect logs and continue

User visual confirmation remains the primary result after each PPSSPP launch.

## Failure Boundaries

The work should stop at the first real boundary and fix only that boundary.

Likely boundaries:

1. Menu scene boots but no 2D output appears
2. Surfaces render but text is missing
3. Text renders but layering/order is wrong
4. Visuals render but highlight state is not readable
5. Visuals render but PSP input does not drive menu state
6. Menu interaction works partially, but panel transitions or scene actions fail

We should not add menu-specific patches to hide missing backend features.

## Implementation Areas

Most of the work is expected in:

- [PspRenderManager2D.cpp](/C:/dev/helworks/helengine-psp/src/platform/psp/rendering/PspRenderManager2D.cpp)
- [PspRenderManager2D.hpp](/C:/dev/helworks/helengine-psp/src/platform/psp/rendering/PspRenderManager2D.hpp)
- PSP texture/font support paths if text rendering needs additional runtime helpers
- PSP input path only if interaction still fails after rendering is in place

## Non-Goals

This milestone does not attempt to solve all future PSP UI work. It defines a reusable, engine-aligned 2D slice that is sufficient for the current menu and intentionally extensible for the next UI features.

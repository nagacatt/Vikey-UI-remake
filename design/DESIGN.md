# Vikey Design Direction: Reze Edition (Atmospheric Glass & Full-Bleed Art)

## Identity & Mood
- **Theme:** Reze (Chainsaw Man: Rainy Midnight Café, Bioluminescent Jellyfish & White Narcissus).
- **Aesthetic Direction (Owner R-37 Specification):** Full-bleed Reze artwork backgrounds visible across the window canvas, paired with optical frosted glass (`Gaussian blur + translucent tint + 1px specular rim`) and a refined, non-uniform rounded corner hierarchy.
- **R-37 Owner Override Note:** The product owner explicitly requested rounded corners, frosted glass cards, and full-window image backgrounds instead of angular `0px` brutalist boxes. Per R-37, this direction supersedes the R-10 glass dose cap while maintaining strict R-25 WCAG AA contrast (`> 8:1` to `15:1`) via optical background blur and high-opacity glass scrims.
- **Dials:** `Dial: ENERGY 2 / RHYTHM 3 / MOTION 1`
  - **ENERGY 2 (Balanced):** Evocative, atmospheric Reze photography in the hero and background canvas balanced by calm, legible frosted-glass control surfaces.
  - **RHYTHM 3 (Varied):** Open cinematic hero stage with floating glass pills at the top, transitioning into translucent frosted-glass setting cards (`16px` radius) and compact footer action pills (`9px` radius).
  - **MOTION 1 (Hover & state feedback):** Smooth state transitions on hover, focus, and toggle activation without distracting infinite loops.

## Visual Assets (From `Tài nguyên`)
All four artworks from `c:\ViKey-UI-Remake\Tài nguyên` are embedded as `RCDATA` in `Vikey.rc` and can be cycled live by the user via the `Đổi ảnh nền` button in the header (`RezeArtIndex` in Registry):
1. `reze_night.jpg` (`Reze - Chainsawman.jpg`): Midnight rain outside the Soviet apartment block (Default Dark Mode wallpaper).
2. `reze_abyss.jpg` (`Reze _ Chainsaw Man.jpg`): Bioluminescent jellyfish underwater portrait (Onboarding showcase & selectable wallpaper).
3. `reze_flower.jpg` (`Reze chainsaw man.jpg`): Close-up holding white narcissus flowers (Default Light Mode wallpaper & About portrait).
4. `reze_graphic.jpg` (`Reze _ Chainsaw Man (1).jpg`): Crisp azure graphic portrait (Selectable wallpaper).

## Radius Hierarchy (R-11)
Instead of uniform pill shapes, radii are differentiated by component role:
- **Window Outer Frame:** Native Windows 11 rounded corners (`DWMWCP_ROUND`).
- **Frosted Glass Cards (`Group`):** `16px` radius (`r = 16`) with `1px` specular rim lighting.
- **Interactive Controls (`VikeyButton`, `VikeySelect`, `VikeyHotkey`):** `9px` radius (`r = 9`).
- **Segmented Language Bar (`VikeySegmented`):** `11px` outer glass track with `8px` active pill.
- **Toggles (`VikeyToggle`):** `42 x 24` px smooth capsule (`r = 12`) with size-differentiated circular thumb (`18px` luminous thumb with inner choker-ring detail when ON vs `14px` muted thumb when OFF).

## Dual Theme Glass Palette & WCAG AA Verification (R-21, R-25, R-29, R-34)
Verified with `contrast-check.py` against worst-case blurred background pixels:
1. **Dark Glass ("Đêm mưa / Midnight Nocturne", `RezeTheme = 0`):**
   - Frosted Glass Surface: 28px Gaussian-blurred Reze art + `80%` `#0D131F` obsidian tint (worst-case composite `#131E32`)
   - Primary Text: `#F4F7FB` (`15.52:1` contrast)
   - Secondary Text: `#B0C0D8` (`9.03:1` contrast)
   - Control Border: `#5E7699` (`3.59:1` non-text contrast)
   - Primary Accent: `#58C4F6` (Bioluminescent Sky Cyan, with `#0B101B` ink at `9.64:1`)
   - Focus & Status Accent: `#34D399` (Reze Emerald Eye)
2. **Light Glass ("Hoa thủy tiên / Daylight Azure", `RezeTheme = 1`):**
   - Frosted Glass Surface: 28px Gaussian-blurred Reze art + `86%` `#F7FAFF` porcelain tint (worst-case composite `#DCE8F7`)
   - Primary Text: `#0F172A` (`14.39:1` contrast)
   - Secondary Text: `#334155` (`8.35:1` contrast)
   - Control Border: `#5B708B` (`4.09:1` non-text contrast)
   - Primary Accent: `#2563EB` (Royal Azure, with `#FFFFFF` ink at `5.17:1`)
   - Focus & Status Accent: `#059669` (Deep Emerald)

## One-Line Decision Log (R-31)
- **Why full-bleed blurred Reze backgrounds?** Requested by the owner so the artwork defines the atmosphere of the entire window rather than sitting in a small corner box.
- **Why real C++ Gaussian blur for glass cards?** Blurring the underlying artwork slice before applying the translucent glass tint prevents background linework from clashing with Vietnamese diacritics and guarantees > 8:1 text contrast.
- **Why parent-canvas BitBlt in child controls?** Eliminates Win32 rectangular background seams so rounded toggles, buttons, and selectors float with true alpha transparency over the frosted glass cards.
- **Why tiered border radii (16px / 11px / 9px)?** Gives cards, segmented bars, and buttons distinct visual roles without turning every element into an identical pill.
- **Why include a live Wallpaper Switcher button?** Lets the user enjoy all four artworks from `Tài nguyên` across both Dark and Light glass modes.

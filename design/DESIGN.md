# Vikey Design Direction: Reze Edition (Chainsaw Man)

## Identity & Mood
- **Theme:** Reze (Bomb Devil Arc / Crossroads Café / Soviet Nocturne).
- **Core Tension:** Quiet rainy-night intimacy and floral fragility (`reze_flower.jpg`, `reze_night.jpg`) contrasted with sharp industrial military hardware (her choker bomb-pin ring) and bold graphic manga framing (`reze_graphic.jpg`, `reze_abyss.jpg`).
- **Dials:** `Dial: ENERGY 2 / RHYTHM 2 / MOTION 1`
  - **ENERGY 2 (Balanced):** Bold manga-panel artwork headers paired with disciplined, high-contrast utility controls that never get in the way of typing settings.
  - **RHYTHM 2 (Consistent with intentional breaks):** Asymmetric split hero banner on the main window and vertical manga-panel sidebar in Onboarding, transitioning into structured two-column and full-width technical rows.
  - **MOTION 1 (Hover and state transitions only):** Instant mechanical state snaps on toggles, buttons, and tabs; zero endless loops or decorative floating animations.

## Visual Assets (From `Tài nguyên`)
All four user-supplied artworks from `c:\ViKey-UI-Remake\Tài nguyên` are embedded as Win32 `RCDATA` resources in `Vikey.rc` (with live disk fallback):
1. `reze_night.jpg` (`Reze - Chainsawman.jpg`): Soviet apartment block at midnight. Used in the Main Window hero stage (Dark Mode) to anchor the nocturnal brutalist atmosphere.
2. `reze_graphic.jpg` (`Reze _ Chainsaw Man (1).jpg`): High-contrast electric azure manga portrait. Used in the Main Window hero stage (Light Mode) and Emoji Picker header strip.
3. `reze_abyss.jpg` (`Reze _ Chainsaw Man.jpg`): Deep abyssal jellyfish portrait with vivid emerald eyes and choker ring. Used as the vertical art panel in the 4-step Onboarding guide.
4. `reze_flower.jpg` (`Reze chainsaw man.jpg`): Close-up portrait holding white narcissus flowers. Used in the About panel and Acknowledgements header.

## Geometry & Anti-Slop Architecture (R-11, R-20, R-31)
- **Zero Pill / AI-Rounded Shapes (`RADIUS = 0`):** Every window uses `DWMWA_WINDOW_CORNER_PREFERENCE = DWMWCP_DONOTROUND` (`1`). Every panel, button, toggle, and input uses sharp `0px` rectangular manga-panel frames with `1px` structural borders and `2px` corner tick marks.
- **Identity Motif (Choker-Pin Hardware & Manga Frames):**
  - **Toggles (`VikeyToggle`):** Rectangular mechanical slide track with a solid rectangular actuator block, a metallic pin-ring circle motif on the thumb, and explicit `ON` / `OFF` (`I` / `O`) state geometry so state never relies on color alone (R-39).
  - **Section Headers:** Framed with a solid square index block and a horizontal rule, echoing technical dossier / manga chapter markers.

## Dual Theme Palette & WCAG AA Verification (R-01, R-21, R-25, R-29, R-34)
Both themes are switchable live from the Main Window header (`ĐÊM` / `NGÀY`) and persisted in `HKCU\Software\Vikey\RezeTheme`.

### 1. Dark Mode: "Đêm Mưa / Soviet Nocturne" (Default, `RezeTheme = 0`)
- **Window Base (`BG`):** `#0B0F17` (Abyssal Night)
- **Panel Surface (`GROUP`):** `#121926` (Brutalist Slate)
- **Control / Input Surface (`FIELD`):** `#192334`
- **Primary Text (`TEXT`):** `#F2F5FA` (Narcissus White: `17.55:1` on `BG`, `16.11:1` on `GROUP`)
- **Secondary Text (`SECONDARY`):** `#A6B4C9` (Fog Grey-Blue: `9.13:1` on `BG`, `8.38:1` on `GROUP`)
- **Tertiary Text (`TERTIARY`):** `#8FA2BD` (Muted Steel: `6.77:1` on `GROUP`)
- **Structural Border (`BORDER`):** `#647894` (`3.90:1` non-text contrast against `GROUP`)
- **Primary Accent (`ACCENT`):** `#38BDF8` (Bioluminescent Cyan: `8.22:1` on `GROUP`, dark ink `#0B0F17` on top at `8.95:1`)
- **Status / Focus Ring (`EMERALD`):** `#34D399` (Reze Emerald Eye: `9.98:1` on `BG`)

### 2. Light Mode: "Crossroads Café / Graphic Azure" (`RezeTheme = 1`)
- **Window Base (`BG`):** `#EDF2F9` (Daylight Mist)
- **Panel Surface (`GROUP`):** `#FFFFFF` (Manga Paper White)
- **Control / Input Surface (`FIELD`):** `#E2EAF4`
- **Primary Text (`TEXT`):** `#0D1321` (Manga Ink: `18.55:1` on `GROUP`, `16.49:1` on `BG`)
- **Secondary Text (`SECONDARY`):** `#3E4C63` (Slate Ink: `8.68:1` on `GROUP`, `7.72:1` on `BG`)
- **Tertiary Text (`TERTIARY`):** `#526077` (Steel Ink: `6.37:1` on `GROUP`, `5.66:1` on `BG`)
- **Structural Border (`BORDER`):** `#6E7F99` (`4.07:1` non-text contrast against `GROUP`)
- **Primary Accent (`ACCENT`):** `#1D4ED8` (Electric Cobalt: `6.70:1` on `GROUP`, white ink `#FFFFFF` on top at `6.70:1`)
- **Status / Focus Ring (`EMERALD`):** `#047857` (Deep Emerald: `5.48:1` on `GROUP`)

## One-Line Decision Log (R-31)
- **Why this palette?** Extracted directly from the four `Tài nguyên` artworks (deep nocturnal navy, electric cobalt/cyan, narcissus white, and Reze's emerald eyes), verified with `contrast-check.py` to exceed WCAG AA 4.5:1 text and 3:1 boundary ratios in both modes.
- **Why sharp 0px geometry?** Explicitly requested by the owner to eliminate generic AI rounded-corner cards and reflect Soviet brutalist architecture and manga panel frames.
- **Why the split hero header & sidebar art panels?** Gives the four Reze artworks a structural editorial role in the Main Window, Onboarding, About, and Emoji Picker without obscuring controls.
- **Why Segoe UI Semibold/Bold + Consolas?** Native Windows fonts with full Vietnamese diacritic coverage; Consolas distinguishes literal Telex keystrokes (`order traf suwax nhes`) from rendered prose.
- **Why mechanical choker-pin toggles?** Replaces iOS/AI pill switches with a rectangular actuator carrying a pin-ring glyph and `ON`/`OFF` position markers so state is readable without color.

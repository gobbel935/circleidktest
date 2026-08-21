<<<<<<< HEAD
## v2.0.0
- Full GUI overhaul: the popup is now organized into a Mode sidebar (left) and a
  two-tab parameter panel (Basic / Advanced, right), so only a small, predictable
  set of controls is visible at any one time instead of everything at once.
- Fix inconsistent/overlapping button and field spacing from the old layout by
  rebuilding every control on a consistent grid with generous margins.
- Add two new duplication Modes: **Wave** (rippling sine-wave radius, with
  Frequency/Amplitude controls) and **Random** (randomized per-copy radius, with
  Jitter and a reproducible Seed).
- Add a **Direction** toggle (CW / CCW) so reversing the duplication direction no
  longer requires typing a negative Arc value.
- Add a **Pivot Offset** (Center X / Center Y) parameter so duplicates can rotate
  around a custom point instead of always the selection's own center.
- Add a **Start %** parameter to Spiral mode, so spirals can begin partway out
  instead of always starting from the center.
- Rename "Advanced Squash" to a clearer Uniform / Split X-Y **Squish Shape**
  toggle, and make the Uniform squish apply symmetrically on both axes across
  every mode for more predictable results.
- Every button, tab and toggle now shows a clear active/inactive state
  (opacity + scale) so it's obvious what's currently selected.
- Add a live "Copies / Objects" counter and a mode-specific info popup that
  updates to explain whichever Mode is currently active.
=======
## Unreleased
- Redesign the popup with a clean mode sidebar and one focused parameter panel.
- Add active Circle/Spiral and Basic/Advanced selectors with context-aware info help.
- Harden numeric input handling and copy-count calculations against invalid or zero values.
- Add Spiral Mode with a Spiral Turns control and an in-popup info guide for beginner-friendly setup.
- Fix the `Advanced Squash` toggle so advanced controls appear when enabled and the basic `Squish` control returns when disabled.
- Add an `Advanced Squash` toggle that reveals independent `Squash Horizontal` and `Squash Vertical` controls.
>>>>>>> 0bf6b945ae46b5187377e234b58aa28e25ac1984

## 1.2.3
- Remake the popup using Geode's classes, preventing a touch handler leak
  - This fixes an issue with the Better Touch Prio mod
- Add changelog

## 1.2.2 - 1.0.0
- Port to newer GD versions
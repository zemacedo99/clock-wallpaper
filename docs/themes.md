# Configuration and theme reference

INI supports unique sections/keys, full-line `;` and `#` comments, unquoted values,
and no inline comments. Unknown keys, invalid values and missing theme references
fail explicitly. Colors are `#RRGGBB`; asset paths are relative to the config file.
Section order matters: declare `[theme NAME]` before `[element NAME.ID]`.

`[wallpaper]` accepts `width`, `height`, and default `theme`. Output limits: 64–8192
pixels per side, at most 34 megapixels. Input images can be 1–8192 per side with the
same pixel budget. JPEG, PNG and BMP input; PNG and BMP output. Photos are center
cropped with bilinear sampling; EXIF orientation is not applied, so normalize photos
before use.

`[theme NAME]` accepts `background`, `color`, `text`, `text_color`, `update_seconds`.
Cadence is 60 seconds by default, any multiple of 60 up to 86400, or 0 for static
themes. Built-in hands/orbits require a nonzero cadence. A static custom renderer
may use 0; a moving custom renderer must declare a suitable cadence in its theme.

`[rule HH:MM]` accepts `theme`, `background`, `color`, and `text`. Omitted keys use
theme defaults, never the preceding rule's state. An empty `background =` clears
the theme image. All rules are daily and local; weekday/date rules are not implemented.
At a spring DST jump a skipped rule becomes active at the jump if it is now the latest
applicable rule. Fall-back occurrences are evaluated separately. Clock angles ignore
seconds and never use preview time when applying a wallpaper.

Elements are painted in declaration order:

| Key | Meaning |
| --- | --- |
| `kind` | `markers`, `hand`, `orbit`, `text`, or a registered custom renderer |
| `x`, `y` | Center as fraction of width/height; default 0.5, 0.5 |
| `radius` | Marker/orbit distance, fraction of shorter screen edge |
| `length`, `width` | Hand dimensions, fraction of shorter screen edge |
| `size` | Marker/orbit diameter, sprite maximum dimension, or text height; fraction of shorter edge |
| `source` | `hour`, `minute`, `day` (one 24-hour revolution) |
| `offset` | Clockwise degrees added to source; zero points at 12 o'clock |
| `color` | `#RRGGBB` |
| `count` | 1–360 markers |
| `image` | Optional orbit sprite; transparent PNG recommended; stays upright |
| `text` | Fixed text for a text element |

The tiny font supports A–Z, digits, spaces, colon and hyphen, with uppercase output.
Unsupported characters are blank. Use a background image for rich typography or
non-Latin text. Theme/rule text is centered near the bottom; a text element allows
custom positioning. Very long text and unusual positions should be previewed.

## Add a theme or pointer

Copy the example config, declare a theme, and add elements. Point `[wallpaper]` or a
rule at its name. `orbit` already supports a moving dot or transparent car image.
A `day` orbit is a time mapping, not astronomical sunrise/sunset calculation.

For new drawing behavior, implement the `ElementRenderer` callback in
`include/wallclock/renderer.h` and call `Renderer::register_element` at host startup.
It receives canvas, element data, local time and dimensions. Registered renderers can
replace built-ins. They must be deterministic for the declared time interval and
must not perform network access or background work. This is a compiled extension
interface, not an executable plugin loader.

Future sun/moon themes can add location, astronomical calculations and extra event
deadlines to the shared core. OS integration stays unchanged. If adding new data
fields, extend the validated parser and Element structure explicitly. The current
interface favors a small, auditable vocabulary over a general scripting engine.

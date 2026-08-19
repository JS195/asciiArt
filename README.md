# ascii-art

Converts raster images (PNG, JPEG, BMP, GIF, TGA, PSD, HDR — anything
`stb_image` reads) into ASCII art for terminal output or web embedding.

```
ascii-art photo.png --mode color --format ansi --width 160
```

## Build

```
make          # builds ./ascii-art
make test     # builds and runs the assert tests
make examples # regenerates examples/ from images/owl.png
make bull     # renders images/bull.jpg + a viewable preview
make preview  # renders and opens in a browser (make preview IMG=photo.jpg)
```

C11, no dependencies beyond the vendored `third_party/stb_image.h`. Builds
clean under `-Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes`.

## How the renderer works

Every output character is a **cell** that stands for a rectangular block of
source pixels. The pipeline runs once per cell:

```
source region -> average RGB -> luminance -> brightness/contrast
    -> ink direction -> threshold -> glyph selection
    -> foreground colour -> quantization -> encode
```

The stages live in separate translation units so each is testable on its own:

| File        | Responsibility                                              |
|-------------|-------------------------------------------------------------|
| `image.c`   | loading and ownership of pixel data                          |
| `sample.c`  | block resampling (`sample_block`)                            |
| `color.c`   | luminance, tone curve, saturation, quantization               |
| `charset.c` | character ramps and glyph selection                           |
| `render.c`  | the pipeline: image in, grid of `Cell` out                    |
| `output.c`  | encoding a finished grid as TXT / ANSI / HTML                 |
| `config.c`  | defaults, argument parsing, validation, `--help`              |

`render.c` decides everything tonal and chromatic; `output.c` only encodes an
already-final grid. That is why the three formats always agree on content, and
why the whole pipeline can be tested without touching a file.

## How source blocks map to characters

The output grid is mapped back onto the source with integer arithmetic:

```c
x0 = out_x * source_width  / output_width;
x1 = (out_x + 1) * source_width  / output_width;
y0 = out_y * source_height / output_height;
y1 = (out_y + 1) * source_height / output_height;
```

Because each cell's `x1` is the next cell's `x0`, the blocks tile the source
exactly — no gaps, no overlap, no accumulated rounding drift across the row.
Every pixel in that rectangle is averaged.

This is the main difference from the naive approach of stepping through the
image and reading one pixel per character. Sampling one pixel throws away
almost all the information in a downscale and makes the result unstable: shift
the source by a pixel and a completely different set of glyphs comes out. A
4-pixel checkerboard sampled at one pixel reads as pure black or pure white; the
same checkerboard averaged reads as the mid-grey it actually is. There is a test
for exactly this (`test_downscale_averages_not_samples`).

When the output is *wider* than the source, a block can come out empty.
`sample_block` widens any degenerate rectangle to at least 1x1, so upscaling
repeats pixels instead of dividing by zero.

Channel layouts are handled explicitly — 1 (grey), 2 (grey + alpha), 3 (RGB),
4+ (RGBA). Nothing assumes `channels >= 3`. Alpha is currently ignored rather
than composited.

## Why character aspect correction matters

Terminal glyphs are not square. In a typical monospace font a character cell is
about twice as tall as it is wide, so a grid with as many rows as columns
renders an image stretched to roughly double height.

The height is therefore derived as:

```c
output_height = (source_height / source_width) * output_width * char_aspect;
```

`--char-aspect` is the cell's **width divided by its height**, and the right
value depends on where the art is displayed:

| Target | Cell | Default |
|--------|------|---------|
| terminal (`txt`, `ansi`) | line-height ≈ 1.2× font-size, so ~2× taller than wide | `0.5` |
| web (`html`) | the emitted CSS uses `line-height: 0.72`, so the cell is nearly square | `0.83` |

Using the terminal's `0.5` for HTML is what makes a render come out squat and
stretched — measured in Chrome, the cell is 3.61 × 4.31 px (aspect 0.838), not
0.5, so the picture ends up ~40% too short. The HTML default is derived from the
same constants that generate the CSS (`HTML_LINE_HEIGHT`, `HTML_GLYPH_ADVANCE` in
`output.h`) so the two cannot drift apart, and `test_html_proportions` asserts
the rendered block stays within 2% of the source's shape.

If you override the emitted CSS with your own `line-height`, pass a matching
`--char-aspect` (roughly `0.6 / your-line-height`). `--height` overrides the
calculation entirely.

## Two kinds of density

These are independent and easy to confuse:

- **Spatial density** — `--width` / `--height`. How many cells the image is cut
  into. More cells means finer structure.
- **Tonal density** — `--charset`. How many distinguishable ink levels the
  glyphs can express. `simple` has 10 steps, `full` 70, `medium` 85.

Wide output with a coarse ramp gives sharp but posterised results; narrow output
with a fine ramp gives smooth tone with little detail.

## Rendering modes

The design principle: **glyph density carries luminance, foreground colour
carries colour.** Neither concept is smuggled into the other.

| Mode    | Glyph from | Foreground             | Use for                        |
|---------|------------|------------------------|--------------------------------|
| `mono`  | luminance  | one fixed colour       | pure ASCII, `<pre>` text, logs |
| `gray`  | luminance  | quantized grey         | high-detail monochrome         |
| `color` | luminance  | averaged source RGB    | photographic output            |

`gray` is the interesting one: tone is expressed *twice*, once as glyph coverage
and once as foreground brightness. The two layers multiply, which is what gives
it far more apparent tonal range than either alone.

`color` derives the glyph from luminance and the foreground from the averaged
RGB. `--saturation` pulls colour toward grey (`0.0`) or leaves it at source
(`1.0`); values above 1 boost. Photographic saturation is usually too busy at
small font sizes — around `0.65` reads better.

### Ink direction and `--invert`

By default a **bright** region gets the heaviest glyph and a dark region gets a
space. That is the right orientation for a terminal or a dark web page, where
glyphs are light marks on a dark field, and it is why `--threshold` exists:
near-black pixels and compression noise become blank instead of speckle.

`--invert` flips which end of the ramp counts as ink, for a light page where
dark glyphs mark a white field. It flips *only* the ink direction — the
grayscale foreground still follows source brightness. Otherwise an inverted
render would draw the darkest part of the image with the densest glyph in
white, which is invisible on the page it was inverted for.

`--threshold` follows the ink direction too, so it blanks the dark background by
default and the bright background under `--invert`.

### Input levels: the control that matters for dark images

`--contrast` pivots on the midpoint **and clamps to [0,1] before brightness is
added**. On an image whose subject sits well below mid-grey, that collapses the
subject to black instead of expanding it — raising contrast makes a dark photo
*worse*, not punchier. `--levels LO,HI` is the only control that stretches a
narrow input range onto the full output range:

```
--levels 20,195     # map input 20..195 onto 0..255
--gamma 1.25        # then lift the midtones (>1 lifts, <1 deepens)
```

Order is levels -> gamma -> contrast -> brightness. Pick `LO,HI` from the
image's actual histogram: `LO` just above the background, `HI` near the
brightest highlight you care about. This is what makes `gray` mode span the
full grey spectrum rather than the bottom few levels.

## Character ramps

Ramps are ordered most ink first and the length is always derived with
`strlen` — never hard-coded.

```
simple   @%#*+=-:.
medium   @MBHENR#KWXDFPQASUZVLGYCTOahkbdpqwmZO0QLCJUYXzcvunxrjft/\|()1{}[]?-_+~<>i!lI;:,"^`'.
full     $@B8&WM#*oahkbdpqwmZO0QLCJUYXzcvunxrjft/\|()1{}[]?-_+~<>i!lI;:,"^`'.
```

Or supply your own: `--charset-custom "@%#*+=-:. "`.

`safe` is `full` with `{` and `}` removed — see [Svelte / JSX](#svelte--jsx).

### Room for measured ramps

Selection is expressed as *wanted ink coverage* in 0..1, not as
`index = luminance * length`:

```c
char charset_glyph(const Charset *cs, float ink);
```

A `Charset` carries an optional `density` table — per-glyph ink coverage
measured by rasterising the target font. When it is `NULL` (today) glyphs are
assumed evenly spaced. When it is present, selection switches to nearest
measured coverage. Adding font calibration therefore means producing the table
and pointing `Charset.density` at it; no caller and no other stage changes. The
behaviour is already covered by `test_measured_density_ramp`.

The built-in ramps are hand-ordered and only approximately even, which is the
main quality ceiling in the current output.

## Output formats

### TXT

Plain characters, one line per row, colour ignored. Suitable for files, logs and
`<pre>` elements. Trailing blanks are trimmed — only foreground colour is ever
set, so trailing spaces are invisible in every format and dropping them is a
large size win on images with dark borders.

### ANSI

True-colour terminal output (`ESC[38;2;r;g;b m`). An escape is emitted only when
the colour actually changes, never for a space, and each line ends with a reset.
In `mono` mode no escapes are emitted at all unless `--fg` is given, so the art
inherits the terminal's own foreground.

### HTML

An embeddable fragment: a `<style>` block plus `<div class="ascii-art"><pre>`.

Adjacent cells sharing a foreground are grouped into a single `<span>`, and
spaces are emitted bare because they show no ink. On the 260x130 sample that is
33800 cells rendered as ~8000 spans rather than one node per character.

Quantization is what makes the grouping effective, and it is tunable:

| `--gray-levels` | spans | size  |   | `--color-step` | spans  | size  |
|-----------------|-------|-------|---|----------------|--------|-------|
| 4               | 3056  |  95 K |   | 8              | 11309  | 431 K |
| 8               | 5313  | 149 K |   | 32             |  8366  | 328 K |
| 16 *(default)*  | 7740  | 210 K |   | 64             |  6283  | 256 K |

Grey levels become CSS classes (`.g0` … `.g15`); colours become inline styles,
since with a useful `--color-step` there are more distinct colours than it is
worth emitting a class for. `&`, `<` and `>` are escaped — several ramps contain
them.

## Seeing the output

The HTML output is an embeddable *fragment*, so it deliberately carries no page
background — grey glyphs on a browser's default white look wrong even when the
render is correct. `preview.sh` wraps a render in a dark standalone page and
opens it:

```bash
./preview.sh images/bull.jpg --mode gray --width 300 --levels 20,195
BG='#ffffff' ./preview.sh sketch.png --mode gray --invert   # light-page check
```

It accepts every ascii-art flag except `--format`/`--output`. Set `NO_OPEN=1` to
write the file without launching a browser, and `OUT=path.html` to choose where.

## HTML embedding

The generated fragment drops straight into a page:

```html
<div class="ascii-art">
    <!-- generated output -->
</div>
```

with the CSS it emits:

```css
.ascii-art pre {
    margin: 0;
    white-space: pre;
    font-family: "JetBrains Mono", "SFMono-Regular", Consolas, monospace;
    font-size: 6px;
    line-height: 0.72;
    letter-spacing: 0;
}
```

`white-space: pre` and a monospace family are load-bearing; a proportional font
destroys the grid. `font-size` and `line-height` set the physical size — keep
their ratio near `0.72` or adjust `--char-aspect` to compensate. Override the
emitted rules by placing your own stylesheet after the fragment.

`gray` mode on a light page needs `--invert`; on a dark page it does not.

### Svelte / JSX

Template languages parse `{` and `}` as expression delimiters, and the `medium`
and `full` ramps both contain them — a 260-column render carries roughly 200 of
each. Pasted into a `.svelte` file that is a compile error, not a rendering
glitch. Two ways round it:

**Generate a component directly** with the brace-free ramp (`make svelte` does
this). The output is a valid `.svelte` file as-is:

```bash
ascii-art photo.png --mode gray --format html --width 200 \
    --charset safe --invert --threshold 8 -o src/lib/AsciiArt.svelte
```

```svelte
<script>
  import AsciiArt from '$lib/AsciiArt.svelte';
</script>
<AsciiArt />
```

**Or keep the full ramp** and import the fragment as a string, so the braces are
never parsed as markup:

```bash
ascii-art photo.png --mode gray --format html --width 200 \
    --charset full --invert --threshold 8 -o src/lib/art.html
```

```svelte
<script>
  import art from '$lib/art.html?raw';
</script>
{@html art}
```

Note that Svelte's scoped styles do not apply to `{@html}` content — the
fragment's own `<style>` block is injected as a global style instead, which
works but is not scoped. The generated-component route keeps normal scoping.

Do not put ASCII art in a `.txt` file and drop it into a page: with no
`white-space: pre`, no monospace font and default line-height, it wraps and
stretches. That is what `--format html` is for.

## Performance

Cost is `O(source_pixels)` — every pixel is read exactly once, whatever the
output size, since the blocks tile the source. Output size only affects the cell
loop and the size of the written file.

A 667x667 source at `--width 400` in colour HTML takes ~13 ms. Rendering is
single-threaded, allocates one grid of cells plus the decoded image, and is
fully deterministic: the same inputs always produce byte-identical output.

`--width` and `--height` are capped at 10000, and the grid at 20 million cells,
so a typo cannot trigger a multi-gigabyte allocation.

## Examples

Grayscale HTML for a web page:

```bash
ascii-art bull.png \
    --mode gray \
    --format html \
    --width 260 \
    --charset full \
    --contrast 1.2 \
    --brightness -5 \
    --threshold 8 \
    --gray-levels 16 \
    --output bull.html
```

Full colour HTML:

```bash
ascii-art bull.png \
    --mode color \
    --format html \
    --width 260 \
    --charset full \
    --saturation 0.65 \
    --contrast 1.15 \
    --threshold 8 \
    --color-step 32 \
    --output bull-color.html
```

Monochrome text file:

```bash
ascii-art bull.png \
    --mode mono \
    --format txt \
    --width 180 \
    --charset simple \
    --output bull.txt
```

Straight to the terminal:

```bash
ascii-art image.png --mode color --format ansi --width 160
```

Dark artwork on a white page:

```bash
ascii-art sketch.png --mode gray --format html --invert --threshold 8 --width 200
```

With no `--output`, everything goes to stdout. Filenames with spaces work
normally: `ascii-art "my photo.png"`.

See `ascii-art --help` for the full argument list, and `examples/` for generated
output.

## Not implemented

Deliberately out of scope for this version, but the structure leaves room:
glyph-density calibration (see above), edge-aware character selection,
dithering, alpha compositing, custom palettes, animation frames, WebAssembly,
threading.

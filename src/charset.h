/* charset.h -- character ramps (tonal density).
 *
 * A ramp is ordered from the most ink to the least ink, so index 0 is the
 * heaviest glyph and the last index is (normally) a space.
 *
 * Selection is expressed in terms of *wanted ink coverage* in 0..1 rather than
 * "index = luminance * length", which is what makes measured ramps pluggable:
 * set `density` to a per-glyph ink-coverage table (from rasterising the target
 * font) and selection switches from evenly-spaced-index to nearest-coverage
 * without any caller changing. Until then `density` is NULL and glyphs are
 * assumed to be uniformly spaced.
 */
#ifndef ASCII_CHARSET_H
#define ASCII_CHARSET_H

#include <stddef.h>

typedef struct {
    const char *name;
    const char *glyphs;   /* NUL-terminated, ordered most ink -> least ink */
    const float *density; /* optional, parallel to glyphs, ink coverage 0..1
                           * in descending order; NULL = assume uniform */
    size_t length;        /* strlen(glyphs), always set by charset_init/lookup */
} Charset;

extern const char *const RAMP_SIMPLE;
extern const char *const RAMP_MEDIUM;
extern const char *const RAMP_FULL;
extern const char *const RAMP_SAFE;

/* Fills `cs` with a built-in ramp ("simple", "medium", "full").
 * Returns 0 on success, non-zero if the name is unknown. */
int charset_lookup(Charset *cs, const char *name);

/* Comma-separated list of built-in names, for help text and error messages. */
const char *charset_names(void);

/* Fills `cs` from a raw ramp string. `density` may be NULL. Returns 0 on
 * success, non-zero if the ramp has fewer than 2 glyphs. */
int charset_init(Charset *cs, const char *name, const char *glyphs, const float *density);

/* Picks the glyph for a wanted ink coverage in 0..1 (1 = maximum ink). */
char charset_glyph(const Charset *cs, float ink);

#endif /* ASCII_CHARSET_H */

#include "charset.h"

#include <string.h>

const char *const RAMP_SIMPLE =
    "@%#*+=-:. ";

const char *const RAMP_MEDIUM =
    "@MBHENR#KWXDFPQASUZVLGYCTOahkbdpqwmZO0QLCJUYXzcvunxrjft/\\|()1{}[]?-_+~<>i!lI;:,\"^`'. ";

const char *const RAMP_FULL =
    "$@B8&WM#*oahkbdpqwmZO0QLCJUYXzcvunxrjft/\\|()1{}[]?-_+~<>i!lI;:,\"^`'. ";

/* RAMP_FULL without { and }, which template languages (Svelte, JSX, Vue,
 * Handlebars) parse as expression delimiters -- a fragment containing them is a
 * compile error when pasted into a component. Costs 2 of 70 tonal steps. */
const char *const RAMP_SAFE =
    "$@B8&WM#*oahkbdpqwmZO0QLCJUYXzcvunxrjft/\\|()1[]?-_+~<>i!lI;:,\"^`'. ";

/* Built-in ramps. `density` stays NULL until font calibration lands; a
 * calibrated build only has to point it at a measured coverage table. */
static const struct {
    const char *name;
    const char *const *glyphs;
    const float *density;
} builtin[] = {
    {"simple", &RAMP_SIMPLE, NULL},
    {"medium", &RAMP_MEDIUM, NULL},
    {"full",   &RAMP_FULL,   NULL},
    {"safe",   &RAMP_SAFE,   NULL}
};

int charset_lookup(Charset *cs, const char *name) {
    size_t i;
    for (i = 0; i < sizeof(builtin) / sizeof(builtin[0]); i++) {
        if (strcmp(builtin[i].name, name) == 0) {
            return charset_init(cs, builtin[i].name, *builtin[i].glyphs, builtin[i].density);
        }
    }
    return 1;
}

const char *charset_names(void) {
    return "simple, medium, full, safe";
}

int charset_init(Charset *cs, const char *name, const char *glyphs, const float *density) {
    size_t len;
    if (glyphs == NULL) return 1;
    len = strlen(glyphs);
    if (len < 2) return 1;
    cs->name = name;
    cs->glyphs = glyphs;
    cs->density = density;
    cs->length = len;
    return 0;
}

char charset_glyph(const Charset *cs, float ink) {
    size_t last = cs->length - 1;

    if (ink < 0.0f) ink = 0.0f;
    if (ink > 1.0f) ink = 1.0f;

    if (cs->density != NULL) {
        /* Measured ramp: pick the glyph whose ink coverage is closest.
         * ponytail: linear scan; ramps are <= a few hundred glyphs, switch to
         * a precomputed 256-entry lookup if this ever shows up in a profile. */
        size_t i, best = 0;
        float best_err = -1.0f;
        for (i = 0; i < cs->length; i++) {
            float err = cs->density[i] - ink;
            if (err < 0.0f) err = -err;
            if (best_err < 0.0f || err < best_err) {
                best_err = err;
                best = i;
            }
        }
        return cs->glyphs[best];
    }

    /* Uniform ramp: index 0 is maximum ink, last index is minimum ink. */
    return cs->glyphs[(size_t)((1.0f - ink) * (float)last + 0.5f)];
}

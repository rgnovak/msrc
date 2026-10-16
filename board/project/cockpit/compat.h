#ifndef COCKPIT_COMPAT_H
#define COCKPIT_COMPAT_H

/* A handful of Arduino convenience macros that the ported RC-Cockpit
 * rendering code (originally an Arduino sketch) relies on. Defined here
 * exactly as Arduino itself defines them, so the ported .c files below
 * did not need their call sites rewritten - only the #include swapped
 * from <Arduino.h> to this header. */

#ifndef constrain
#define constrain(x, lo, hi) ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))
#endif

#ifndef map
#define map(x, in_min, in_max, out_min, out_max) \
    (((x) - (in_min)) * ((out_max) - (out_min)) / ((in_max) - (in_min)) + (out_min))
#endif

#ifndef DEG_TO_RAD
#define DEG_TO_RAD (0.017453292519943295f)
#endif
#ifndef RAD_TO_DEG
#define RAD_TO_DEG (57.29577951308232f)
#endif

#endif /* COCKPIT_COMPAT_H */

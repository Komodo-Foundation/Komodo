/* NULL cannot be passed for a handle argument from Komodo (only `h == 0`
 * tests it), so these wrappers supply the NULL context and match data
 * that pcre2_match_data_create_from_pattern and pcre2_match need. */
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

pcre2_code *km_pcre2_compile(const char *pattern) {
    int err;
    PCRE2_SIZE offset;
    return pcre2_compile((PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED, 0, &err, &offset, NULL);
}

/* 1 when the compiled pattern matches somewhere in subject, else 0. */
int km_pcre2_matches(pcre2_code *re, const char *subject) {
    pcre2_match_data *md = pcre2_match_data_create_from_pattern(re, NULL);
    int rc = pcre2_match(re, (PCRE2_SPTR)subject, PCRE2_ZERO_TERMINATED, 0, 0, md, NULL);
    pcre2_match_data_free(md);
    return rc >= 0;
}

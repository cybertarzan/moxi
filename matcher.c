/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sysexits.h>
#include <sys/time.h>
#include <assert.h>
#include "matcher.h"

#define MATCHER_MAGIC 0xa135b21a

void matcher_add(matcher *m, char *pattern);

void matcher_init(matcher *m, char *spec) {
    assert(m);
    assert(!matcher_initted(m));
    memset(m, 0, sizeof(matcher));
    m->initted = MATCHER_MAGIC;

    struct timeval timer;
    gettimeofday(&timer, NULL);
    m->version = ((timer.tv_sec * 1000) +
                  (timer.tv_usec / 1000));

    // The spec currently is a string of '|' separated prefixes.
    //
    if (spec != NULL &&
        strlen(spec) > 0) {
        char *next = spec;
        while (next != NULL) {
            char *patt = strsep(&next, "|");
            if (patt != NULL) {
                matcher_add(m, patt);
            }
        }
    }
}

bool matcher_initted(matcher *m) {
    return m != NULL && m->initted == MATCHER_MAGIC;
}

matcher *matcher_clone(matcher *m, matcher *copy) {
    assert(m);
    assert(matcher_initted(m));
    if (!matcher_initted(m)) return NULL;
    assert(m->patterns_num <= m->patterns_max);

    assert(copy);
    assert(!matcher_initted(copy));
    matcher_init(copy, NULL);

    copy->version = m->version;
    copy->patterns_max = m->patterns_num; // Optimize copy's array size.
    copy->patterns_num = m->patterns_num;

    if (copy->patterns_max > 0) {
        copy->patterns = calloc(copy->patterns_max, sizeof(char *));
        copy->lengths  = calloc(copy->patterns_max, sizeof(int));
        copy->hits     = calloc(copy->patterns_max, sizeof(uint64_t));
        if (copy->patterns != NULL &&
            copy->lengths != NULL &&
            copy->hits != NULL) {
            for (int i = 0; i < copy->patterns_num; i++) {
                assert(m->patterns[i]);
                copy->patterns[i] = strdup(m->patterns[i]);
                if (copy->patterns[i] == NULL)
                    goto fail;

                copy->lengths[i] = m->lengths[i];

                // Note we don't copy statistics.
            }
            return copy;
        }
    } else
        return copy;

 fail:
    matcher_uninit(copy);

    return NULL;
}

void matcher_uninit(matcher *m) {
    if (m == NULL)
        return;

    if (m->patterns != NULL) {
        for (int i = 0; i < m->patterns_num; i++) {
            free(m->patterns[i]);
        }
    }

    free(m->patterns);
    free(m->lengths);
    free(m->hits);

    memset(m, 0, sizeof(matcher)); // Helps clear m->initted.
}

void matcher_add(matcher *m, char *pattern) {
    assert(m);
    assert(matcher_initted(m));
    assert(m->patterns_num <= m->patterns_max);
    assert(pattern);

    int length = strlen(pattern);
    if (length <= 0)
        return;

    if (m->patterns_num >= m->patterns_max) {
        int    nmax = (m->patterns_max * 2) + 4; // 4 is slop when 0.
        char **npatterns = realloc(m->patterns, nmax * sizeof(char *));
        int   *nlengths  = realloc(m->lengths,  nmax * sizeof(int));
        uint64_t *nhits  = realloc(m->hits,     nmax * sizeof(uint64_t));
        if (npatterns != NULL &&
            nlengths != NULL &&
            nhits != NULL) {
            m->patterns_max = nmax;
            m->patterns     = npatterns;
            m->lengths      = nlengths;
            m->hits         = nhits;
        } else {
            free(npatterns);
            free(nlengths);
            free(nhits);
            return;
        }
    }

    assert(m->patterns_num < m->patterns_max);

    m->patterns[m->patterns_num] = strdup(pattern);
    if (m->patterns[m->patterns_num] != NULL) {
        m->lengths[m->patterns_num] = strlen(pattern);
        m->patterns_num++;
        m->version++;
    }
}

bool matcher_check(matcher *m, char *str, int str_len) {
    assert(m);
    if (!matcher_initted(m)) return false;
    assert(m->patterns_num <= m->patterns_max);

    for (int i = 0; i < m->patterns_num; i++) {
        assert(m->patterns);
        assert(m->lengths);
        assert(m->hits);

        int n = m->lengths[i];
        if (n <= str_len) {
            if (strncmp(str, m->patterns[i], n) == 0) {
                m->hits[i]++;
                return true;
            }
        }
    }

    m->misses++;

    return false;
}



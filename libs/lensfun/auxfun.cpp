/*
    Auxiliary helper functions for lensfun
    Copyright (C) 2007 by Andrew Zabolotny
*/

#include "config.h"
#include "lensfun.h"
#include "lensfunprv.h"
#include <locale.h>
#include <ctype.h>
#include <math.h>

void lf_free (void *data)
{
    g_free (data);
}

const char *lf_mlstr_get (const lfMLstr str)
{
    if (!str)
        return str;

    /* Get the current locale for messages */
    const char *lc_msg = setlocale (LC_MESSAGES, NULL);
    char lang [10];

    if (lc_msg)
    {
        const char *u = strchr (lc_msg, '_');
        if (!u || (size_t (u - lc_msg) >= sizeof (lang)))
            lc_msg = NULL;
        else
        {
            memcpy (lang, lc_msg, u - lc_msg);
            lang [u - lc_msg] = 0;
        }
    }

    if (!lc_msg)
        strcpy (lang, "en");

    /* Default value if no language matches */
    const char *def = str;
    /* Find the corresponding string in the lot */
    const char *cur = strchr (str, 0) + 1;
    while (*cur)
    {
        const char *next = strchr (cur, 0) + 1;
        if (!strcmp (cur, lang))
            return next;
        if (!strcmp (cur, "en"))
            def = next;
        if (*(cur = next))
            cur = strchr (cur, 0) + 1;
    }

    return def;
}

lfMLstr lf_mlstr_add (lfMLstr str, const char *lang, const char *trstr)
{
    if (!trstr)
        return str;
    size_t trstr_len = strlen (trstr) + 1;

    /* Find the length of multi-language string */
    size_t str_len = 0;
    if (str)
    {
        str_len = strlen (str) + 1;
        while (str [str_len])
            str_len += 1 + strlen (str + str_len + 1);
    }

    if (!lang)
    {
        /* Replace the default string */
        size_t def_str_len = str ? strlen (str) + 1 : 0;

        memcpy (str + trstr_len, str + def_str_len, str_len - def_str_len);

        str = (char *)g_realloc (str, str_len - def_str_len + trstr_len + 1);

        memcpy (str, trstr, trstr_len);
        str_len = str_len - def_str_len + trstr_len;
        str [str_len] = 0;

        return str;
    }

    size_t lang_len = lang ? strlen (lang) + 1 : 0;

    str = (char *)g_realloc (str, str_len + lang_len + trstr_len + 1);
    memcpy (str + str_len, lang, lang_len);
    memcpy (str + str_len + lang_len, trstr, trstr_len);
    str [str_len + lang_len + trstr_len] = 0;

    return str;
}

lfMLstr lf_mlstr_dup (const lfMLstr str)
{
    /* Find the length of multi-language string */
    size_t str_len = 0;
    if (str)
    {
        str_len = strlen (str) + 1;
        while (str [str_len])
            str_len += 1 + strlen (str + str_len + 1);
    }
    
    gchar *ret = (char *)g_malloc (str_len);
    memcpy (ret, str, str_len);
    return ret;
}

void _lf_list_free (void **list)
{
    int i;

    if (!list)
        return;

    for (i = 0; list [i]; i++)
        g_free (list [i]);

    g_free (list);
}

void _lf_setstr (gchar **var, const gchar *val)
{
    if (*var)
        g_free (*var);

    *var = g_strdup (val);
}

void _lf_addstr (gchar ***var, const gchar *val)
{
    _lf_addobj ((void ***)var, val, strlen (val) + 1);
}

void _lf_addobj (void ***var, const void *val, size_t val_size)
{
    int n = 0;

    if (*var)
        for (n = 0; (*var) [n]; n++)
            ;

    n++;

    (*var) = (void **)g_realloc (*var, (n + 1) * sizeof (void *));
    (*var) [n - 1] = g_malloc (val_size);
    memcpy ((*var) [n - 1], val, val_size);
    (*var) [n] = NULL;
}

void _lf_xml_printf (GString *output, char *format, ...)
{
    va_list args;
    va_start (args, format);
    gchar *s = g_markup_vprintf_escaped (format, args);
    va_end (args);

    g_string_append (output, s);

    g_free (s);
}

void _lf_xml_printf_mlstr (GString *output, const char *prefix,
                                 const char *element, const lfMLstr val)
{
    if (!val)
        return;

    _lf_xml_printf (output, "%s<%s>%s</%s>\n", prefix, element, val, element);

    for (const char *cur = val;;)
    {
        cur = strchr (cur, 0) + 1;
        if (!*cur)
            break;
        const char *lang = cur;
        cur = strchr (cur, 0) + 1;
        _lf_xml_printf (output, "%s<%s lang=\"%s\">%s</%s>\n",
                        prefix, element, lang, cur, element);
    }
}

int _lf_ptr_array_insert_sorted (
    GPtrArray *array, void *item, GCompareFunc compare)
{
    int length = array->len;
    g_ptr_array_set_size (array, length + 1);
    void **root = array->pdata;

    int m = 0, l = 0, r = length - 1;

    // Skip trailing NULL, if any
    if (l <= r && !root [r])
        r--;

    while (l <= r)
    {
        m = (l + r) / 2;
        int cmp = compare (root [m], item);

        if (cmp == 0)
        {
            ++m;
            goto done;
        }
        else if (cmp < 0)
            l = m + 1;
        else
            r = m - 1;
    }
    if (r == m)
        m++;

done:
    memmove (root + m + 1, root + m, (length - m) * sizeof (void *));
    root [m] = item;
    return m;
}

int _lf_ptr_array_insert_unique (
    GPtrArray *array, void *item, GCompareFunc compare, GDestroyNotify dest)
{
    int idx1, idx2;
    int idx = _lf_ptr_array_insert_sorted (array, item, compare);
    void **root = array->pdata;
    int length = array->len;

    for (idx1 = idx - 1; idx1 >= 0 && compare (root [idx1], item) == 0; idx1--)
        ;
    for (idx2 = idx + 1; idx2 < length && compare (root [idx2], item) == 0; idx2++)
        ;

    if (dest)
        for (int i = idx1 + 1; i < idx2; i++)
            if (i != idx)
                dest (g_ptr_array_index (array, i));

    if (idx2 - idx - 1)
        g_ptr_array_remove_range (array, idx + 1, idx2 - idx - 1);
    if (idx - idx1 - 1)
        g_ptr_array_remove_range (array, idx1 + 1, idx - idx1 - 1);

    return idx1 + 1;
}

int _lf_ptr_array_find_sorted (
    const GPtrArray *array, void *item, GCompareFunc compare)
{
    int length = array->len;
    if (!length)
      return -1;

    void **root = array->pdata;

    int l = 0, r = length - 1;
    int m = 0, cmp = 0;

    // Skip trailing NULL, if any
    if (!root [r])
        r--;

    while (l <= r)
    {
        m = (l + r) / 2;
        cmp = compare (root [m], item);

        if (cmp == 0)
            return m;
        else if (cmp < 0)
            l = m + 1;
        else
            r = m - 1;
    }

    return -1;
}

int _lf_strcmp (const char *s1, const char *s2)
{
    if (!s1)
        if (!s2)
            return 0;
        else
            return -1;
    if (!s2)
        return +1;

    bool begin = true;
    for (;;)
    {
skip_start_spaces_s1:
        gunichar c1 = g_utf8_get_char (s1);
        s1 = g_utf8_next_char (s1);
        if (g_unichar_isspace (c1))
        {
            c1 = L' ';
            while (g_unichar_isspace (g_utf8_get_char (s1)))
                s1 = g_utf8_next_char (s1);
        }
        if (begin && c1 == L' ')
            goto skip_start_spaces_s1;
        c1 = g_unichar_tolower (c1);

skip_start_spaces_s2:
        gunichar c2 = g_utf8_get_char (s2);
        s2 = g_utf8_next_char (s2);
        if (g_unichar_isspace (c2))
        {
            c2 = L' ';
            while (g_unichar_isspace (g_utf8_get_char (s2)))
                s2 = g_utf8_next_char (s2);
        }
        if (begin && c2 == L' ')
            goto skip_start_spaces_s2;
        c2 = g_unichar_tolower (c2);

        begin = false;
        if (c1 == c2)
        {
            if (!c1)
                return 0;
            continue;
        }

        if ((!c1 && c2 == L' ') || (!c2 && c1 == L' '))
            return 0; // strings are equal, ignore spaces at the end of one

        return c1 - c2;
    }
}

float _lf_interpolate (float y1, float y2, float y3, float y4, float t)
{
    float tg2, tg3;
    float t2 = t * t;
    float t3 = t2 * t;

    if (y1 == FLT_MAX)
        tg2 = y3 - y2;
    else
        tg2 = (y3 - y1) * 0.5;

    if (y4 == FLT_MAX)
        tg3 = y3 - y2;
    else
        tg3 = (y4 - y2) * 0.5;

    // Hermite polynomial
    return (2 * t3 - 3 * t2 + 1) * y2 +
        (t3 - 2 * t2 + t) * tg2 +
        (-2 * t3 + 3 * t2) * y3 +
        (t3 - t2) * tg3;
}

//------------------------// Fuzzy string matching //------------------------//

lfFuzzyStrCmp::lfFuzzyStrCmp (const char *pattern)
{
    pattern_words = g_ptr_array_new ();
    match_words = g_ptr_array_new ();
    Split (pattern, pattern_words);
}

lfFuzzyStrCmp::~lfFuzzyStrCmp ()
{
    Free (pattern_words);
    g_ptr_array_free (pattern_words, TRUE);
    g_ptr_array_free (match_words, TRUE);
}

void lfFuzzyStrCmp::Free (GPtrArray *dest)
{
    for (size_t i = 0; i < dest->len; i++)
        g_free (g_ptr_array_index (dest, i));
    g_ptr_array_set_size (dest, 0);
}

void lfFuzzyStrCmp::Split (const char *str, GPtrArray *dest)
{
    if (!str)
        return;

    while (*str)
    {
        while (*str && isspace (*str))
            str++;
        if (!*str)
            break;

        const char *word = str++;

        // Split into words based on character class
        if (isdigit (*word))
            while (*str && (isdigit (*str) || *str == '.'))
                str++;
        else if (ispunct (*word))
            while (*str && ispunct (*str))
                str++;
        else
            while (*str && !isspace (*str) && !isdigit (*str) && !ispunct (*str))
                str++;

        // Skip solitary symbols
        if ((str - word == 1) &&
            (*word == '/' || *word == '-' || *word == '(' || *word == ')' ||
             tolower (*word) == 'f'))
            continue;

        gchar *item = g_strndup (word, str - word);
        _lf_ptr_array_insert_sorted (dest, item, (GCompareFunc)strcasecmp);
    }
}

int lfFuzzyStrCmp::Compare (const char *match)
{
    Split (match, match_words);
    if (!match_words->len)
        return 0;

    size_t mi = 0;
    for (size_t pi = 0; pi < pattern_words->len; pi++)
    {
        const char *pattern_str = (char *)g_ptr_array_index (pattern_words, pi);
        for (; mi < match_words->len; mi++)
        {
            int r = strcasecmp (pattern_str,
                                (char *)g_ptr_array_index (match_words, mi));

            if (r == 0)
                break;

            if (r < 0)
            {
                // Since our arrays are sorted, if pattern word becomes
                // "smaller" than next word from the match, this means
                // there's no chance anymore to find it.
                Free (match_words);
                return 0;
            }
        }

        if (mi >= match_words->len)
        {
            Free (match_words);
            return 0; // Found a word not present in pattern
        }
        
        mi++;
    }

    // Allright, all words from pattern were found in the match.
    // Compute the score.

    int score = (pattern_words->len * 100) / match_words->len;
    Free (match_words);

    return score;
}

double _lf_rt3 (double x)
{
    if (x < 0.0)
        return -pow (-x, 1.0 / 3.0);
    else
        return pow (x, 1.0 / 3.0);
}

double _lf_rt6 (double x)
{
    if (x < 0.0)
        return -pow (-x, 1.0 / 6.0);
    else
        return pow (x, 1.0 / 6.0);
}

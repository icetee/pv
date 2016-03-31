/*
 * Very minimal (and stupid) implementation of gettext, with a fixed lookup
 * table.
 *
 * This library ONLY handles gettext(), and that only for the basic form (it
 * translates strings to other strings with no other modification, so %2$d
 * style constructs are not dealt with). The setlocale(), bindtextdomain(),
 * and textdomain() functions are ignored.
 *
 * To use this library, create a function that, given a language string,
 * returns a struct msg_table_s[] of msgid and msgstr pairs, with the end
 * of the table being marked by a NULL msgid. The po2table.sh script will do
 * this.
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef HAVE_GETTEXT

struct msgtable_s {
	char *msgid;
	char *msgstr;
};

#if ENABLE_NLS
struct msgtable_s *minigettext__gettable(char *);
#else				/* ENABLE_NLS */
struct msgtable_s *minigettext__gettable(char *a)
{
	return NULL;
}
#endif				/* ENABLE_NLS */

char *minisetlocale(char *a, char *b)
{
	return NULL;
}


char *minibindtextdomain(char *a, char *b)
{
	return NULL;
}


char *minitextdomain(char *a)
{
	return NULL;
}


char *minigettext(char *msgid)
{
	static struct msgtable_s *table = NULL;
	static int tried_lang = 0;
	char *lang;
	int i;

	if (NULL == msgid)
		return msgid;

	if (0 == tried_lang) {
		lang = getenv("LANGUAGE");
		if (lang)
			table = minigettext__gettable(lang);

		if (NULL == table) {
			lang = getenv("LANG");
			if (lang)
				table = minigettext__gettable(lang);
		}

		if (NULL == table) {
			lang = getenv("LC_ALL");
			if (lang)
				table = minigettext__gettable(lang);
		}

		if (NULL == table) {
			lang = getenv("LC_MESSAGES");
			if (lang)
				table = minigettext__gettable(lang);
		}

		tried_lang = 1;
	}

	if (NULL == table)
		return msgid;

	for (i = 0; table[i].msgid; i++) {
		if (0 == strcmp(table[i].msgid, msgid)) {
			if (0 == table[i].msgstr)
				return msgid;
			if (0 == table[i].msgstr[0])
				return msgid;
			return table[i].msgstr;
		}
	}

	return msgid;
}

#endif				/* HAVE_GETTEXT */

/* EOF */

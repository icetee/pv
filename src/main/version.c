/*
 * Output version information to stdout.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>


/*
 * Display current package version.
 */
void display_version(void)
{
	printf(_("%s %s - Copyright %s %s"),
	       PROGRAM_NAME, VERSION, COPYRIGHT_YEAR, COPYRIGHT_HOLDER);
	printf("\n\n");
	printf(_("Web site: %s"), PROJECT_HOMEPAGE);
	printf("\n\n");
	printf("%s",
	       _("This program is free software, and is being distributed "
		 "under the\nterms of the Artistic License 2.0."));
	printf("\n\n");
	printf("%s",
	       _
	       ("This program is distributed in the hope that it will be useful,\n"
		"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
		"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE."));
	printf("\n\n");
}

/* EOF */

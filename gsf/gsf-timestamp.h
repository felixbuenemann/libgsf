/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-timestamp.h: A useful little type for metadata, contains a superset of
 *		 all the features it would be nice to have.
 *
 * Copyright (C) 2002 Jody Goldberg (jody@gnome.org)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifndef GSF_TIMESTAMP_H
#define GSF_TIMESTAMP_H

#include <gsf/gsf.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GSF_TIMESTAMP_TYPE        (gsf_timestamp_get_type ())

struct _GsfTimestamp {
	GDate	  date;			/* In local timezone */
	glong     seconds;		/* time of day */
	GString	  time_zone;		/* possibly blank */

	guint32	  timet;
};

GType gsf_timestamp_get_type (void);

GsfTimestamp *gsf_timestamp_new_now	(void);
GsfTimestamp *gsf_timestamp_new		(GDate const *date, glong seconds,
					 char const *tz);

GsfTimestamp *gsf_timestamp_copy	(GsfTimestamp const *stamp);
void          gsf_timestamp_free	(GsfTimestamp       *stamp);
int           gsf_timestamp_parse	(char const *spec, GsfTimestamp *stamp);
char 	     *gsf_timestamp_as_string	(GsfTimestamp const *stamp);
guint         gsf_timestamp_hash	(GsfTimestamp const *stamp);
gboolean      gsf_timestamp_equal	(GsfTimestamp const *a,
					 GsfTimestamp const *b);

void g_value_set_timestamp (GValue *value, GsfTimestamp const *stamp);

G_END_DECLS

#endif /* GSF_TIMESTAMP_H */
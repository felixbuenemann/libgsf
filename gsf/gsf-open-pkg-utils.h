/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-open-pkg-utils.h: Utilities for handling Open Package zip files
 * 			from MS Office 2007 or XPS.
 *
 * Copyright (C) 2006-2007 Jody Goldberg (jody@gnome.org)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#ifndef GSF_OPEN_PKG_UTILS_H
#define GSF_OPEN_PKG_UTILS_H

#include <gsf/gsf.h>
#include <gsf/gsf-libxml.h>

G_BEGIN_DECLS

typedef struct _GsfOpenPkgRel	GsfOpenPkgRel;
typedef struct _GsfOpenPkgRels	GsfOpenPkgRels;

GsfInput *gsf_open_pkg_get_rel_by_type (GsfInput *in, char const *type);
GsfInput *gsf_open_pkg_get_rel_by_id   (GsfInput *in, char const *id);
GError   *gsf_open_pkg_parse_rel_by_id (GsfXMLIn *xin, char const *part_id,
					GsfXMLInNode const *dtd,
					GsfXMLInNS const *ns);

typedef struct _GsfOutfileOpenPkg GsfOutfileOpenPkg;

#define GSF_OUTFILE_OPEN_PKG_TYPE	(gsf_outfile_open_pkg_get_type ())
#define GSF_OUTFILE_OPEN_PKG(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GSF_OUTFILE_OPEN_PKG_TYPE, GsfOutfileOpenPkg))
#define GSF_IS_OUTFILE_OPEN_PKG(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), GSF_OUTFILE_OPEN_PKG_TYPE))

GType	    gsf_outfile_open_pkg_get_type	(void) G_GNUC_CONST;
void	    gsf_outfile_open_pkg_register_type	(GTypeModule *module);

GsfOutfile *gsf_outfile_open_pkg_new	  	  (GsfOutfile *sink);
void	    gsf_outfile_open_pkg_set_sink	  (GsfOutfileOpenPkg *open_pkg,
						   GsfOutput *sink);
void	    gsf_outfile_open_pkg_set_content_type (GsfOutfileOpenPkg *open_pkg,
						   char const *content_type);
char const *gsf_outfile_open_pkg_relate		  (GsfOutfileOpenPkg *child,
						   GsfOutfileOpenPkg *parent,
						   char const *type);
GsfOutput  *gsf_outfile_open_pkg_add_rel	  (GsfOutfile *dir,
						   char const *name,
						   char const *content_type,
						   GsfOutfile *parent,
						   char const *type);

gint gsf_open_pkg_error_id (void);

G_END_DECLS

#endif /* GSF_OPEN_PKG_UTILS_H */
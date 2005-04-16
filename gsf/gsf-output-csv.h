/*
 * gsf-output-csv.h: a GsfOutput to write .csv style files.
 *
 * Copyright (C) 2005 Morten Welinder (terra@gnome.org)
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifndef GSF_OUTPUT_CSV_H
#define GSF_OUTPUT_CSV_H

#include "gsf-output.h"

G_BEGIN_DECLS

typedef enum {
	GSF_OUTPUT_CSV_QUOTING_MODE_NEVER,
	GSF_OUTPUT_CSV_QUOTING_MODE_AUTO,
	GSF_OUTPUT_CSV_QUOTING_MODE_ALWAYS
} GsfOutputCsvQuotingMode;
GType gsf_output_csv_quoting_mode_get_type (void);
#define GSF_OUTPUT_CSV_QUOTING_MODE_TYPE (gsf_output_csv_quoting_mode_get_type ())

typedef struct {
	GsfOutput output;

	GsfOutput *sink;

	char *quote;
	size_t quote_len;
	GsfOutputCsvQuotingMode quoting_mode;
	char *quoting_triggers;

	char *eol;
	size_t eol_len;
	char *separator;
	size_t separator_len;
	gboolean fields_on_line;

	GString *buf;
} GsfOutputCsv;

#define GSF_OUTPUT_CSV_TYPE        (gsf_output_csv_get_type ())
#define GSF_OUTPUT_CSV(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), GSF_OUTPUT_CSV_TYPE, GsfOutputCsv))
#define GSF_IS_OUTPUT_CSV(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), GSF_OUTPUT_CSV_TYPE))
GType gsf_output_csv_get_type (void);
gboolean gsf_output_csv_write_field (GsfOutputCsv *csv,
				     char const *field,
				     size_t len);
gboolean gsf_output_csv_write_eol (GsfOutputCsv *csv);

typedef struct {
	GsfOutputClass output_class;
} GsfOutputCsvClass;

G_END_DECLS

#endif /* GSF_OUTPUT_CSV_H */

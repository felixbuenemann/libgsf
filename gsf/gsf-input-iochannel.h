/*
 * gsf-input-iochannel.h: interface for use by the structured file layer to read data from IO channels
 *
 * Copyright (C) 2002 Rodrigo Moya (rodrigo@gnome-db.org)
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

#ifndef GSF_INPUT_IOCHANNEL_H
#define GSF_INPUT_IOCHANNEL_H

#include "gsf-input.h"
#include <gsf-input-memory.h>

G_BEGIN_DECLS

GsfInputMemory * gsf_input_memory_new_from_iochannel (GIOChannel * channel,
						      GError ** error);

G_END_DECLS

#endif /* GSF_INPUT_IOCHANNEL_H */

/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gsf-libxml.c :
 *
 * Copyright (C) 2002-2005 Jody Goldberg (jody@gnome.org)
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

#include <gsf-config.h>
#include <gsf/gsf-libxml.h>
#include <gsf/gsf-input.h>
#include <gsf/gsf-output.h>
#include <gsf/gsf-input-gzip.h>
#include <gsf/gsf-impl-utils.h>
#include <gsf/gsf-utils.h>
#include <gsf/gsf-timestamp.h>

#include <math.h>
#include <string.h>

static GObjectClass *parent_class;

static gint
glade_enum_from_string (GType type, const char *string)
{
    GEnumClass *eclass;
    GEnumValue *ev;
    gchar *endptr;
    gint ret = 0;

    ret = strtoul(string, &endptr, 0);
    if (endptr != string) /* parsed a number */
	return ret;

    eclass = g_type_class_ref(type);
    ev = g_enum_get_value_by_name(eclass, string);
    if (!ev) ev = g_enum_get_value_by_nick(eclass, string);
    if (ev)  ret = ev->value;

    g_type_class_unref(eclass);

    return ret;
}

static char const *
glade_string_from_enum (GType type, gint value)
{
    GEnumClass *eclass;
    GEnumValue *ev;

    eclass = g_type_class_ref(type);

    ev = g_enum_get_value (eclass, value);

    g_type_class_unref(eclass);

    return ev ? ev->value_name : "0";
}

static guint
glade_flags_from_string (GType type, const char *string)
{
    GFlagsClass *fclass;
    gchar *endptr, *prevptr;
    guint i, j, ret = 0;
    char *flagstr;

    ret = strtoul(string, &endptr, 0);
    if (endptr != string) /* parsed a number */
	return ret;

    fclass = g_type_class_ref(type);


    flagstr = g_strdup (string);
    for (ret = i = j = 0; ; i++) {
	gboolean eos;

	eos = flagstr [i] == '\0';
	
	if (eos || flagstr [i] == '|') {
	    GFlagsValue *fv;
	    const char  *flag;
	    gunichar ch;

	    flag = &flagstr [j];
            endptr = &flagstr [i];

	    if (!eos) {
		flagstr [i++] = '\0';
		j = i;
	    }

            /* trim spaces */
	    for (;;)
	      {
		ch = g_utf8_get_char (flag);
		if (!g_unichar_isspace (ch))
		  break;
		flag = g_utf8_next_char (flag);
	      }

	    while (endptr > flag)
	      {
		prevptr = g_utf8_prev_char (endptr);
		ch = g_utf8_get_char (prevptr);
		if (!g_unichar_isspace (ch))
		  break;
		endptr = prevptr;
	      }

	    if (endptr > flag)
	      {
		*endptr = '\0';
		fv = g_flags_get_value_by_name (fclass, flag);

		if (!fv)
		  fv = g_flags_get_value_by_nick (fclass, flag);

		if (fv)
		  ret |= fv->value;
		else
		  g_warning ("Unknown flag: '%s'", flag);
	      }

	    if (eos)
		break;
	}
    }
    
    g_free (flagstr);

    g_type_class_unref(fclass);

    return ret;
}
static gchar *
glade_string_from_flags (GType type, guint flags)
{
    GFlagsClass *flags_class;
    GString *string;
    char *ret;

    flags_class = g_type_class_ref (type);

    string = g_string_new ("");

    if (flags_class->n_values)
      {
	GFlagsValue *fval;
      
	for (fval = flags_class->values; fval->value_name; fval++)
	  {
	    /* We have to be careful as some flags include 0 values, e.g.
	       BonoboDockItemBehavior uses 0 for BONOBO_DOCK_ITEM_BEH_NORMAL.
	       If a 0 value is available, we only output it if the entire
	       flags value is 0, otherwise we check if the bit-flag is set. */
	    if ((fval->value == 0 && flags == 0)
		|| (fval->value && (fval->value & flags) == fval->value))
	      {
		if (string->len)
		  g_string_append_c (string, '|');
		g_string_append (string, fval->value_name);
	      }
	  }
      }

    ret = string->str;
    g_string_free (string, FALSE);

    g_type_class_unref (flags_class);

    return ret;
}

/**
 * gsf_xml_gvalue_from_str :
 * @res : Result value
 * @t : Type of data
 * @str : Value string
 *
 * Try to parse @str as a value of type @t into @res.
 *
 * Returns : True when parsing of @str as a value of type @t was succesfull;
 * false otherwise.
 */
gboolean
gsf_xml_gvalue_from_str (GValue *res, GType t, char const *str)
{
	g_return_val_if_fail (res != NULL, FALSE);
	g_return_val_if_fail (str != NULL, FALSE);

	g_value_init (res, t);
	switch (t) {
	case G_TYPE_CHAR:
		g_value_set_char (res, str[0]);
		break;
	case G_TYPE_UCHAR:
		g_value_set_uchar (res, (guchar)str[0]);
		break;
	case G_TYPE_BOOLEAN:
		g_value_set_boolean (res, 
			g_ascii_tolower (str[0]) == 't' ||
			g_ascii_tolower (str[0]) == 'y' ||
			strtol (str, NULL, 0));
		break;
	case G_TYPE_INT:
		g_value_set_int (res, strtol (str, NULL, 0));
		break;
	case G_TYPE_UINT:
		g_value_set_uint (res, strtoul (str, NULL, 0));
		break;
	case G_TYPE_LONG:
		g_value_set_long (res, strtol (str, NULL, 0));
		break;
	case G_TYPE_ULONG:
		g_value_set_ulong (res, strtoul (str, NULL, 0));
		break; 
	case G_TYPE_ENUM:
		g_value_set_enum (res, glade_enum_from_string (t, str));
		break;
	case G_TYPE_FLAGS:
		g_value_set_flags (res, glade_flags_from_string (t, str));
		break;
	case G_TYPE_FLOAT:
		g_value_set_float (res, g_strtod (str, NULL));
		break;
	case G_TYPE_DOUBLE:
		g_value_set_double (res, g_strtod (str, NULL));
		break;
	case G_TYPE_STRING:
		g_value_set_string (res, str);
		break;

	default:
		if (GSF_TIMESTAMP_TYPE == t) {
			GsfTimestamp ts;
			if (gsf_timestamp_parse (str, &ts)) {
				gsf_value_set_timestamp (res, &ts);
				break;
			}
		}

		return FALSE;
	}
	return TRUE;
}

/* Note: libxml erroneously declares the length argument as int.  */
static int
gsf_libxml_read (void *context, guint8 *buffer, int len)
{
	gsf_off_t remaining = gsf_input_remaining ((GsfInput *)context);
	guint8* res;

	if (len > remaining)
		len = remaining;
	res = (guint8 *) gsf_input_read ((GsfInput *)context,
					 (size_t)len, buffer);
	if (res == NULL && len > 0) /* Not an error if len == 0 */
		return -1;
	return len;
}

static int
gsf_libxml_write (void *context, char const *buffer, int len)
{
	if (!gsf_output_write ((GsfOutput *)context, (size_t)len, buffer))
		return -1;
	return len;
}

static int
gsf_libxml_close (void *context)
{
	g_object_unref (G_OBJECT (context));
	return TRUE;
}

static xmlParserCtxtPtr
gsf_xml_parser_context_full (GsfInput *input, xmlSAXHandlerPtr sax, gpointer user)
{
	GsfInput *gzip;
	xmlParserCtxtPtr res;

	g_return_val_if_fail (GSF_IS_INPUT (input), NULL);

	gzip = gsf_input_gzip_new (input, NULL);
	if (gzip != NULL)
		input = gzip;
	else
		g_object_ref (G_OBJECT (input));

	res = xmlCreateIOParserCtxt (
		sax, user,
		(xmlInputReadCallback) gsf_libxml_read, 
		(xmlInputCloseCallback) gsf_libxml_close,
		input, XML_CHAR_ENCODING_NONE);

	if (res)
		res->replaceEntities = TRUE;
	else
		g_object_unref (input);

	return res;
}

/**
 * gsf_xml_parser_context :
 * @input : #GsfInput
 *
 * Create a libxml2 pull style parser context wrapper around gsf input @input.
 * This signature will probably change to supply a SAX structure.
 *
 * <note>This adds a reference to @input.</note>
 * <note>A simple wrapper around a cleaner implementation that will fold in
 * when we add other api changes.  Its not worth bumping just for this.</note>
 *
 * Returns : A parser context or NULL
 **/
xmlParserCtxtPtr
gsf_xml_parser_context (GsfInput *input)
{
	return gsf_xml_parser_context_full (input, NULL, NULL);
}

/**
 * gsf_xml_output_buffer_new :
 * @output:
 * @encoding: optionally NULL.
 *
 * <note>This adds a reference to @output.</note>
 * <note>This is <emphasis>not</emphasis> releated to #GsfXMLOut.</note>
 */
static xmlOutputBufferPtr
gsf_xml_output_buffer_new (GsfOutput *output,
			   xmlCharEncodingHandlerPtr handler)
{
	xmlOutputBufferPtr res = xmlAllocOutputBuffer (handler);
	if (res != NULL) {
		g_object_ref (G_OBJECT (output));
		res->context = (void *)output;
		res->writecallback = gsf_libxml_write;
		res->closecallback = gsf_libxml_close;
	}

	return res;
}

int
gsf_xmlDocFormatDump (GsfOutput *output, xmlDocPtr cur, char const *encoding,
		      gboolean format)
{
	xmlOutputBufferPtr buf;
	xmlCharEncodingHandlerPtr handler = NULL;

	if (cur == NULL) {
#ifdef DEBUG_TREE
		xmlGenericError(xmlGenericErrorContext,
				"xmlDocDump : document == NULL\n");
#endif
		return(-1);
	}

	if (encoding != NULL) {
		xmlCharEncoding enc;

		enc = xmlParseCharEncoding(encoding);

		if (cur->charset != XML_CHAR_ENCODING_UTF8) {
			xmlGenericError(xmlGenericErrorContext,
					"xmlDocDump: document not in UTF8\n");
			return(-1);
		}
		if (enc != XML_CHAR_ENCODING_UTF8) {
			handler = xmlFindCharEncodingHandler(encoding);
			if (handler == NULL) {
				xmlFree((char *) cur->encoding);
				cur->encoding = NULL;
			}
		}
	}
	buf = gsf_xml_output_buffer_new (output, handler);
	return xmlSaveFormatFileTo (buf, cur, encoding, format);
}

/***************************************************************************/

typedef struct {
	GsfXMLInNode pub;
	/* internal state */
	GSList *groups;
	GSList *extensions;
} GsfXMLInNodeInternal;
struct _GsfXMLInDoc {
	GsfXMLInNodeInternal const *root_node;
	GHashTable      *symbols; /* GsfXMLInNodeInternal hashed by id */
	GsfXMLInNS const*ns;
	GPtrArray	*ns_by_id;
	GsfXMLInUnknownFunc	unknown_handler;
};
typedef struct {
	GsfXMLIn	pub;

	GsfXMLInNS const *default_ns;	   /* optionally NULL */
	GSList	 	 *ns_stack;
	GHashTable	 *ns_prefixes;
	GPtrArray	 *ns_by_id;
	gboolean          initialized;
	gint	  	  unknown_depth; /* handle recursive unknown tags */
	gboolean	  from_unknown_handler;

	GSList	 	 *extension_stack; /* stack of GsfXMLInExtension */
} GsfXMLInInternal;
typedef struct {
	char    *tag;
	unsigned taglen;
	unsigned ref_count;
} GsfXMLInNSInstance;
typedef struct {
	GsfXMLInNS const *ns;
	GSList *elem;
} GsfXMLInNodeGroup;
typedef struct {
	GsfXMLInExtDtor	   dtor;
	gpointer	   state;
	GsfXMLInDoc const *doc;
	gboolean	   from_unknown;
} GsfXMLInExtension;

static char const *
node_name (GsfXMLInNode const *node)
{
	return (node->name != NULL) ? node->name : "{catch all)}";
}

static void
push_child (GsfXMLInInternal *state, GsfXMLInNode const *node, GsfXMLInNS const *default_ns,
	    xmlChar const **attrs, GsfXMLInExtension *ext)
{
	if (node->has_content == GSF_XML_CONTENT &&
	    state->pub.content->len > 0) {
		g_warning ("too lazy to support nested unshared content for now.  We'll add it for 2.0");
	}
	state->pub.node_stack	= g_slist_prepend (state->pub.node_stack,
		(gpointer)state->pub.node);
	state->ns_stack		= g_slist_prepend (state->ns_stack,
		(gpointer)state->default_ns);
	state->pub.node = node;
	state->default_ns = default_ns;

	state->extension_stack	= g_slist_prepend (state->extension_stack, ext);
	if (NULL != ext) {
		GsfXMLInDoc const *old_doc = state->pub.doc;
		state->pub.doc = ext->doc;
		ext->doc = old_doc;
		if (NULL != ext->state) {
			gpointer old_state = state->pub.user_state;
			state->pub.user_state = ext->state;
			ext->state = old_state;
		}
	}
	if (NULL != node->start)
		node->start (&state->pub, attrs);
}

static gboolean
lookup_child (GsfXMLInInternal *state, GsfXMLInNS const *default_ns,
	      GSList *groups, xmlChar const *name,
	      xmlChar const **attrs, GsfXMLInExtension *ext)
{
	GsfXMLInNodeGroup  *group;
	GsfXMLInNode	   *node;
	GsfXMLInNSInstance *inst;
	GSList *elem, *ptr;
	char const *tmp;

	for (ptr = groups ; ptr != NULL ; ptr = ptr->next) {
		group = ptr->data;
		/* does the namespace match */
		if (group->ns != NULL && group->ns != default_ns) {
			g_return_val_if_fail (state->ns_by_id->len > group->ns->ns_id, FALSE);
			inst = g_ptr_array_index (state->ns_by_id, group->ns->ns_id);
			if (inst == NULL || 0 != strncmp (name, inst->tag, inst->taglen))
				continue;
			tmp = name + inst->taglen;
		} else {
#if 0
			g_return_if_fail (state->ns_by_id->len > group->ns->ns_id);
			inst = g_ptr_array_index (state->ns_by_id, group->ns->ns_id);
			g_warning ("accepted ns = '%s' looking for '%s'", inst->tag, name);
#endif
			tmp = name;
		}
		for (elem = group->elem ; elem != NULL ; elem = elem->next) {
			node = elem->data;
			if (node->name == NULL || !strcmp (tmp, node->name)) {
				push_child (state, node, default_ns, attrs, ext);
				return TRUE;
			}
		}
	}
	return FALSE;
}

static void
gsf_xml_in_start_element (GsfXMLInInternal *state, xmlChar const *name, xmlChar const **attrs)
{
	GsfXMLInNSInstance *inst;
	GsfXMLInNS const   *ns, *default_ns = state->default_ns;
	GsfXMLInNodeInternal const *node;
	xmlChar const **ns_ptr;
	GSList *ptr;
	char const *tmp;
	int i;

	/* Scan for namespace declarations.  Yes it is ugly to have the api
	 * flag that its children can declare namespaces. However, given that a
	 * we need to know which namespace we are in before we can recognize
	 * the current node there is no choice.
	 * eg <gnm:Workbook xmlns:gnm="www.gnumeric.org"/> we can not know
	 * that we are in node 'Workbook' without recognizing ns=gnm, which we
	 * would not do unless we checked for a namespace */
	ns = state->pub.doc->ns;
	if (ns != NULL && state->pub.node->check_children_for_ns) {
		for (ns_ptr = attrs; ns_ptr != NULL && ns_ptr[0] && ns_ptr[1] ; ns_ptr += 2) {
			if (strncmp (*ns_ptr, "xmlns", 5))
				continue;
			if (ns_ptr[0][5] != '\0' && ns_ptr[0][5] != ':')
				continue;
			for (i = 0; (tmp = ns[i].uri) != NULL ; i++) {
				if (strcmp (tmp, ns_ptr[1]))
					continue;

				if (ns_ptr[0][5] == '\0') {
					default_ns = ns + i;
					break;
				}

				inst = g_hash_table_lookup (state->ns_prefixes, ns_ptr[0] + 6);
				if (inst == NULL) {
					inst = g_new0 (GsfXMLInNSInstance, 1);
					inst->tag    = g_strconcat (ns_ptr[0] + 6, ":", NULL);
					inst->taglen = strlen (inst->tag);
					inst->ref_count = 1;
					g_hash_table_insert (state->ns_prefixes, g_strdup (ns_ptr[0] + 6), inst);

					if (ns[i].ns_id >= state->ns_by_id->len)
						g_ptr_array_set_size  (state->ns_by_id, ns[i].ns_id+1);
					if (g_ptr_array_index (state->ns_by_id, ns[i].ns_id)) {
						g_warning ("Damn.  Someone just declared the same namespace '%s' with a different prefix '%s'",
							   ns[i].uri, inst->tag);
					} else
						g_ptr_array_index (state->ns_by_id, ns[i].ns_id) = inst;
				} else
					inst->ref_count++;
				break;
			}
		}
	}

	node = (GsfXMLInNodeInternal const *) state->pub.node;
	if (lookup_child (state, default_ns, node->groups, name, attrs, NULL))
		return;

	/* useful for <Data><b><i><u></u></i></b></Data> where all of the markup can nest */
	ptr = state->pub.node_stack;
	for (; ptr != NULL && node->pub.share_children_with_parent; ptr = ptr->next) {
		node = ptr->data;
		if (lookup_child (state, default_ns, node->groups, name, attrs, NULL))
			return;
	}

	/* Check for extensions */
	for (ptr = node->extensions; ptr != NULL ; ptr = ptr->next) {
		GsfXMLInExtension *ext = ptr->data;
		if (lookup_child (state, default_ns,
			ext->doc->root_node->groups, name, attrs, ext))
			return;
	}

	if (state->pub.doc->unknown_handler != NULL) {
		gboolean handled;
		state->from_unknown_handler = TRUE;
		handled = (state->pub.doc->unknown_handler) (&state->pub, name, attrs);
		state->from_unknown_handler = FALSE;
		if (handled)
			return;
	}

	if (state->unknown_depth++ > 0)
		return;
	g_warning ("Unexpected element '%s' in state %s.", name, node_name (state->pub.node));

	ptr = state->pub.node_stack = g_slist_reverse (state->pub.node_stack);
	for (;ptr != NULL && ptr->next != NULL; ptr = ptr->next) {
		node = ptr->data;
		if (node != NULL) {
/* FIXME FIXME FIXME if we really want this do we also want namespaces ? */
			g_print ("%s", node_name (&node->pub));
			if (ptr->next != NULL && ptr->next->data != NULL)
				g_print (" -> ");
		}
	}
	state->pub.node_stack = g_slist_reverse (state->pub.node_stack);
}

static void
gsf_xml_in_ext_free (GsfXMLInInternal *state, GsfXMLInExtension *ext)
{
	if (ext->dtor)
		(ext->dtor) (&state->pub, ext->state);
	g_free (ext);
}

static void
gsf_xml_in_end_element (GsfXMLInInternal *state,
			G_GNUC_UNUSED xmlChar const *name)
{
	GsfXMLInNodeInternal	*node;
	GsfXMLInExtension	*ext;
	GSList *ptr;

	if (state->unknown_depth > 0) {
		state->unknown_depth--;
		return;
	}

	g_return_if_fail (state->pub.node	!= NULL);
	g_return_if_fail (state->pub.node_stack != NULL);
	g_return_if_fail (state->ns_stack != NULL);

	node = (GsfXMLInNodeInternal *) state->pub.node;
	if (node->pub.end)
		node->pub.end (&state->pub, NULL);
	if (node->pub.has_content == GSF_XML_CONTENT)
		g_string_truncate (state->pub.content, 0);

	/* Free any potential extensions associated with the current node */
	for (ptr = node->extensions; ptr != NULL ; ptr = ptr->next)
		gsf_xml_in_ext_free (state, ptr->data);
	g_slist_free (node->extensions);
	node->extensions = NULL;

	/* pop the state stack */
	ext = state->extension_stack->data;
	state->extension_stack	= g_slist_remove (state->extension_stack, ext);
	state->pub.node		= state->pub.node_stack->data;
	state->pub.node_stack	= g_slist_remove (state->pub.node_stack, state->pub.node);
	state->default_ns	= state->ns_stack->data;
	state->ns_stack		= g_slist_remove (state->ns_stack, state->default_ns);

	if (NULL != ext) {
		GsfXMLInDoc const *ext_doc = state->pub.doc;
		state->pub.doc = ext->doc;
		ext->doc = ext_doc;
		if (NULL != ext->state) {
			gpointer ext_state = state->pub.user_state;
			state->pub.user_state = ext->state;
			ext->state = ext_state;
		}
		if (ext->from_unknown)
			gsf_xml_in_ext_free (state, ext);
	}
}

static void
gsf_xml_in_characters (GsfXMLInInternal *state, xmlChar const *chars, int len)
{
	if (state->pub.node->has_content != GSF_XML_NO_CONTENT)
		g_string_append_len (state->pub.content, chars, len);
}

static xmlEntityPtr
gsf_xml_in_get_entity (G_GNUC_UNUSED GsfXMLInInternal *state, xmlChar const *name)
{
	return xmlGetPredefinedEntity (name);
}

static void
gsf_free_xmlinnsinstance (GsfXMLInNSInstance *inst)
{
	g_free (inst->tag);
	g_free (inst);
}

static void
gsf_xml_in_start_document (GsfXMLInInternal *state)
{
	/*
	 * This function will not be called when parsing an empty
	 * document.
	 */
	state->initialized	= TRUE;
	state->pub.node = &state->pub.doc->root_node->pub;
	state->unknown_depth	= 0;
	state->pub.node_stack	= NULL;
	state->extension_stack	= NULL;
	state->ns_stack		= NULL;
	state->default_ns	= NULL;
	state->ns_by_id		= g_ptr_array_new ();
	state->ns_prefixes	= g_hash_table_new_full (
		g_str_hash, g_str_equal,
		g_free, (GDestroyNotify) gsf_free_xmlinnsinstance);
	state->from_unknown_handler = FALSE;
}

static void
gsf_xml_in_end_document (GsfXMLInInternal *state)
{
	g_string_free (state->pub.content, TRUE);
	state->pub.content = NULL;

	if (state->initialized) {
		g_ptr_array_free (state->ns_by_id, TRUE);
		state->ns_by_id = NULL;

		g_hash_table_destroy (state->ns_prefixes);
		state->ns_prefixes = NULL;

		g_return_if_fail (state->pub.node == &state->pub.doc->root_node->pub);
		g_return_if_fail (state->unknown_depth == 0);
	}
}

static void
gsf_xml_in_warning (G_GNUC_UNUSED GsfXMLInInternal *state, char const *msg, ...)
{
	va_list args;

	va_start (args, msg);
	g_logv ("XML", G_LOG_LEVEL_WARNING, msg, args);
	va_end (args);
}

static void
gsf_xml_in_error (G_GNUC_UNUSED GsfXMLInInternal *state, char const *msg, ...)
{
	va_list args;

	va_start (args, msg);
	g_logv ("XML", G_LOG_LEVEL_CRITICAL, msg, args);
	va_end (args);
}

static void
gsf_xml_in_fatal_error (G_GNUC_UNUSED GsfXMLInInternal *state, char const *msg, ...)
{
	va_list args;

	va_start (args, msg);
	g_logv ("XML", G_LOG_LEVEL_ERROR, msg, args);
	va_end (args);
}

static xmlSAXHandler gsfXMLInParser = {
	NULL, /* internalSubset */
	NULL, /* isStandalone */
	NULL, /* hasInternalSubset */
	NULL, /* hasExternalSubset */
	NULL, /* resolveEntity */
	(getEntitySAXFunc)gsf_xml_in_get_entity, /* getEntity */
	NULL, /* entityDecl */
	NULL, /* notationDecl */
	NULL, /* attributeDecl */
	NULL, /* elementDecl */
	NULL, /* unparsedEntityDecl */
	NULL, /* setDocumentLocator */
	(startDocumentSAXFunc)gsf_xml_in_start_document, /* startDocument */
	(endDocumentSAXFunc)gsf_xml_in_end_document, /* endDocument */
	(startElementSAXFunc)gsf_xml_in_start_element, /* startElement */
	(endElementSAXFunc)gsf_xml_in_end_element, /* endElement */
	NULL, /* reference */
	(charactersSAXFunc)gsf_xml_in_characters, /* characters */
	NULL, /* ignorableWhitespace */
	NULL, /* processingInstruction */
	NULL, /* comment */
	(warningSAXFunc)gsf_xml_in_warning, /* warning */
	(errorSAXFunc)gsf_xml_in_error, /* error */
	(fatalErrorSAXFunc)gsf_xml_in_fatal_error, /* fatalError */
	NULL, /* getParameterEntity */
	NULL, /* cdataBlock */
	NULL, /* externalSubset */
	0
#if LIBXML_VERSION >= 20600
	,
	NULL, NULL, NULL
#if LIBXML_VERSION >= 20602
	,
	NULL /* serror */
#endif
#endif
};

static void
gsf_xml_in_node_internal_free (GsfXMLInNodeInternal *node)
{
	GSList *ptr;
	GsfXMLInNodeGroup *group;

	if (NULL != node->extensions) {
		g_warning ("leaking extensions");
	}

	for (ptr = node->groups; ptr != NULL ; ptr = ptr->next) {
		group = ptr->data;
		g_slist_free (group->elem);
		g_free (group);
	}
	g_slist_free (node->groups);
	node->groups = NULL;
	g_free (node);
}

/**
 * gsf_xml_in_doc_free :
 * @doc :
 *
 * Free up resources
 **/
void
gsf_xml_in_doc_free (GsfXMLInDoc *doc)
{
	g_return_if_fail (doc != NULL);
	g_return_if_fail (doc->symbols != NULL);
	g_return_if_fail (doc->ns_by_id != NULL);

	g_hash_table_destroy (doc->symbols);
	g_ptr_array_free (doc->ns_by_id, TRUE);
	/* poison the well just in case */
	doc->symbols   = NULL;
	doc->ns_by_id  = NULL;
	doc->root_node = NULL;
	g_free (doc);
}

/**
 * gsf_xml_in_doc_new :
 * @nodes : an array of node descriptors
 * @ns : an array of namespace identifiers
 *
 * Combine the nodes in the NULL terminated array starting at @nodes with the
 * name spaces in the NULL terminated array starting at @ns.  Prepare the
 * data structures necessary to validate a doument based on that description.
 *
 * Returns NULL on error
 **/
GsfXMLInDoc *
gsf_xml_in_doc_new (GsfXMLInNode const *nodes, GsfXMLInNS const *ns)
{
	GsfXMLInDoc  *doc;
	GsfXMLInNode const *e_node;
	GsfXMLInNodeInternal *tmp, *node;
	unsigned i;

	g_return_val_if_fail (nodes != NULL, NULL);

	doc = g_new0 (GsfXMLInDoc, 1);
	doc->root_node = NULL;
	doc->symbols   = g_hash_table_new_full (g_str_hash, g_str_equal,
		NULL, (GDestroyNotify) gsf_xml_in_node_internal_free);
	doc->ns        = ns;
	doc->ns_by_id  = g_ptr_array_new ();

	/* Add namespaces to an idex */
	if (ns != NULL)
		for (i = 0; ns[i].uri != NULL ; i++) {
			if (ns[i].ns_id >= doc->ns_by_id->len)
				g_ptr_array_set_size  (doc->ns_by_id, ns[i].ns_id+1);
			g_ptr_array_index (doc->ns_by_id, ns[i].ns_id) = (gpointer)(ns+i);
		}

	for (e_node = nodes; e_node->id != NULL ; e_node++ ) {
		node = g_hash_table_lookup (doc->symbols, e_node->id);
		if (node != NULL) {
			/* if its empty then this is just a recusion */
			if (e_node->start != NULL || e_node->end != NULL ||
			    e_node->has_content != GSF_XML_NO_CONTENT ||
			    e_node->user_data.v_int != 0) {
				g_warning ("ID '%s' has already been registered.\n"
					   "The additional decls should not specify start,end,content,data", e_node->id);
				continue;
			}
		} else {
			node = g_new0 (GsfXMLInNodeInternal, 1);
			node->pub = *e_node;
			/* WARNING VILE HACK :
			 * The api in 1.8.2 passed has_content as a boolean.
			 * Too many things still use that to change yet.  We
			 * edit the bool here to be GSF_CONTENT_NONE or
			 * GSF_XML_CONTENT and try to ignore SHARED_CONTENT */
			if (node->pub.has_content != 0 &&
			    node->pub.has_content != GSF_XML_SHARED_CONTENT)
				node->pub.has_content = GSF_XML_CONTENT;
			node->groups = NULL;
			g_hash_table_insert (doc->symbols,
				(gpointer)node->pub.id, node);
		}
		if (e_node == nodes) /* first valid node is the root */
			doc->root_node = node;

		tmp = g_hash_table_lookup (doc->symbols, node->pub.parent_id);
		if (tmp != NULL) {
			GSList *ptr;
			GsfXMLInNodeGroup *group = NULL;
			GsfXMLInNS const *ns = NULL;
			
			ns = (node->pub.ns_id < 0) ? NULL
				: g_ptr_array_index (doc->ns_by_id, node->pub.ns_id);
			for (ptr = tmp->groups; ptr != NULL ; ptr = ptr->next) {
				group = ptr->data;
				if (group->ns == ns)
					break;
			}
			if (ptr == NULL) {
				group = g_new0 (GsfXMLInNodeGroup, 1);
				group->ns = ns;
				tmp->groups = g_slist_prepend (tmp->groups, group);
			}
			group->elem = g_slist_prepend (group->elem, node);
		} else if (strcmp (node->pub.id, node->pub.parent_id)) {
			g_warning ("Parent ID '%s' unknown", node->pub.parent_id);
			continue;
		}
	}

	return doc;
}

/**
 * gsf_xml_in_doc_set_unknown_handler:
 * @doc : #GsfXMLInDoc
 * @handler : The function to call
 *
 * Call the function @handler when an unexpected child node is found
 **/
void
gsf_xml_in_doc_set_unknown_handler (GsfXMLInDoc *doc,
				    GsfXMLInUnknownFunc handler)
{
	g_return_if_fail (doc != NULL);
	doc->unknown_handler = handler;
}

/**
 * gsf_xml_in_push_state :
 * @xin : #GsfXMLIn
 * @doc : #GsfXMLInDoc
 * @new_state :
 * @dtor : #GsfXMLInExtDtor
 * @attrs :
 *
 * Take the first node from @doc as the current node and call it's start handler.
 **/
void
gsf_xml_in_push_state (GsfXMLIn *xin, GsfXMLInDoc const *doc,
		       gpointer new_state, GsfXMLInExtDtor dtor,
		       xmlChar const **attrs)
{
	GsfXMLInInternal *state = (GsfXMLInInternal *)xin;
	GsfXMLInExtension *ext;
	
	g_return_if_fail (xin != NULL);
	g_return_if_fail (doc != NULL);
	g_return_if_fail (doc->root_node != NULL);

	ext = g_new (GsfXMLInExtension, 1);
	ext->doc	  = doc;
	ext->state	  = new_state;
	ext->dtor	  = dtor;
	if (!(ext->from_unknown = state->from_unknown_handler)) {
		GsfXMLInNodeInternal *node = (GsfXMLInNodeInternal *) xin->node;
		node->extensions = g_slist_prepend (node->extensions, ext);
	} else
		push_child (state, &doc->root_node->pub, NULL, attrs, ext);
}

/**
 * gsf_xml_in_doc_parse :
 * @doc :
 * @input :
 * @user_state :
 *
 * Read an xml document from @input and parse based on the the descriptor in
 * @doc
 *
 * returns FALSE on error
 **/
gboolean
gsf_xml_in_doc_parse (GsfXMLInDoc *doc, GsfInput *input, gpointer user_state)
{
	xmlParserCtxt	*ctxt;
	GsfXMLInInternal state;
	gboolean res;

	g_return_val_if_fail (doc != NULL, FALSE);

	state.initialized = FALSE;
	ctxt = gsf_xml_parser_context_full (input, &gsfXMLInParser, &state);
	if (ctxt == NULL)
		return FALSE;

	state.pub.doc = doc;
	state.pub.user_state = user_state;
	state.pub.content = g_string_sized_new (128);
	xmlParseDocument (ctxt);
	res = ctxt->wellFormed;
	xmlFreeParserCtxt (ctxt);

	return res;
}

/**
 * gsf_xml_in_check_ns :
 * @xin :
 * @str :
 * @ns_id :
 * 
 * According to @state is @str in the namespace @ns_id ?
 *
 * Returns a pointer to @str after the namespace if successful,
 * otherwise NULL.
 **/
char const *
gsf_xml_in_check_ns (GsfXMLIn const *xin, char const *str, unsigned int ns_id)
{
	GsfXMLInInternal const *state = (GsfXMLInInternal const *)xin;
	GsfXMLInNSInstance *inst;
	if (state->ns_by_id->len <= ns_id)
		return NULL;
	if (NULL == (inst = g_ptr_array_index (state->ns_by_id, ns_id)))
		return NULL;
	if (strncmp (str, inst->tag, inst->taglen))
		return NULL;
	return str + inst->taglen;
}

/**
 * gsf_xml_in_namecmp :
 * @xin   : The #GsfXMLIn we are reading from.
 * @str   : The potentially namespace qualified node name.
 * @ns_id : The name space id to check
 * @name  : The target node name
 *
 * Returns TRUE if @str == @ns_id:@name according to @state.
 **/
gboolean
gsf_xml_in_namecmp (GsfXMLIn const *xin, char const *str,
		    unsigned int ns_id, char const *name)
{
	GsfXMLInInternal const *state = (GsfXMLInInternal const *)xin;
	GsfXMLInNSInstance *inst;

	if (state->ns_by_id->len <= ns_id)
		return FALSE;
	if (NULL == (inst = g_ptr_array_index (state->ns_by_id, ns_id)))
		return FALSE;
	if (strncmp (str, inst->tag, inst->taglen))
		return FALSE;
	return 0 == strcmp (name, str + inst->taglen);
}

/****************************************************************************/

typedef enum {
	GSF_XML_OUT_NOCONTENT,
	GSF_XML_OUT_CHILD,
	GSF_XML_OUT_CONTENT
} GsfXMLOutState;

struct _GsfXMLOut {
	GObject	   base;

	GsfOutput	 *output;
	char		 *doc_type;
	GSList		 *stack;
	GsfXMLOutState	  state;
	unsigned   	  indent;
	gboolean	  needs_header;
};

typedef struct {
	GObjectClass  base;
} GsfXMLOutClass;

static void
gsf_xml_out_finalize (GObject *obj)
{
	GsfXMLOut *xml = GSF_XML_OUT (obj);

	g_free (xml->doc_type);

	parent_class->finalize (obj);
}

static void
gsf_xml_out_init (GObject *obj)
{
	GsfXMLOut *xml = GSF_XML_OUT (obj);
	xml->output = NULL;
	xml->stack  = NULL;
	xml->state  = GSF_XML_OUT_CHILD;
	xml->indent = 0;
	xml->needs_header = TRUE;
	xml->doc_type = NULL;
}

static void
gsf_xml_out_class_init (GObjectClass *gobject_class)
{
	gobject_class->finalize = gsf_xml_out_finalize;
	parent_class = g_type_class_peek_parent (gobject_class);
}

GSF_CLASS (GsfXMLOut, gsf_xml_out,
	   gsf_xml_out_class_init, gsf_xml_out_init,
	   G_TYPE_OBJECT)

/**
 * gsf_xml_out_new :
 * @output : #GsfOutput
 *
 * Create an XML output stream.
 *
 * Returns: #GsfXMLOut
 */
GsfXMLOut *
gsf_xml_out_new (GsfOutput *output)
{
	GsfXMLOut *xml = g_object_new (GSF_XML_OUT_TYPE, NULL);
	if (!gsf_output_wrap (G_OBJECT (xml), output))
		return NULL;
	xml->output = output;
	return xml;
}

/**
 * gsf_xml_out_set_doc_type :
 * @xml : #GsfXMLOut
 * @type : the document type declaration
 *
 * Store some optional some &lt;!DOCTYPE .. &gt; content
 **/
void
gsf_xml_out_set_doc_type (GsfXMLOut *xml, char const *type)
{
	g_free (xml->doc_type);
	xml->doc_type = g_strdup (type);
}

static inline void
gsf_xml_out_indent (GsfXMLOut *xml)
{
	static char const spaces [] =
		"                                        "
		"                                        "
		"                                        "
		"                                        "
		"                                        "
		"                                        ";
	unsigned i;
	for (i = xml->indent ; i > (sizeof (spaces)/2) ; i -= sizeof (spaces)/2)
		gsf_output_write (xml->output, sizeof (spaces) - 1, spaces);
	gsf_output_write (xml->output, i*2, spaces);
}

/**
 * gsf_xml_out_start_element :
 * @xml : #GsfXMLOut
 * @id  : Element name
 *
 * Output a start element @id, if necessary preceeded by an XML declaration.
 */
void
gsf_xml_out_start_element (GsfXMLOut *xml, char const *id)
{
	g_return_if_fail (id != NULL);
	g_return_if_fail (xml != NULL);
	g_return_if_fail (xml->state != GSF_XML_OUT_CONTENT);

	if (xml->needs_header) {
		static char const header0[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
		gsf_output_write (xml->output, sizeof (header0) - 1, header0);
		if (xml->doc_type != NULL)
			gsf_output_puts (xml->output, xml->doc_type);
		xml->needs_header = FALSE;
	}
	if (xml->state == GSF_XML_OUT_NOCONTENT)
		gsf_output_write (xml->output, 2, ">\n");

	gsf_xml_out_indent (xml);
	gsf_output_printf (xml->output, "<%s", id);

	xml->stack = g_slist_prepend (xml->stack, (gpointer)id);
	xml->indent++;
	xml->state = GSF_XML_OUT_NOCONTENT;
}

/**
 * gsf_xml_out_end_element :
 * @xml : #GsfXMLOut
 *
 * Closes/ends an XML element.
 *
 * Returns: the element that has been closed.
 */
char const *
gsf_xml_out_end_element (GsfXMLOut *xml)
{
	char const *id;

	g_return_val_if_fail (xml != NULL, NULL);
	g_return_val_if_fail (xml->stack != NULL, NULL);

	id = xml->stack->data;
	xml->stack = g_slist_remove (xml->stack, id);
	xml->indent--;
	switch (xml->state) {
	case GSF_XML_OUT_NOCONTENT :
		gsf_output_write (xml->output, 3, "/>\n");
		break;

	case GSF_XML_OUT_CHILD :
		gsf_xml_out_indent (xml);
	/* fall through */
	case GSF_XML_OUT_CONTENT :
		gsf_output_printf (xml->output, "</%s>\n", id);
	}
	xml->state = GSF_XML_OUT_CHILD;
	return id;
}

/**
 * gsf_xml_out_simple_element :
 * @xml : #GsfXMLOut
 * @id  : Element name
 * @content : Content of the element
 *
 * Convenience routine to output a simple @id element with content @content.
 **/
void
gsf_xml_out_simple_element (GsfXMLOut *xml, char const *id,
			    char const *content)
{
	gsf_xml_out_start_element (xml, id);
	if (content != NULL)
		gsf_xml_out_add_cstr (xml, NULL, content);
	gsf_xml_out_end_element (xml);
}

/**
 * gsf_xml_out_simple_int_element :
 * @xml : #GsfXMLOut
 * @id  : Element name
 * @val : Element value
 *
 * Convenience routine to output an element @id with integer value @val.
 **/
void
gsf_xml_out_simple_int_element (GsfXMLOut *xml, char const *id, int val)
{
	gsf_xml_out_start_element (xml, id);
	gsf_xml_out_add_int (xml, NULL, val);
	gsf_xml_out_end_element (xml);
}

/**
 * gsf_xml_out_simple_float_element :
 * @xml : #GsfXMLOut
 * @id  : Element name
 * @val : Element value
 * @precision : the number of significant digits to use, -1 meaning "enough".
 *
 * Convenience routine to output an element @id with float value @val using
 * @precision significant digits.
 **/
void
gsf_xml_out_simple_float_element (GsfXMLOut *xml, char const *id,
				  double val, int precision)
{
	gsf_xml_out_start_element (xml, id);
	gsf_xml_out_add_float (xml, NULL, val, precision);
	gsf_xml_out_end_element (xml);
}

static void
close_tag_if_neccessary (GsfXMLOut* xml)
{
	if (xml->state != GSF_XML_OUT_CONTENT) {
		xml->state = GSF_XML_OUT_CONTENT;
		gsf_output_write (xml->output, 1, ">");
	}
}

/**
 * gsf_xml_out_add_cstr_unchecked :
 * @xml : #GsfXMLOut
 * @id : optionally NULL for content
 * @val_utf8 : a utf8 encoded string to export
 *
 * dump @val_utf8 to an attribute named @id without checking to see if the
 * content needs escaping.  A useful performance enhancement when the
 * application knows that structure of the content well.  If @val_utf8 is NULL
 * do nothing (no warning, no output)
 **/
void
gsf_xml_out_add_cstr_unchecked (GsfXMLOut *xml, char const *id,
				char const *val_utf8)
{
	g_return_if_fail (xml != NULL);

	if (val_utf8 == NULL)
		return;

	if (id == NULL) {
		close_tag_if_neccessary (xml);
		gsf_output_write (xml->output, strlen (val_utf8), val_utf8);
	} else
		gsf_output_printf (xml->output, " %s=\"%s\"", id, val_utf8);
}

/**
 * gsf_xml_out_add_cstr :
 * @xml : #GsfXMLOut
 * @id : optionally NULL for content
 * @val_utf8 : a utf8 encoded string
 *
 * dump @val_utf8 to an attribute named @id or as the nodes content escaping
 * characters as necessary.  If @val_utf8 is NULL do nothing (no warning, no
 * output)
 **/
void
gsf_xml_out_add_cstr (GsfXMLOut *xml, char const *id,
		      char const *val_utf8)
{
	guint8 const *cur   = val_utf8;
	guint8 const *start = val_utf8;

	g_return_if_fail (xml != NULL);

	if (val_utf8 == NULL)
		return;

	if (id == NULL) {
		close_tag_if_neccessary (xml);
	} else
		gsf_output_printf (xml->output, " %s=\"", id);
	while (*cur != '\0') {
		if (*cur == '<') {
			if (cur != start)
				gsf_output_write (xml->output, cur-start, start);
			start = ++cur;
			gsf_output_write (xml->output, 4, "&lt;");
		} else if (*cur == '>') {
			if (cur != start)
				gsf_output_write (xml->output, cur-start, start);
			start = ++cur;
			gsf_output_write (xml->output, 4, "&gt;");
		} else if (*cur == '&') {
			if (cur != start)
				gsf_output_write (xml->output, cur-start, start);
			start = ++cur;
			gsf_output_write (xml->output, 5, "&amp;");
		} else if (*cur == '"') {
			if (cur != start)
				gsf_output_write (xml->output, cur-start, start);
			start = ++cur;
			gsf_output_write (xml->output, 6, "&quot;");
		} else if (*cur < 0x20 && id != NULL) {
			guint8 buf[8];
			sprintf (buf, "&#%d;", *cur);

			if (cur != start)
				gsf_output_write (xml->output, cur-start, start);
			start = ++cur;

			gsf_output_write (xml->output, strlen (buf), buf);
		} else if (((*cur >= 0x20) && (*cur != 0x7f)) ||
			   (*cur == '\n') || (*cur == '\r') || (*cur == '\t')) {
			cur++;
		} else {
			g_warning ("Unknown char 0x%hhx in string", *cur);
			cur++;
		}
	}
	if (cur != start)
		gsf_output_write (xml->output, cur-start, start);
	if (id != NULL)
		gsf_output_write (xml->output, 1, "\"");
}

/**
 * gsf_xml_out_add_bool :
 * @xml : #GsfXMLOut
 * @id  : optionally NULL for content
 * @val : a boolean
 *
 * dump boolean value @val to an attribute named @id or as the nodes content
 * Use '1' or '0' to simplify import
 **/
void
gsf_xml_out_add_bool (GsfXMLOut *xml, char const *id,
		      gboolean val)
{
	gsf_xml_out_add_cstr_unchecked (xml, id,
					val ? "1" : "0");
}

/**
 * gsf_xml_out_add_int :
 * @xml : #GsfXMLOut
 * @id  : optionally NULL for content
 * @val : the value
 *
 * dump integer value @val to an attribute named @id or as the nodes content
 **/
void
gsf_xml_out_add_int (GsfXMLOut *xml, char const *id,
		     int val)
{
	char buf [4 * sizeof (int)];
	sprintf (buf, "%d", val);
	gsf_xml_out_add_cstr_unchecked (xml, id, buf);
}

/**
 * gsf_xml_out_add_uint :
 * @xml : #GsfXMLOut
 * @id  : optionally NULL for content
 * @val : the value
 *
 * dump unsigned integer value @val to an attribute named @id or as the nodes
 * content
 **/
void
gsf_xml_out_add_uint (GsfXMLOut *xml, char const *id,
		      unsigned int val)
{
	char buf [4 * sizeof (unsigned int)];
	sprintf (buf, "%u", val);
	gsf_xml_out_add_cstr_unchecked (xml, id, buf);
}

/**
 * gsf_xml_out_add_float :
 * @xml : #GsfXMLOut
 * @id  : optionally NULL for content
 * @val : the value
 * @precision : the number of significant digits to use, -1 meaning "enough".
 *
 * dump float value @val to an attribute named @id or as the nodes
 * content with precision @precision.  The number will be formattted
 * according to the "C" locale.
 **/
void
gsf_xml_out_add_float (GsfXMLOut *xml, char const *id,
		       double val, int precision)
{
	char format_str[4 * sizeof (int) + 10];
	char buf[G_ASCII_DTOSTR_BUF_SIZE + DBL_DIG];

	if (precision < 0 || precision > DBL_DIG)
		precision = DBL_DIG;

	sprintf (format_str, "%%.%dg", precision);
	g_ascii_formatd (buf, sizeof (buf), format_str, val);
	gsf_xml_out_add_cstr_unchecked (xml, id, buf);
}

/**
 * gsf_xml_out_add_color :
 * @xml : #GsfXMLOut
 * @id  : optionally NULL for content
 * @r : Red value
 * @g : Green value
 * @b : Blue value
 *
 * dump Color @r.@g.@b to an attribute named @id or as the nodes content
 **/
void
gsf_xml_out_add_color (GsfXMLOut *xml, char const *id,
		       unsigned int r, unsigned int g, unsigned int b)
{
	char buf [3 * 4 * sizeof (unsigned int) + 1];
	sprintf (buf, "%X:%X:%X", r, g, b);
	gsf_xml_out_add_cstr_unchecked (xml, id, buf);
}

/**
 * gsf_xml_out_add_enum :
 * @xml : #GsfXMLOut
 * @id  : optionally NULL for content
 * @etype : #GType
 * @val : enum element number
 *
 * Output the name of value @val of enumeration type @etype.
 **/
void
gsf_xml_out_add_enum (GsfXMLOut *xml, char const *id, GType etype, gint val)
{
	GEnumClass *eclass = G_ENUM_CLASS (g_type_class_peek (etype));
	GEnumValue *ev = g_enum_get_value (eclass, val);

	if (ev)
		gsf_xml_out_add_cstr_unchecked (xml, id, ev->value_name);
	else
		g_warning ("Invalid value %d for type %s",
			   val, g_type_name (etype));
}

/**
 * gsf_xml_out_add_gvalue :
 * @xml : #GsfXMLOut
 * @id  : optionally NULL for content
 * @val : #GValue
 *
 * Output the value of @val as a string.  Does NOT store any type information
 * with the string, just thevalue.
 **/
void
gsf_xml_out_add_gvalue (GsfXMLOut *xml, char const *id, GValue const *val)
{
	GType t;
	g_return_if_fail (xml != NULL);
	g_return_if_fail (val != NULL);

	t = G_VALUE_TYPE (val);
	switch (t) {
	case G_TYPE_CHAR: {
		char c[2] = { 0, 0 };
		c[0] = g_value_get_char (val);
		gsf_xml_out_add_cstr (xml, id, c);
		break;
	}

	case G_TYPE_UCHAR: {
		unsigned char c[2] = { 0, 0 };
		c[0] = g_value_get_uchar (val);
		gsf_xml_out_add_cstr (xml, id, c);
		break;
	}

	case G_TYPE_BOOLEAN:
		gsf_xml_out_add_cstr (xml, id,
			g_value_get_boolean (val) ? "t" : "f");
		break;
	case G_TYPE_INT:
		gsf_xml_out_add_int (xml, id, g_value_get_int (val));
		break;
	case G_TYPE_UINT:
		gsf_xml_out_add_uint (xml, id, g_value_get_uint (val));
		break;
	case G_TYPE_LONG:
		gsf_xml_out_add_uint (xml, id, g_value_get_long (val));
		break;
	case G_TYPE_ULONG:
		gsf_xml_out_add_uint (xml, id, g_value_get_ulong (val));
		break; 
	case G_TYPE_ENUM:
		gsf_xml_out_add_cstr (xml, id,
			glade_string_from_enum (t, g_value_get_enum (val)));
		break;
	case G_TYPE_FLAGS:
		gsf_xml_out_add_cstr (xml, id,
			glade_string_from_flags (t, g_value_get_flags (val)));
		break;
	case G_TYPE_FLOAT:
		gsf_xml_out_add_float (xml, id, g_value_get_float (val), -1);
		break;
	case G_TYPE_DOUBLE:
		gsf_xml_out_add_float (xml, id, g_value_get_double (val), -1);
		break;
	case G_TYPE_STRING:
		gsf_xml_out_add_cstr (xml, id, g_value_get_string (val));
		break;

	default:
		if (GSF_TIMESTAMP_TYPE == t) {
			char *str = gsf_timestamp_as_string (
				(GsfTimestamp const *)g_value_get_boxed (val));
			gsf_xml_out_add_cstr (xml, id, str);
			break;
		}
	}

	/* FIXME FIXME FIXME Add some error checking */
}

/**
 * gsf_xml_out_add_base64 :
 * @xml : #GsfXMLOut
 * @id  : optionally NULL for content
 * @data : Data to be written
 * @len : Length of data
 *
 * dump @len bytes in @data into the content of node @id using base64
 **/
void
gsf_xml_out_add_base64 (GsfXMLOut *xml, char const *id,
			guint8 const *data, unsigned int len)
{
	/* We could optimize and stream right to the output,
	 * or even just keep the buffer around
	 * for now we start simple */
	guint8 *tmp = gsf_base64_encode_simple (data, len);
	if (tmp == NULL)
		return;
	if (id != NULL)
		g_warning ("Stream a binary blob into an attribute ??");
	gsf_xml_out_add_cstr_unchecked (xml, id, tmp);
	g_free (tmp);
}

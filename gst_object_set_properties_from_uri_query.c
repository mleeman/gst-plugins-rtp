/* 
 * See: https://bugzilla.gnome.org/show_bug.cgi?id=779765
 */

#include "gst_object_set_properties_from_uri_query.h"

static gboolean
_gst_uri_query_to_boolean (const gchar * value)
{
  gchar *down;
  gboolean ret = FALSE;

  g_return_val_if_fail (value != NULL, FALSE);

  down = g_ascii_strdown (value, -1);
  if (g_strcmp0 (down, "true") == 0 ||
      g_strcmp0 (down, "1") == 0 || g_strcmp0 (down, "on") == 0) {
    ret = TRUE;
  }
  g_free (down);
  return ret;
}

void
gst_object_set_properties_from_uri_query(GObject * obj,
    const GstUri * uri)
{
  GHashTable *hash_table = gst_uri_get_query_table (uri);
  GList *keys = NULL, *key;

  if (hash_table) {
    keys = g_hash_table_get_keys (hash_table);

    for (key = keys; key; key = key->next) {
      GParamSpec *spec;
      spec = g_object_class_find_property (G_OBJECT_GET_CLASS (obj), key->data);
      if (spec) {
        switch (spec->value_type) {
          case G_TYPE_BOOLEAN:
            g_object_set (obj, key->data,
                _gst_uri_query_to_boolean (g_hash_table_lookup (hash_table,
                        key->data)), NULL);
            break;
          case G_TYPE_DOUBLE:
            g_object_set (obj, key->data,
                (gdouble) g_ascii_strtoll (
                    (gchar *) g_hash_table_lookup (hash_table, key->data),
                    NULL, 0), NULL);
            break;
          case G_TYPE_INT:
            g_object_set (obj, key->data,
                (gint) g_ascii_strtoll (
                    (gchar *) g_hash_table_lookup (hash_table, key->data),
                    NULL, 0), NULL);
            break;
          case G_TYPE_UINT:
            g_object_set (obj, key->data,
                (guint) g_ascii_strtoull (
                    (gchar *) g_hash_table_lookup (hash_table, key->data),
                    NULL, 0), NULL);
            break;
          case G_TYPE_INT64:
            g_object_set (obj, key->data,
                g_ascii_strtoll (
                    (gchar *) g_hash_table_lookup (hash_table, key->data),
                    NULL, 0), NULL);
            break;
          case G_TYPE_UINT64:
            g_object_set (obj, key->data,
                g_ascii_strtoull (
                    (gchar *) g_hash_table_lookup (hash_table, key->data),
                    NULL, 0), NULL);
            break;
          case G_TYPE_STRING:
            g_object_set (obj, key->data,
                (gchar *) g_hash_table_lookup (hash_table, key->data), NULL);
            break;
          default:
            /* Not fundamental types or unknown */
            if (spec->value_type == GST_TYPE_CAPS) {
              GstCaps *caps = gst_caps_from_string (
                  (gchar *) g_hash_table_lookup (hash_table, key->data));
              g_object_set (obj, key->data, caps, NULL);
              gst_caps_unref (caps);
            } else if (spec->value_type == GST_TYPE_FRACTION) {
              /* In the case of a fraction, the notation is
               * numerator/denominator (%u/%u). We are not yet aware of a
               * helper function that gets us the values, so we scan then
               * with normal string operations. */
              const gchar *s = g_hash_table_lookup (hash_table, key->data);
              gchar **arr = g_strsplit (s, "/", 0);
              if (g_strv_length (arr) == 2) {
                g_object_set (obj, key->data,
                    g_ascii_strtoull (arr[0], NULL, 0),
                    g_ascii_strtoull (arr[1], NULL, 0), NULL);
              }
              g_strfreev (arr);
            } else {
              GST_WARNING ("Unknown type or not yet supported: %s "
                  "(Maybe it should be added)", g_type_name (spec->value_type));
              continue;
            }
            break;
        }
        GST_LOG ("Set property %s: %s", (char *) key->data,
            (gchar *) g_hash_table_lookup (hash_table, key->data));
      } else
        GST_LOG ("Property %s not supported", (char *) key->data);
    }

    g_list_free (keys);
    g_hash_table_unref (hash_table);
  }
}


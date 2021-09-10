#include "jest_file_helpers.h"
#include <gio/gio.h>

bool jest_edit_file(const char *filename)
{
    const gchar *mimetype = "text/plain";

    GAppInfo *appinfo = g_app_info_get_default_for_type(mimetype, FALSE);
    if (!appinfo)
        return false;

    GList *files = nullptr;
    GFile *file = g_file_new_for_path(filename);
    files = g_list_append(files, file);
    gboolean success = g_app_info_launch(appinfo, files, nullptr, nullptr);
    g_object_unref(file);
    g_list_free(files);
    g_object_unref(appinfo);
    return success == TRUE;
}

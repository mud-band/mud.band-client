//
//  mudband_ui_bandadmin.m
//  Mud.band
//
//  Created by Weongyo Jeong on 3/2/25.
//

#import <Foundation/Foundation.h>

#include "jansson.h"
#include "odr.h"
#include "vtc_log.h"
#include "vuuid.h"

#include "libwireguard.h"
#include "mudband_ui.h"

static struct vtclog *bandadmin_vl;

NSString *
mudband_ui_bandadmin_get(void)
{
    NSString *str;
    json_t *root;
    char filepath[ODR_BUFSIZ], *body;
    const char *default_band_uuid;
    
    default_band_uuid = mudband_ui_progconf_get_default_band_uuid();
    if (default_band_uuid == NULL)
        return (NULL);
    snprintf(filepath, sizeof(filepath), "%s/admin_%s.json",
             band_admin_dir, default_band_uuid);
    root = json_load_file(filepath, 0, NULL);
    if (root == NULL) {
        vtc_log(bandadmin_vl, 0, "BANDEC_00842: Failed to load band admin file.");
        return (NULL);
    }
    body = json_dumps(root, JSON_INDENT(2));
    AN(body);
    str = [NSString stringWithUTF8String:body];
    free(body);
    json_decref(root);
    return (str);
}

int
mudband_ui_bandadmin_save(NSString *ns_band_uuid, NSString *ns_jwt)
{
    char filepath[ODR_BUFSIZ];
    json_t *root;
    const char *band_uuid = [ns_band_uuid UTF8String];
    const char *jwt = [ns_jwt UTF8String];
    
    snprintf(filepath, sizeof(filepath), "%s/admin_%s.json",
             band_admin_dir, band_uuid);
    root = json_object();
    AN(root);
    json_object_set_new(root, "band_uuid", json_string(band_uuid));
    json_object_set_new(root, "jwt", json_string(jwt));
    json_dump_file(root, filepath, 0);
    json_decref(root);
    return (0);
}

int
mudband_ui_bandadmin_init(void)
{
    
    bandadmin_vl = vtc_logopen("bandadmin", NULL);
    AN(bandadmin_vl);
    return (0);
}

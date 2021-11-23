#include "migration/fingerprint.h"

Fingerprint *fingerprint_parse(char const *buffer, Error **errp)
{
    QObject *object = NULL;
    QDict *dict = NULL;
    Fingerprint *fingerprint;

    fingerprint = g_malloc0(sizeof(Fingerprint));

    object = qobject_from_json(buffer, errp);
    if (object == NULL) {
        goto error;
    }

    dict = qobject_to(QDict, object);
    if (dict == NULL) {
        goto error;
    }

    /* TODO handle missing invalid fields */

    fingerprint->uuid = qdict_get_str(dict, "uuid");
    fingerprint->migration_type = qdict_get_str(dict, "migration_type");

    dict = qdict_get_qdict(dict, "fingerprints");
    if (dict == NULL) {
        goto error;
    }

    dict = qdict_get_qdict(dict, "memory");
    if (dict == NULL) {
        goto error;
    }

    fingerprint->memory.algorithm = qdict_get_str(dict, "algorithm");
    fingerprint->memory.hash = qdict_get_str(dict, "hash");

    dict = qdict_get_qdict(dict, "disk");
    if (dict != NULL) {
        fingerprint->disk.algorithm = qdict_get_str(dict, "algorithm");
        fingerprint->disk.hash = qdict_get_str(dict, "hash");
    }

    return fingerprint;

error:
    g_free(fingerprint);
    return NULL;
}

struct QJSON *fingerprint_to_json(Fingerprint const *fingerprint)
{
    QJSON *json;

    json = qjson_new();

    json_prop_str(json, "uuid", fingerprint->uuid);
    json_prop_str(json, "migration_type", fingerprint->migration_type);

    json_start_object(json, "fingerprints");

    json_start_object(json, "memory");
    json_prop_str(json, "algorithm", fingerprint->memory.algorithm);
    json_prop_str(json, "hash", fingerprint->memory.hash);
    json_end_object(json);

    if (fingerprint->disk.hash != NULL) {
        json_start_object(json, "disk");
        json_prop_str(json, "algorithm", fingerprint->disk.algorithm);
        json_prop_str(json, "hash", fingerprint->disk.hash);
        json_end_object(json);
    }

    json_end_object(json);

    qjson_finish(json);

    return json;
}

static inline int storage_is_equal(Storage const *s1, Storage const *s2)
{
    return strcmp(s1->algorithm, s2->algorithm) == 0 &&
       strcmp(s1->hash, s2->hash) == 0;
}

bool fingerprint_is_equal(Fingerprint const *f1, Fingerprint const *f2)
{
    return strcmp(f1->uuid, f2->uuid) == 0 &&
       storage_is_equal(&f1->memory, &f2->memory) &&
       ((f1->disk.algorithm == NULL && f2->disk.algorithm == NULL)
       || storage_is_equal(&f1->disk, &f2->disk))
    ;
}
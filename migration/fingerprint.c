#include <stdbool.h>

#include "migration/fingerprint.h"
#include "qapi/error.h"

void fingerprint_free(Fingerprint *fingerprint)
{
    if (fingerprint) {
        g_free(fingerprint->uuid);
        g_free(fingerprint->migration_type);
        g_free(fingerprint->memory.hash);
        g_free(fingerprint->memory.algorithm);
        g_free(fingerprint->disk.hash);
        g_free(fingerprint->disk.algorithm);
    }
}

static bool storage_parse(QDict *dict, Storage * storage, char const * name,
                          Error **errp)
{

    storage->algorithm = g_strdup(qdict_get_try_str(dict, "algorithm"));
    if (storage->algorithm == NULL) {
        error_setg(errp, "%s algorithm is missing", name);
        goto error;
    }

    storage->hash = g_strdup(qdict_get_try_str(dict, "hash"));
    if (storage->hash == NULL) {
        error_setg(errp, "%s hash is missing", name);
        goto error;
    }

    return true;

error:
    return false;
}

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
        error_setg(errp, "unable to cast json object to dict");
        goto error;
    }

    fingerprint->uuid = g_strdup(qdict_get_try_str(dict, "uuid"));
    if (fingerprint->uuid == NULL) {
        error_setg(errp, "unable to get VM uuid");
        goto error;
    }

    fingerprint->migration_type = g_strdup(
        qdict_get_try_str(dict, "migration_type")
    );
    if (fingerprint->migration_type == NULL) {
        error_setg(errp, "unable to get VM migration_type");
        goto error;
    }

    dict = qdict_get_qdict(dict, "fingerprints");
    if (dict == NULL) {
        error_setg(errp, "unable to get fingerprints dict");
        goto error;
    }

    dict = qdict_get_qdict(dict, "memory");
    if (dict == NULL) {
        error_setg(errp, "unable to get memory dict");
        goto error;
    }

    if (!storage_parse(dict, &fingerprint->memory, "memory", errp)) {
        goto error;
    }

    dict = qdict_get_qdict(dict, "disk");
    if (dict != NULL) {
        if (!storage_parse(dict, &fingerprint->disk, "disk", errp)) {
            goto error;
        }
    }

    return fingerprint;

error:
    fingerprint_free(fingerprint);
    return NULL;
}

Fingerprint *fingerprint_alloc(char const *uuid,
                                char const *migration_type,
                                char const *memory_algorithm,
                                char const *memory_hash) {

    Fingerprint *f = g_malloc0(sizeof(Fingerprint));

    if (uuid) {
        f->uuid = strdup(uuid);
    } else {
        f->uuid = strdup("");
    }

    f->migration_type = strdup(migration_type);
    f->memory.algorithm = strdup(memory_algorithm);
    f->memory.hash = strdup(memory_hash);

    return f;
}

Fingerprint *fingerprint_from_channel(char const *uuid,
                                      char const *migration_type,
                                      QIOChannelFingerprint *fioc,
                                      Error **errp) {

    int i;
    unsigned char hash[SHA_DIGEST_LENGTH];
    char hash_buf[SHA_DIGEST_LENGTH * 2 + 1] = {'\0'};

    if (!qio_channel_fingerprint_get_hash(fioc, hash)) {
        error_setg_errno(errp, errno, "failed to get to fingerprint hash");
        return NULL;
    }

    for (i = 0; i < SHA_DIGEST_LENGTH; i++) {
        sprintf(&hash_buf[i * 2], "%02x", hash[i]);
    }

    return fingerprint_alloc(
        uuid, migration_type, CHANNEL_FINGERPRINT_HASH_TYPE, hash_buf
    );
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

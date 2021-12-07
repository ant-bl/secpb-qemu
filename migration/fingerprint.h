#ifndef QEMU_FINGERPRINT_H
#define QEMU_FINGERPRINT_H

#include "qemu/osdep.h"
#include "qemu-common.h"
#include "sysemu/sysemu.h"
#include "qemu/typedefs.h"
#include "qapi/qmp/qobject.h"
#include "qapi/qmp/qjson.h"
#include "qapi/qmp/qdict.h"
#include "migration/qjson.h"
#include "io/channel-fingerprint.h"

typedef struct {
    char *algorithm;
    char *hash;
} Storage;

typedef struct {
    char *uuid;
    char *migration_type;
    Storage memory;
    Storage disk;
} Fingerprint;

#define FINGERPRINT_MAX_SIZE 1024

Fingerprint *fingerprint_parse(char const *buffer, Error **errp);

Fingerprint *fingerprint_alloc(char const *uuid,
                               char const *migration_type,
                               char const *memory_algorithm,
                               char const *memory_hash);

Fingerprint *fingerprint_from_channel(char const *uuid,
                                      char const *migration_type,
                                      QIOChannelFingerprint *fioc,
                                      Error **errp);

void fingerprint_free(Fingerprint *fingerprint);

struct QJSON *fingerprint_to_json(Fingerprint const *fingerprint);

bool fingerprint_is_equal(Fingerprint const *f1, Fingerprint const *f2);

#endif

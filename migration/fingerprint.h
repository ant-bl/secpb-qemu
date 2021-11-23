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

typedef struct {
    char const *algorithm;
    char const *hash;
} Storage;

typedef struct {
    char const *uuid;
    char const *migration_type;
    Storage memory;
    Storage disk;
} Fingerprint;

Fingerprint *fingerprint_parse(char const *buffer, Error **errp);

struct QJSON *fingerprint_to_json(Fingerprint const *fingerprint);

bool fingerprint_is_equal(Fingerprint const *f1, Fingerprint const *f2);

#endif
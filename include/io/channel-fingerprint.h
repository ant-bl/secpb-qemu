
#ifndef QIO_CHANNEL_FINGERPRINT_H
#define QIO_CHANNEL_FINGERPRINT_H

#include <stdio.h>
#include <openssl/sha.h>

#include "io/channel.h"
#include "qom/object.h"

#define TYPE_QIO_CHANNEL_FINGERPRINT "qio-channel-fingerprint"
OBJECT_DECLARE_SIMPLE_TYPE(QIOChannelFingerprint, QIO_CHANNEL_FINGERPRINT)

#define CHANNEL_FINGERPRINT_HASH_TYPE "sha1"

QIOChannelFingerprint *
qio_channel_fingerprint_new(QIOChannel *ioc, char const *fingerprint_path,
                            char const *ram_path, char const *disk_path,
                            Error **errp);

bool qio_channel_fingerprint_get_hash(QIOChannelFingerprint *fioc,
                                      unsigned char hash[SHA_DIGEST_LENGTH]);

#endif

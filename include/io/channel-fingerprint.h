
#ifndef QIO_CHANNEL_FINGERPRINT_H
#define QIO_CHANNEL_FINGERPRINT_H

#include <stdio.h>

#include "io/channel.h"
#include "qom/object.h"

#define TYPE_QIO_CHANNEL_FINGERPRINT "qio-channel-fingerprint"
OBJECT_DECLARE_SIMPLE_TYPE(QIOChannelFingerprint, QIO_CHANNEL_FINGERPRINT)

QIOChannelFingerprint *
qio_channel_fingerprint_new(QIOChannel *ioc, char const *fingerprint_path,
                            char const *ram_path, char const *disk_path,
                            Error **errp);

#endif
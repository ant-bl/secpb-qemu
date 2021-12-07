/*
 * QEMU live migration channel operations
 *
 * Copyright Red Hat, Inc. 2016
 *
 * Authors:
 *  Daniel P. Berrange <berrange@redhat.com>
 *
 * Contributions after 2012-01-13 are licensed under the terms of the
 * GNU GPL, version 2 or (at your option) any later version.
 */

#include "qemu/osdep.h"
#include "channel.h"
#include "tls.h"
#include "migration.h"
#include "qemu-file-channel.h"
#include "trace.h"
#include "qapi/error.h"
#include "io/channel-tls.h"
#include "io/channel-fingerprint.h"

/**
 * @fingerprint_channel_process_incoming - Create new incoming channel for
 * fingerprint
 *
 * @ioc: Channel to which we are connecting
 */
void fingerprint_channel_process_incoming(QIOChannel *ioc)
{
    Error *local_err = NULL;

    fingerprint_ioc_process_incoming(ioc, &local_err);

    if (local_err) {
        error_report_err(local_err);
    }
}


/**
 * @migration_channel_process_incoming - Create new incoming migration channel
 *
 * Notice that TLS is special.  For it we listen in a listener socket,
 * and then create a new client socket from the TLS library.
 *
 * @ioc: Channel to which we are connecting
 */
void migration_channel_process_incoming(QIOChannel *ioc)
{
    MigrationState *s = migrate_get_current();
    Error *local_err = NULL;

    trace_migration_set_incoming_channel(
        ioc, object_get_typename(OBJECT(ioc)));

    if (s->parameters.tls_creds &&
        *s->parameters.tls_creds &&
        !object_dynamic_cast(OBJECT(ioc),
                             TYPE_QIO_CHANNEL_TLS)) {
        migration_tls_channel_process_incoming(s, ioc, &local_err);
    } else {
        migration_ioc_process_incoming(ioc, &local_err);
    }

    if (local_err) {
        error_report_err(local_err);
    }
}

static bool migration_channel_setup_fingerprint(MigrationState *s,
                                                char const *fingerprint_path,
                                                QIOChannel **ioc,
                                                Error **errp) {

    QIOChannelFingerprint *fioc = NULL;
    char *ram_path = s->parameters.has_fingerprint_ram_path ?
        s->parameters.fingerprint_ram_path : NULL;
    char *disk_path = s->parameters.has_fingerprint_disk_path ?
        s->parameters.fingerprint_disk_path : NULL;

    if (!ram_path & !disk_path & !fingerprint_path) {
        return true;
    }

    if (fingerprint_path) {
        s->fingerprint_path = g_strdup(fingerprint_path);
    }

    fioc = qio_channel_fingerprint_new(*ioc, fingerprint_path != NULL,
                                       ram_path, disk_path, errp);
    if (!fioc) {
        return false;
    }

    *ioc = QIO_CHANNEL(fioc);

    return true;
}

/**
 * @migration_channel_connect - Create new outgoing migration channel
 *
 * @s: Current migration state
 * @ioc: Channel to which we are connecting
 * @hostname: Where we want to connect
 * @error: Error indicating failure to connect, free'd here
 */
void migration_channel_connect(MigrationState *s,
                               QIOChannel *ioc,
                               const char *hostname,
                               const char *fingerprint_path,
                               Error *error)
{
    trace_migration_set_outgoing_channel(
        ioc, object_get_typename(OBJECT(ioc)), hostname, error);

    if (!error) {
        if (s->parameters.tls_creds &&
            *s->parameters.tls_creds &&
            !object_dynamic_cast(OBJECT(ioc),
                                 TYPE_QIO_CHANNEL_TLS)) {
            migration_tls_channel_connect(s, ioc, hostname, fingerprint_path,
                                          &error);

            if (!error) {
                /*
                 * tls_channel_connect will call back to this
                 * function after the TLS handshake,
                 * so we mustn't call migrate_fd_connect until then
                 */

                return;
            }
        } else {
            QEMUFile *f = NULL;
            QIOChannel *nioc = ioc;

            if (!migration_channel_setup_fingerprint(
                s, fingerprint_path, &nioc, &error)
            ) {
                return;
            }

            f = qemu_fopen_channel_output(nioc);

            qemu_mutex_lock(&s->qemu_file_lock);
            s->to_dst_file = f;
            qemu_mutex_unlock(&s->qemu_file_lock);
        }
    }
    migrate_fd_connect(s, error);
    g_free(s->hostname);
    error_free(error);
}

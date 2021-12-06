#include "qemu/osdep.h"
#include "io/channel-fingerprint.h"
#include "qapi/error.h"

enum Log {
    LOG_RAM = 0b1,
    LOG_DISK = 0b10,
    LOG_FINGERPRINT = 0b100
};

struct QIOChannelFingerprint {
    QIOChannel parent;
    QIOChannel *inner;
    enum Log must_log;
    FILE *ram_file;
    int64_t length;
    bool has_length;
    SHA_CTX hash_context;
};

static struct iovec *iovec_copy(struct iovec const *src, int iovcnt)
{
    int i;
    struct iovec *dst = g_new(struct iovec, iovcnt);

    for (i = 0; i < iovcnt; i++) {
        dst[i].iov_base = g_memdup(src[i].iov_base, src[i].iov_len);
        dst[i].iov_len = src[i].iov_len;
    }

    return dst;
}

static void iovec_free(struct iovec *io, int iovcnt)
{
    int i;

    for (i = 0; i < iovcnt; i++) {
        g_free(io[i].iov_base);
    }
    g_free(io);
}

static FILE *log_open(char const *path, Error **errp)
{
    FILE *file = NULL;

    file = fopen(path, "w");
    if (file == NULL) {
        error_reportf_err(
            *errp, "Unable to open dump file: %s error: %s",
            path, strerror(errno)
        );
    }

    return file;
}

static int log_write_full_iovec(FILE *out, struct iovec const *io,
                                int iovcnt, Error **errp) {

    int i;

    for (i = 0; i < iovcnt; i++) {
        if (fwrite(io[i].iov_base, 1, io[i].iov_len, out) != io[i].iov_len) {
            error_reportf_err(
                *errp, "Unable to dump file error %s", strerror(errno)
            );
            return -1;
        }
    }

    fflush(out);

    return 0;
}

static int log_write_size_iovec(FILE *out, struct iovec const *io,
                                int iovcnt, ssize_t len, Error **errp) {

    int i;
    long int sum = 0;

    for (i = 0; i < iovcnt && len != 0; i++) {
        size_t write_size;

        if (len > io[i].iov_len) {
            write_size = io[i].iov_len;
            len -= io[i].iov_len;
        } else {
            write_size = len;
            len = 0;
        }

        if (fwrite(io[i].iov_base, 1, write_size, out) != write_size) {
            error_reportf_err(
                *errp, "Unable to dump file error %s", strerror(errno)
            );
            return -1;
        }
        sum += write_size;
    }

    fflush(out);

    return sum;
}

static int log_full_iovec(QIOChannelFingerprint *fioc,
                          struct iovec *copy, size_t niov,
                          Error **errp)
{
    if (fioc->must_log & LOG_RAM) {
        if (log_write_full_iovec(fioc->ram_file, copy, niov, errp) != 0) {
            return -1;
        }
    }

    if (fioc->must_log & LOG_FINGERPRINT) {
        int i;
        for (i = 0; i < niov; i++) {
            if (!SHA1_Update(&fioc->hash_context,
                            copy[i].iov_base,
                            copy[i].iov_len)) {
                error_setg_errno(errp, errno,
                                 "unable to update SHA fingerprint");
                return -1;
            }
        }
    }

    return 0;
}

static int log_size_iovec(QIOChannelFingerprint *fioc,
                          struct iovec const *copy, size_t niov,
                          ssize_t size, Error **errp)
{
    if (fioc->must_log & LOG_RAM) {
        if (log_write_size_iovec(fioc->ram_file, copy, niov,
                                 size, errp) != size) {
            return -1;
        }
    }

    return 0;
}

QIOChannelFingerprint *
qio_channel_fingerprint_new(QIOChannel *ioc, char const *fingerprint_path,
                            char const *ram_path, char const *disk_path,
                            Error **errp) {

    QIOChannelFingerprint *fioc = NULL;

    fioc = QIO_CHANNEL_FINGERPRINT(object_new(TYPE_QIO_CHANNEL_FINGERPRINT));
    fioc->has_length = false;

    if (ram_path != NULL) {
        fioc->ram_file = log_open(ram_path, errp);
        if (fioc->ram_file == NULL) {
            goto err_open;
        }
        fioc->must_log |= LOG_RAM;
    }

    if (fingerprint_path != NULL) {
        fioc->must_log |= LOG_FINGERPRINT;
        if (!SHA1_Init(&fioc->hash_context)) {
            goto err_sha;
        }
    }

    fioc->inner = ioc;
    object_ref(OBJECT(ioc));

    return fioc;

err_open:
    object_unref(fioc);
    return NULL;

err_sha:
    error_setg_errno(errp, errno, "unable to update SHA fingerprint");
    return NULL;
}

bool qio_channel_fingerprint_get_hash(QIOChannelFingerprint *fioc,
                                      unsigned char buf[SHA_DIGEST_LENGTH])
{
    if (!SHA1_Final(buf, &fioc->hash_context)) {
        return false;
    }
    return true;
}

static ssize_t qio_channel_fingerprint_readv(QIOChannel *ioc,
                                             const struct iovec *iov,
                                             size_t niov,
                                             int **fds,
                                             size_t *nfds,
                                             Error **errp)
{
    ssize_t ret;
    QIOChannelFingerprint *fioc = QIO_CHANNEL_FINGERPRINT(ioc);

    ret = qio_channel_readv(fioc->inner, iov, niov, errp);

    if (ret > 0 && fioc->must_log != 0 &&
        log_size_iovec(fioc, iov, niov, ret, errp) != 0) {
        return -1;
    }

    return ret;
}

static ssize_t qio_channel_fingerprint_writev(QIOChannel *ioc,
                                              const struct iovec *iov,
                                              size_t niov,
                                              int *fds,
                                              size_t nfds,
                                              Error **errp)
{
    ssize_t ret;
    QIOChannelFingerprint *fioc = QIO_CHANNEL_FINGERPRINT(ioc);
    struct iovec *copy = iovec_copy(iov, niov);

    if (fioc->must_log != 0 &&
        log_full_iovec(fioc, copy, niov, errp) != 0) {
        goto err_log;
    }

    ret = qio_channel_writev(fioc->inner, copy, niov, errp);

    iovec_free(copy, niov);

    return ret;

err_log:
    iovec_free(copy, niov);
    return -1;
}

static int qio_channel_fingerprint_set_blocking(QIOChannel *ioc,
                                                bool enabled,
                                                Error **errp)
{
    QIOChannelFingerprint *fioc = QIO_CHANNEL_FINGERPRINT(ioc);

    return qio_channel_set_blocking(fioc->inner, enabled, errp);
}

static void qio_channel_fingerprint_set_delay(QIOChannel *ioc,
                                              bool enabled)
{
    QIOChannelFingerprint *fioc = QIO_CHANNEL_FINGERPRINT(ioc);

    qio_channel_set_delay(fioc->inner, enabled);
}

static void qio_channel_fingerprint_set_cork(QIOChannel *ioc,
                                             bool enabled)
{
    QIOChannelFingerprint *fioc = QIO_CHANNEL_FINGERPRINT(ioc);

    qio_channel_set_cork(fioc->inner, enabled);
}

static int qio_channel_fingerprint_shutdown(QIOChannel *ioc,
                                            QIOChannelShutdown how,
                                            Error **errp)
{
    QIOChannelFingerprint *fioc = QIO_CHANNEL_FINGERPRINT(ioc);

    return qio_channel_shutdown(fioc->inner, how, errp);
}

static int qio_channel_fingerprint_close(QIOChannel *ioc,
                                         Error **errp)
{
    QIOChannelFingerprint *fioc = QIO_CHANNEL_FINGERPRINT(ioc);

    return qio_channel_close(fioc->inner, errp);
}

static void qio_channel_fingerprint_set_aio_fd_handler(QIOChannel *ioc,
                                                       AioContext *ctx,
                                                       IOHandler *io_read,
                                                       IOHandler *io_write,
                                                       void *opaque)
{
    QIOChannelFingerprint *fioc = QIO_CHANNEL_FINGERPRINT(ioc);

    qio_channel_set_aio_fd_handler(fioc->inner, ctx, io_read, io_write, opaque);
}

static GSource *qio_channel_fingerprint_create_watch(QIOChannel *ioc,
                                                     GIOCondition condition)
{
    QIOChannelFingerprint *fioc = QIO_CHANNEL_FINGERPRINT(ioc);

    return qio_channel_create_watch(fioc->inner, condition);
}

static off_t qio_channel_fingerprint_io_seek(QIOChannel *ioc,
                     off_t offset,
                     int whence,
                     Error **errp) {

    QIOChannelFingerprint *fioc = QIO_CHANNEL_FINGERPRINT(ioc);

    return qio_channel_io_seek(fioc->inner, offset, whence, errp);
}

static void qio_channel_fingerprint_class_init(ObjectClass *klass,
                                               void *class_data G_GNUC_UNUSED)
{
    QIOChannelClass *ioc_klass = QIO_CHANNEL_CLASS(klass);

    ioc_klass->io_writev = qio_channel_fingerprint_writev;
    ioc_klass->io_readv = qio_channel_fingerprint_readv;
    ioc_klass->io_set_blocking = qio_channel_fingerprint_set_blocking;
    ioc_klass->io_set_delay = qio_channel_fingerprint_set_delay;
    ioc_klass->io_set_cork = qio_channel_fingerprint_set_cork;
    ioc_klass->io_close = qio_channel_fingerprint_close;
    ioc_klass->io_shutdown = qio_channel_fingerprint_shutdown;
    ioc_klass->io_create_watch = qio_channel_fingerprint_create_watch;
    ioc_klass->io_set_aio_fd_handler =
        qio_channel_fingerprint_set_aio_fd_handler;
    ioc_klass->io_seek = qio_channel_fingerprint_io_seek;
}

static void qio_channel_fingerprint_init(Object *obj)
{
}

static void qio_channel_fingerprint_finalize(Object *obj)
{
    QIOChannelFingerprint *fioc = QIO_CHANNEL_FINGERPRINT(obj);

    if (fioc->ram_file != NULL) {
        fclose(fioc->ram_file);
        fioc->ram_file = NULL;
    }
}

static const TypeInfo qio_channel_fingerprint_info = {
    .parent = TYPE_QIO_CHANNEL,
    .name = TYPE_QIO_CHANNEL_FINGERPRINT,
    .instance_size = sizeof(QIOChannelFingerprint),
    .instance_init = qio_channel_fingerprint_init,
    .instance_finalize = qio_channel_fingerprint_finalize,
    .class_init = qio_channel_fingerprint_class_init,
};

static void qio_channel_fingerprint_register_types(void)
{
    type_register_static(&qio_channel_fingerprint_info);
}

type_init(qio_channel_fingerprint_register_types);

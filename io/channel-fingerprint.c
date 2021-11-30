#include "qemu/osdep.h"
#include "io/channel-fingerprint.h"
#include "qapi/error.h"

struct QIOChannelFingerprint {
    QIOChannel parent;
    QIOChannel *inner;
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

QIOChannelFingerprint *
qio_channel_fingerprint_new(QIOChannel *ioc, char const *fingerprint_path,
                            char const *ram_path, char const *disk_path,
                            Error **errp) {

    QIOChannelFingerprint *fioc;

    fioc = QIO_CHANNEL_FINGERPRINT(object_new(TYPE_QIO_CHANNEL_FINGERPRINT));

    fioc->inner = ioc;

    object_ref(OBJECT(ioc));

    return fioc;
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

    ret = qio_channel_writev(fioc->inner, copy, niov, errp);

    iovec_free(copy, niov);

    return ret;
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
    QIOChannelFingerprint *fioc = QIO_CHANNEL_FINGERPRINT(obj);
}

static void qio_channel_fingerprint_finalize(Object *obj)
{
    QIOChannelFingerprint *ioc = QIO_CHANNEL_FINGERPRINT(obj);
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

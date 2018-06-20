/*
 * This work is licensed under the terms of the GNU GPL, version 2 or
 * (at your option) any later version.  See the COPYING file in the
 * top-level directory.
 */

#include <glib.h>
#include <linux/input.h>

#include "qemu/osdep.h"
#include "qemu/iov.h"
#include "qemu/bswap.h"
#include "contrib/libvhost-user/libvhost-user.h"
#include "contrib/libvhost-user/libvhost-user-glib.h"
#include "standard-headers/linux/virtio_input.h"

typedef struct virtio_input_event virtio_input_event;
typedef struct virtio_input_config virtio_input_config;

typedef struct VuInput {
    VugDev dev;
    GSource *evsrc;
    int evdevfd;
    GArray *config;
    virtio_input_event *queue;
    uint32_t qindex, qsize;
} VuInput;

static void vi_input_send(VuInput *vi, struct virtio_input_event *event)
{
    VuDev *dev = &vi->dev.parent;
    VuVirtq *vq = vu_get_queue(dev, 0);
    VuVirtqElement *elem;
    unsigned have, need;
    int i, len;

    /* queue up events ... */
    if (vi->qindex == vi->qsize) {
        vi->qsize++;
        vi->queue = realloc(vi->queue, vi->qsize *
                                sizeof(virtio_input_event));
    }
    vi->queue[vi->qindex++] = *event;

    /* ... until we see a report sync ... */
    if (event->type != htole16(EV_SYN) ||
        event->code != htole16(SYN_REPORT)) {
        return;
    }

    /* ... then check available space ... */
    need = sizeof(virtio_input_event) * vi->qindex;
    vu_queue_get_avail_bytes(dev, vq, &have, NULL, need, 0);
    if (have < need) {
        vi->qindex = 0;
        g_warning("ENOSPC in vq, dropping events");
        return;
    }

    /* ... and finally pass them to the guest */
    for (i = 0; i < vi->qindex; i++) {
        elem = vu_queue_pop(dev, vq, sizeof(VuVirtqElement));
        if (!elem) {
            /* should not happen, we've checked for space beforehand */
            g_warning("%s: Huh?  No vq elem available ...\n", __func__);
            return;
        }
        len = iov_from_buf(elem->in_sg, elem->in_num,
                           0, vi->queue + i, sizeof(virtio_input_event));
        vu_queue_push(dev, vq, elem, len);
        g_free(elem);
    }
    vu_queue_notify(&vi->dev.parent, vq);
    vi->qindex = 0;
}

static void
vi_evdev_watch(VuDev *dev, int condition, void *data)
{
    VuInput *vi = data;
    int fd = vi->evdevfd;

    g_debug("Got evdev condition %x", condition);

    struct virtio_input_event virtio;
    struct input_event evdev;
    int rc;

    for (;;) {
        rc = read(fd, &evdev, sizeof(evdev));
        if (rc != sizeof(evdev)) {
            break;
        }

        g_debug("input %d %d %d", evdev.type, evdev.code, evdev.value);

        virtio.type  = htole16(evdev.type);
        virtio.code  = htole16(evdev.code);
        virtio.value = htole32(evdev.value);
        vi_input_send(vi, &virtio);
    }
}


static void vi_handle_status(VuInput *vi, virtio_input_event *event)
{
    struct input_event evdev;
    int rc;

    if (gettimeofday(&evdev.time, NULL)) {
        perror("vi_handle_status: gettimeofday");
        return;
    }

    evdev.type = le16toh(event->type);
    evdev.code = le16toh(event->code);
    evdev.value = le32toh(event->value);

    rc = write(vi->evdevfd, &evdev, sizeof(evdev));
    if (rc == -1) {
        perror("vi_host_handle_status: write");
    }
}

static void vi_handle_sts(VuDev *dev, int qidx)
{
    VuInput *vi = container_of(dev, VuInput, dev.parent);
    VuVirtq *vq = vu_get_queue(dev, qidx);
    virtio_input_event event;
    VuVirtqElement *elem;
    int len;

    g_debug("%s", G_STRFUNC);

    for (;;) {
        elem = vu_queue_pop(dev, vq, sizeof(VuVirtqElement));
        if (!elem) {
            break;
        }

        memset(&event, 0, sizeof(event));
        len = iov_to_buf(elem->out_sg, elem->out_num,
                         0, &event, sizeof(event));
        vi_handle_status(vi, &event);
        vu_queue_push(dev, vq, elem, len);
        g_free(elem);
    }

    vu_queue_notify(&vi->dev.parent, vq);
}

static void
vi_panic(VuDev *dev, const char *msg)
{
    g_critical("%s\n", msg);
    exit(1);
}

static void
vi_queue_set_started(VuDev *dev, int qidx, bool started)
{
    VuInput *vi = container_of(dev, VuInput, dev.parent);
    VuVirtq *vq = vu_get_queue(dev, qidx);

    g_debug("queue started %d:%d", qidx, started);

    if (qidx == 0) {
        if (started && !vi->evsrc) {
            vi->evsrc = vug_source_new(&vi->dev, vi->evdevfd,
                                       G_IO_IN, vi_evdev_watch, vi);
        } else if (!started) {
            g_source_destroy(vi->evsrc);
            vi->evsrc = NULL;
        }
    } else {
        vu_set_queue_handler(dev, vq, started ? vi_handle_sts : NULL);
    }
}

static int
vi_process_msg(VuDev *dev, VhostUserMsg *vmsg, int *do_reply)
{
    VuInput *vi = container_of(dev, VuInput, dev.parent);

    switch (vmsg->request) {
    case VHOST_USER_INPUT_GET_CONFIG:
        vmsg->size = vi->config->len * sizeof(virtio_input_config);
        vmsg->data = g_memdup(vi->config->data, vmsg->size);
        *do_reply = true;
        return 1;
    default:
        return 0;
    }
}

static const VuDevIface vuiface = {
    .queue_set_started = vi_queue_set_started,
    .process_msg = vi_process_msg,
};

static void
vi_bits_config(VuInput *vi, int type, int count)
{
    virtio_input_config bits;
    int rc, i, size = 0;

    memset(&bits, 0, sizeof(bits));
    rc = ioctl(vi->evdevfd, EVIOCGBIT(type, count / 8), bits.u.bitmap);
    if (rc < 0) {
        return;
    }

    for (i = 0; i < count / 8; i++) {
        if (bits.u.bitmap[i]) {
            size = i + 1;
        }
    }
    if (size == 0) {
        return;
    }

    bits.select = VIRTIO_INPUT_CFG_EV_BITS;
    bits.subsel = type;
    bits.size   = size;
    g_array_append_val(vi->config, bits);
}

static int unix_sock_new(char *path)
{
    int sock;
    struct sockaddr_un un;
    size_t len;

    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock <= 0) {
        perror("socket");
        return -1;
    }

    un.sun_family = AF_UNIX;
    snprintf(un.sun_path, sizeof(un.sun_path), "%s", path);
    len = sizeof(un.sun_family) + strlen(un.sun_path);

    unlink(path);
    if (bind(sock, (struct sockaddr *)&un, len) < 0) {
        perror("bind");
        goto fail;
    }

    if (listen(sock, 1) < 0) {
        perror("listen");
        goto fail;
    }

    return sock;

fail:
    close(sock);

    return -1;
}

static char **opt_fname;
static char *opt_socket_path;
static gboolean opt_nograb;

static GOptionEntry entries[] = {
    { "no-grab", 'n', 0, G_OPTION_ARG_NONE, &opt_nograb,
      "Don't grab device", NULL },
    { "socket-path", 's', 0, G_OPTION_ARG_FILENAME, &opt_socket_path,
      "Use UNIX socket path", "PATH" },
    { G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, &opt_fname,
      "EVDEV filename", "..." },
    { NULL, }
};

int
main(int argc, char *argv[])
{
    GMainLoop *loop = NULL;
    VuInput vi = { 0, };
    int rc, ver, fd;
    virtio_input_config id;
    struct input_id ids;
    GError *error = NULL;
    GOptionContext *context;

    context = g_option_context_new("EVDEV - vhost-user-input sample");
    g_option_context_add_main_entries(context, entries, NULL);
    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_printerr("Option parsing failed: %s\n", error->message);
        exit(1);
    }
    if (!opt_fname || !opt_fname[0] || opt_fname[1]) {
        g_printerr("Please specify a single EVDEV filename\n");
        exit(1);
    }

    vi.evdevfd = open(opt_fname[0], O_RDWR);
    if (vi.evdevfd < 0) {
        g_printerr("Failed to open evdev: %s\n", g_strerror(errno));
        exit(1);
    }

    rc = ioctl(vi.evdevfd, EVIOCGVERSION, &ver);
    if (rc < 0) {
        g_printerr("%s: is not an evdev device\n", argv[1]);
        exit(1);
    }

    if (!opt_nograb) {
        rc = ioctl(vi.evdevfd, EVIOCGRAB, 1);
        if (rc < 0) {
            g_printerr("Failed to grab device\n");
            exit(1);
        }
    }

    vi.config = g_array_new(false, false, sizeof(virtio_input_config));
    memset(&id, 0, sizeof(id));
    ioctl(vi.evdevfd, EVIOCGNAME(sizeof(id.u.string) - 1), id.u.string);
    id.select = VIRTIO_INPUT_CFG_ID_NAME;
    id.size = strlen(id.u.string);
    g_array_append_val(vi.config, id);

    if (ioctl(vi.evdevfd, EVIOCGID, &ids) == 0) {
        memset(&id, 0, sizeof(id));
        id.select = VIRTIO_INPUT_CFG_ID_DEVIDS;
        id.size = sizeof(struct virtio_input_devids);
        id.u.ids.bustype = cpu_to_le16(ids.bustype);
        id.u.ids.vendor  = cpu_to_le16(ids.vendor);
        id.u.ids.product = cpu_to_le16(ids.product);
        id.u.ids.version = cpu_to_le16(ids.version);
        g_array_append_val(vi.config, id);
    }

    vi_bits_config(&vi, EV_KEY, KEY_CNT);
    vi_bits_config(&vi, EV_REL, REL_CNT);
    vi_bits_config(&vi, EV_ABS, ABS_CNT);
    vi_bits_config(&vi, EV_MSC, MSC_CNT);
    vi_bits_config(&vi, EV_SW,  SW_CNT);
    g_debug("config length: %u", vi.config->len);

    if (opt_socket_path) {
        int lsock = unix_sock_new(opt_socket_path);
        fd = accept(lsock, NULL, NULL);
        close(lsock);
    } else {
        fd = 3;
    }
    if (fd == -1) {
        g_printerr("Invalid socket");
        exit(1);
    }
    vug_init(&vi.dev, fd, vi_panic, &vuiface);

    loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    vug_deinit(&vi.dev);

    return 0;
}

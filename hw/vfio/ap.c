/*
 * VFIO based AP matrix device assignment
 *
 * Copyright 2018 IBM Corp.
 * Author(s): Tony Krowiak <akrowiak@linux.vnet.ibm.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or (at
 * your option) any later version. See the COPYING file in the top-level
 * directory.
 */

#include <linux/vfio.h>
#include <sys/ioctl.h>
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/sysbus.h"
#include "hw/vfio/vfio.h"
#include "hw/vfio/vfio-common.h"
#include "hw/s390x/ap-device.h"
#include "qemu/error-report.h"
#include "qemu/queue.h"
#include "qemu/option.h"
#include "qemu/config-file.h"
#include "cpu.h"
#include "kvm_s390x.h"
#include "sysemu/sysemu.h"

#define VFIO_AP_DEVICE_TYPE      "vfio-ap"

typedef struct VFIOAPDevice {
    APDevice apdev;
    VFIODevice vdev;
    QTAILQ_ENTRY(VFIOAPDevice) sibling;
} VFIOAPDevice;

VFIOAPDevice *vfio_apdev;

static void vfio_ap_compute_needs_reset(VFIODevice *vdev)
{
    vdev->needs_reset = false;
}

/*
 * We don't need vfio_hot_reset_multi and vfio_eoi operations for
 * vfio-ap-matrix device now.
 */
struct VFIODeviceOps vfio_ap_ops = {
    .vfio_compute_needs_reset = vfio_ap_compute_needs_reset,
};

static void vfio_ap_put_device(VFIOAPDevice *vapdev)
{
    g_free(vapdev->vdev.name);
    vfio_put_base_device(&vapdev->vdev);
}

static VFIOGroup *vfio_ap_get_group(VFIOAPDevice *vapdev, Error **errp)
{
    char *tmp, group_path[PATH_MAX];
    ssize_t len;
    int groupid;

    tmp = g_strdup_printf("%s/iommu_group", vapdev->vdev.sysfsdev);
    len = readlink(tmp, group_path, sizeof(group_path));
    g_free(tmp);

    if (len <= 0 || len >= sizeof(group_path)) {
        error_setg(errp, "%s: no iommu_group found for %s",
                   VFIO_AP_DEVICE_TYPE, vapdev->vdev.sysfsdev);
        return NULL;
    }

    group_path[len] = 0;

    if (sscanf(basename(group_path), "%d", &groupid) != 1) {
        error_setg(errp, "vfio: failed to read %s", group_path);
        return NULL;
    }

    return vfio_get_group(groupid, &address_space_memory, errp);
}

static void vfio_ap_realize(DeviceState *dev, Error **errp)
{
    VFIOGroup *vfio_group;
    APDevice *apdev = DO_UPCAST(APDevice, parent_obj, dev);
    char *mdevid;
    Error *local_err = NULL;
    int ret;

    /*
     * Since a guest's matrix is configured in its entirety by the mediated
     * matrix device and hot plug is not currently supported, there is no
     * need to have more than one vfio-ap device. Check if a vfio-ap device
     * has already been defined.
     */
    if (vfio_apdev) {
        error_setg(&local_err, "Only one %s device is allowed",
                   VFIO_AP_DEVICE_TYPE);
        goto out_err;
    }

    if (!s390_has_feat(S390_FEAT_AP)) {
        error_setg(&local_err, "AP support not enabled");
        goto out_err;
    }

    vfio_apdev = DO_UPCAST(VFIOAPDevice, apdev, apdev);

    vfio_group = vfio_ap_get_group(vfio_apdev, &local_err);
    if (!vfio_group) {
        goto out_err;
    }

    vfio_apdev->vdev.ops = &vfio_ap_ops;
    vfio_apdev->vdev.type = VFIO_DEVICE_TYPE_AP;
    mdevid = basename(vfio_apdev->vdev.sysfsdev);
    vfio_apdev->vdev.name = g_strdup_printf("%s", mdevid);
    vfio_apdev->vdev.dev = dev;

    ret = vfio_get_device(vfio_group, mdevid, &vfio_apdev->vdev, &local_err);
    if (ret) {
        goto out_get_dev_err;
    }

    return;

out_get_dev_err:
    vfio_ap_put_device(vfio_apdev);
    vfio_put_group(vfio_group);
out_err:
    vfio_apdev = NULL;
    error_propagate(errp, local_err);
}

static void vfio_ap_unrealize(DeviceState *dev, Error **errp)
{
    APDevice *apdev = DO_UPCAST(APDevice, parent_obj, dev);
    VFIOAPDevice *vapdev = DO_UPCAST(VFIOAPDevice, apdev, apdev);
    VFIOGroup *group = vapdev->vdev.group;

    vfio_ap_put_device(vapdev);
    vfio_put_group(group);
    vfio_apdev = NULL;
}

static Property vfio_ap_properties[] = {
    DEFINE_PROP_STRING("sysfsdev", VFIOAPDevice, vdev.sysfsdev),
    DEFINE_PROP_END_OF_LIST(),
};

static const VMStateDescription vfio_ap_vmstate = {
    .name = VFIO_AP_DEVICE_TYPE,
    .unmigratable = 1,
};

static void vfio_ap_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->props = vfio_ap_properties;
    dc->vmsd = &vfio_ap_vmstate;
    dc->desc = "VFIO-based AP device assignment";
    dc->realize = vfio_ap_realize;
    dc->unrealize = vfio_ap_unrealize;
    dc->hotpluggable = false;
}

static const TypeInfo vfio_ap_info = {
    .name = VFIO_AP_DEVICE_TYPE,
    .parent = AP_DEVICE_TYPE,
    .instance_size = sizeof(VFIOAPDevice),
    .class_init = vfio_ap_class_init,
};

static void vfio_ap_type_init(void)
{
    type_register_static(&vfio_ap_info);
    vfio_apdev = NULL;
}

type_init(vfio_ap_type_init)

/*
 * Resource handler interface.
 *
 * Copyright (c) 2018 Red Hat Inc.
 *
 * Authors:
 *  David Hildenbrand <david@redhat.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "hw/resource-handler.h"
#include "qemu/module.h"

void resource_handler_pre_assign(ResourceHandler *rh,
                                 const DeviceState *dev, Error **errp)
{
    ResourceHandlerClass *rhc = RESOURCE_HANDLER_GET_CLASS(rh);

    if (rhc->pre_assign) {
        rhc->pre_assign(rh, dev, errp);
    }
}

void resource_handler_assign(ResourceHandler *rh, DeviceState *dev,
                             Error **errp)
{
    ResourceHandlerClass *rhc = RESOURCE_HANDLER_GET_CLASS(rh);

    if (rhc->assign) {
        rhc->assign(rh, dev, errp);
    }
}

void resource_handler_unassign(ResourceHandler *rh, DeviceState *dev)
{
    ResourceHandlerClass *rhc = RESOURCE_HANDLER_GET_CLASS(rh);

    if (rhc->unassign) {
        rhc->unassign(rh, dev);
    }
}

static const TypeInfo resource_handler_info = {
    .name          = TYPE_RESOURCE_HANDLER,
    .parent        = TYPE_INTERFACE,
    .class_size = sizeof(ResourceHandlerClass),
};

static void resource_handler_register_types(void)
{
    type_register_static(&resource_handler_info);
}

type_init(resource_handler_register_types)

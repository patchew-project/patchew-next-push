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

#ifndef RESOURCE_HANDLER_H
#define RESOURCE_HANDLER_H

#include "qom/object.h"

#define TYPE_RESOURCE_HANDLER "resource-handler"

#define RESOURCE_HANDLER_CLASS(klass) \
    OBJECT_CLASS_CHECK(ResourceHandlerClass, (klass), TYPE_RESOURCE_HANDLER)
#define RESOURCE_HANDLER_GET_CLASS(obj) \
    OBJECT_GET_CLASS(ResourceHandlerClass, (obj), TYPE_RESOURCE_HANDLER)
#define RESOURCE_HANDLER(obj) \
    INTERFACE_CHECK(ResourceHandler, (obj), TYPE_RESOURCE_HANDLER)

typedef struct ResourceHandler {
    Object Parent;
} ResourceHandler;

typedef struct ResourceHandlerClass {
    InterfaceClass parent;

    void (*pre_assign)(ResourceHandler *rh, const DeviceState *dev,
                       Error **errp);
    void (*assign)(ResourceHandler *rh, DeviceState *dev, Error **errp);
    void (*unassign)(ResourceHandler *rh, DeviceState *dev);
} ResourceHandlerClass;

void resource_handler_pre_assign(ResourceHandler *rh, const DeviceState *dev,
                                 Error **errp);
void resource_handler_assign(ResourceHandler *rh, DeviceState *dev,
                             Error **errp);
void resource_handler_unassign(ResourceHandler *rh, DeviceState *dev);

#endif /* RESOURCE_HANDLER_H */

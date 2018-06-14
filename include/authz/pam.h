/*
 * QEMU pam authorization driver
 *
 * Copyright (c) 2018 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef QAUTHZ_PAM_H__
#define QAUTHZ_PAM_H__

#include "authz/base.h"


#define TYPE_QAUTHZ_PAM "authz-pam"

#define QAUTHZ_PAM_CLASS(klass) \
     OBJECT_CLASS_CHECK(QAuthZPamClass, (klass), \
                        TYPE_QAUTHZ_PAM)
#define QAUTHZ_PAM_GET_CLASS(obj) \
     OBJECT_GET_CLASS(QAuthZPamClass, (obj), \
                      TYPE_QAUTHZ_PAM)
#define QAUTHZ_PAM(obj) \
     INTERFACE_CHECK(QAuthZPam, (obj), \
                     TYPE_QAUTHZ_PAM)

typedef struct QAuthZPam QAuthZPam;
typedef struct QAuthZPamClass QAuthZPamClass;


/**
 * QAuthZPam:
 *
 * This authorization driver provides a pam mechanism
 * for granting access by matching user names against a
 * list of globs. Each match rule has an associated policy
 * and a catch all policy applies if no rule matches
 *
 * To create an instance of this class via QMP:
 *
 *  {
 *    "execute": "object-add",
 *    "arguments": {
 *      "qom-type": "authz-pam",
 *      "id": "auth0",
 *      "parameters": {
 *        "service": "qemu-vnc-tls"
 *      }
 *    }
 *  }
 *
 *
 */
struct QAuthZPam {
    QAuthZ parent_obj;

    char *service;
};


struct QAuthZPamClass {
    QAuthZClass parent_class;
};


QAuthZPam *qauthz_pam_new(const char *id,
                          const char *service,
                          Error **errp);


#endif /* QAUTHZ_PAM_H__ */


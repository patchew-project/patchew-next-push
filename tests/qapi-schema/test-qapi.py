#
# QAPI parser test harness
#
# Copyright (c) 2013 Red Hat Inc.
#
# Authors:
#  Markus Armbruster <armbru@redhat.com>
#
# This work is licensed under the terms of the GNU GPL, version 2 or later.
# See the COPYING file in the top-level directory.
#

from __future__ import print_function
import sys
from qapi.common import QAPIError, QAPISchema, QAPISchemaVisitor, \
    QAPISchemaObjectTypeMember


class QAPISchemaTestVisitor(QAPISchemaVisitor):

    def visit_module(self, name):
        print('module %s' % name)

    def visit_include(self, name, info):
        print('include %s' % name)

    def visit_enum_type(self, name, info, ifcond, members, prefix):
        print('enum %s' % name)
        if prefix:
            print('    prefix %s' % prefix)
        self._print_members(members)
        self._print_if(ifcond)

    def visit_object_type(self, name, info, ifcond, base, members, variants):
        print('object %s' % name)
        if base:
            print('    base %s' % base.name)
        self._print_members(members)
        self._print_variants(variants)
        self._print_if(ifcond)

    def visit_alternate_type(self, name, info, ifcond, variants):
        print('alternate %s' % name)
        self._print_variants(variants)
        self._print_if(ifcond)

    def visit_command(self, name, info, ifcond, arg_type, ret_type,
                      gen, success_response, boxed, allow_oob):
        print('command %s %s -> %s' % \
              (name, arg_type and arg_type.name, ret_type and ret_type.name))
        print('   gen=%s success_response=%s boxed=%s' % \
              (gen, success_response, boxed))
        self._print_if(ifcond)

    def visit_event(self, name, info, ifcond, arg_type, boxed):
        print('event %s %s' % (name, arg_type and arg_type.name))
        print('   boxed=%s' % boxed)
        self._print_if(ifcond)

    @staticmethod
    def _print_members(members):
        for m in members:
            print('    member %s%s' % (
                m.name,
                ': %s optional=%s' % (m.type.name, m.optional)
                if isinstance(m, QAPISchemaObjectTypeMember) else ''))

    @staticmethod
    def _print_variants(variants):
        if variants:
            print('    tag %s' % variants.tag_member.name)
            for v in variants.variants:
                print('    case %s: %s' % (v.name, v.type.name))

    @staticmethod
    def _print_if(ifcond, indent=4):
        if ifcond:
            print('%sif %s' % (' ' * indent, ifcond))

try:
    schema = QAPISchema(sys.argv[1])
except QAPIError as err:
    print(err, file=sys.stderr)
    exit(1)

schema.visit(QAPISchemaTestVisitor())

for doc in schema.docs:
    if doc.symbol:
        print('doc symbol=%s' % doc.symbol)
    else:
        print('doc freeform')
    print('    body=\n%s' % doc.body.text)
    for arg, section in doc.args.items():
        print('    arg=%s\n%s' % (arg, section.text))
    for section in doc.sections:
        print('    section=%s\n%s' % (section.name, section.text))

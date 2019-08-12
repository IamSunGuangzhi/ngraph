#!/usr/bin/env python
# ******************************************************************************
# Copyright 2017-2019 Intel Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ******************************************************************************
#
# This is a crude, hacky prototype of a class generator.

import json
import sys
import argparse

class ClassReadError(Exception):
    pass

class OpArgumentWriter():
    def __init__(self,name,description,idx):
        self._name = name
        self._description = description
        self._idx = idx

    def ctor_parameter_proto(self):
        s = ''
        s += 'const ::ngraph::Output<::ngraph::Node>& '
        s += self._name
        return s

    def getter_proto(self,is_const):
        s = ''
        s_const = 'const ' if is_const else ''
        s += ('::ngraph::Input<%s::ngraph::Node> get_' % s_const)
        s += self._name
        s += ('() %s{ ' % s_const)
        s += ('return input(%d);' % self._idx)
        s += ' }'
        return s

    def name(self):
        return self._name

class OpResultWriter():
    def __init__(self,name,description,idx):
        self._name = name
        self._description = description
        self._idx = idx

    def getter_proto(self,is_const):
        s = ''
        s_const = 'const ' if is_const else ''
        s += ('::ngraph::Output<%s::ngraph::Node> get_' % s_const)
        s += self._name
        s += ('() %s{ ' % s_const)
        s += ('return output(%d);' % self._idx)
        s += ' }'
        return s

    def name(self):
        return self._name

class OpAttributeWriter():
    def __init__(self,name,type,description,idx):
        self._name = name
        self._type = type
        self._description = description
        self._idx = idx

    def ctor_parameter_proto(self):
        return('const %s& %s' % (self._type, self._name))

    def getter_proto(self):
        return ('const %s& get_%s() const { return %s; }'
                   % (self._type, self._name, self.member_var_name()))
    def setter_proto(self):
        return ('void set_%s (const %s& %s) { %s = %s; }'
                   % (self._name, self._type, self._name, self.member_var_name(), self._name))

    def member_var_proto(self):
        return ('%s %s;' % (self._type, self.member_var_name()))

    def member_var_name(self):
        return 'm_' + self._name

    def name(self):
        return self._name

class ClassWriter():
    def __init__(self,f):
        self._load_op_json(f)

    def _load_op_json(self,f):
        j = json.load(f)

        if 'name' in j:
            self._name = j['name']
        else:
            raise ClassReadError('Required field \'name\' is missing')

        if 'dialect' in j:
            self._dialect = j['dialect']
        else:
            raise ClassReadError('Required field \'dialect\' is missing')
        
        self._commutative = 'commutative' in j and j['commutative']
        self._has_state = 'has_state' in j and j['has_state']

        if 'description' in j:
            self._description = j['description']
        else:
            self._description = ''

        self._arguments = []
        if 'arguments' in j:
            for (idx,arg_j) in enumerate(j['arguments']):
                self._arguments.append(
                    OpArgumentWriter(arg_j['name'],
                                     arg_j['description'],
                                     idx)
                )

        self._results = []
        if 'results' in j:
            for (idx,arg_j) in enumerate(j['results']):
                self._results.append(
                    OpResultWriter(arg_j['name'],
                                   arg_j['description'],
                                   idx)
                )

        self._attributes = []
        if 'attributes' in j:
            for (idx,attr_j) in enumerate(j['attributes']):
                self._attributes.append(
                    OpAttributeWriter(attr_j['name'],
                                      attr_j['type'],
                                      attr_j['description'],
                                      idx)
                )

        f.close()
    
    def qualified_name(self):
        return ('::ngraph::op::gen::%s::%s' % (self._dialect, self._name))

    def gen_copyright_header(self,f):
        f.write("""\
//*****************************************************************************
// Copyright 2017-2019 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//*****************************************************************************

""")

    def gen_class_declaration(self, f):
        f.write('namespace ngraph\n')
        f.write('{\n')
        f.write('    namespace op\n')
        f.write('    {\n')
        f.write('        namespace gen\n')
        f.write('        {\n')
        f.write('            namespace %s\n' % self._dialect)
        f.write('            {\n')
        f.write('                class %s : public ::ngraph::op::util::GenOp;\n' % self._name)
        f.write('            } // namespace %s\n' % self._dialect)
        f.write('        } // namespace gen\n')
        f.write('    } // namespace op\n')
        f.write('} // namespace ngraph\n')
        f.write('\n')

    def gen_class_definition(self, f):
        f.write('// ')
        f.write(self._description)
        f.write('\n')
        f.write('class %s : public ::ngraph::op::util::GenOp\n' % self.qualified_name())
        f.write('{')
        f.write('\n')

        f.write('public:\n')

        f.write('    NGRAPH_API static const ::std::string type_name;\n')
        f.write('    const std::string& description() const final override { return type_name; }\n')

        f.write('    %s() = default;\n' % self._name)

        f.write('    ')
        f.write(self._name)
        f.write('(')

        f.write(', '.join(argument.ctor_parameter_proto() for argument in self._arguments))

        if len(self._arguments) > 0 and len(self._attributes) > 0:
            f.write(', ')

        f.write(', '.join(attribute.ctor_parameter_proto() for attribute in self._attributes))

        f.write(')\n')
        f.write('        : ::ngraph::op::util::GenOp(')

        f.write('::ngraph::make_flat_output_vector(')
        f.write(', '.join(argument.name() for argument in self._arguments))
        f.write(')')

        f.write(')\n')

        for attribute in self._attributes:
            f.write('        , %s(%s)\n' % (attribute.member_var_name(), attribute.name()))

        f.write('    {\n')
        f.write('    }\n')

        for argument in self._arguments:
            f.write('    %s\n' % argument.getter_proto(is_const=False))

        for argument in self._arguments:
            f.write('    %s\n' % argument.getter_proto(is_const=True))

        for result in self._results:
            f.write('    %s\n' % result.getter_proto(is_const=False))

        for result in self._results:
            f.write('    %s\n' % result.getter_proto(is_const=True))

        for attribute in self._attributes:
            f.write('    %s\n' % attribute.getter_proto())

        for attribute in self._attributes:
            f.write('    %s\n' % attribute.setter_proto())

        f.write('    ::std::vector<::std::string> get_argument_keys() const final override { return ::std::vector<::std::string>{')
        f.write(', '.join('"%s"' % argument.name() for argument in self._arguments))
        f.write('}; }\n')

        f.write('    ::std::vector<::std::string> get_result_keys() const final override { return ::std::vector<::std::string>{')
        f.write(', '.join('"%s"' % result.name() for result in self._results))
        f.write('}; }\n')

        f.write('    ::std::vector<::std::string> get_attribute_keys() const final override { return ::std::vector<::std::string>{')
        f.write(', '.join('"%s"' % attribute.name() for attribute in self._attributes))
        f.write('}; }\n')

        f.write('    bool is_commutative() const final override { return %s; }\n' % ('true' if self._commutative else 'false'))
        f.write('    bool has_state() const final override { return %s; }\n' % ('true' if self._has_state else 'false'))

        f.write('    ::std::shared_ptr<::ngraph::Node> copy_with_new_inputs\n')
        f.write('        (const ::ngraph::OutputVector& inputs,\n')
        f.write('         const ::ngraph::NodeVector& control_dependencies)\n')
        f.write('        const final override\n')
        f.write('    {\n')
        f.write('        // TODO: check input count\n')
        f.write('        ::std::shared_ptr<::ngraph::Node> new_node = ::std::make_shared<%s>(' % self._name)
        f.write(', '.join('inputs[%d]' % idx for (idx,_) in enumerate(self._arguments)))
        if len(self._arguments) > 0 and len(self._attributes) > 0:
            f.write(', ')
        f.write(', '.join(attribute.member_var_name() for attribute in self._attributes))
        f.write(');\n')
        f.write('        // TODO: control deps\n')
        f.write('        return new_node;\n')
        f.write('    }\n')

        f.write('private:\n')

        for attribute in self._attributes:
            f.write('    %s\n' % attribute.member_var_proto())

        f.write('};\n')

    def gen_hpp_file(self, f):
        self.gen_copyright_header(f)

        f.write('#pragma once\n')
        f.write('\n')
        f.write('#include "ngraph/op/util/gen_op.hpp"\n')
        f.write('\n')

        self.gen_class_declaration(f)

        self.gen_class_definition(f)

    def gen_function_definitions(self, f):
        f.write('const ::std::string %s::type_name = "%s.%s";\n' % (self.qualified_name(),self._dialect,self._name))
        f.write('\n')

    def gen_cpp_file(self, f):
        self.gen_copyright_header(f)

        f.write('#include "ngraph/op/gen/%s/%s.hpp"\n' % (self._dialect,self._name))
        f.write('\n')

        self.gen_function_definitions(f)

if __name__ == "__main__":
    parser = argparse.ArgumentParser('class_gen')
    parser.add_argument('--defn', metavar='filename.json', type=argparse.FileType('r'), nargs=1, action='store', required=True, help='Source class definition file')
    parser.add_argument('--hpp', metavar='filename.hpp', type=argparse.FileType('w'), nargs=1, action='store', required=True, help='Destination header file')
    parser.add_argument('--cpp', metavar='filename.cpp', type=argparse.FileType('w'), nargs=1, action='store', required=True, help='Destination source file')
    flags = parser.parse_args()

    print('hpp: %s' % flags.hpp)

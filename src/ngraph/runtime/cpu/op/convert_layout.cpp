//*****************************************************************************
// Copyright 2017-2020 Intel Corporation
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

#include "ngraph/runtime/cpu/op/convert_layout.hpp"
#include "ngraph/runtime/cpu/cpu_layout_descriptor.hpp"
#include "ngraph/runtime/cpu/mkldnn_utils.hpp"

using namespace std;
using namespace ngraph;

constexpr NodeTypeInfo runtime::cpu::op::ConvertLayout::type_info;

runtime::cpu::op::ConvertLayout::ConvertLayout(
    const Output<Node>& arg, const shared_ptr<runtime::cpu::LayoutDescriptor>& layout)
    : Op({arg})
    , arg_output_index(arg.get_index())
    , output_layout(layout)
{
    runtime::cpu::mkldnn_utils::assign_mkldnn_kernel(this);
    constructor_validate_and_infer_types();
}

shared_ptr<Node>
    runtime::cpu::op::ConvertLayout::clone_with_new_inputs(const OutputVector& new_args) const
{
    if (new_args.size() != 1)
    {
        throw ngraph_error("Incorrect number of new arguments");
    }
    return make_shared<ConvertLayout>(new_args.at(0), output_layout);
}

void runtime::cpu::op::ConvertLayout::validate_and_infer_types()
{
    Input<Node> arg = input(0);

    shared_ptr<descriptor::Tensor> arg_tensor = arg.get_tensor_ptr();
    const auto& arg_layout = arg_tensor->get_tensor_layout();

    if (!arg_layout)
    {
        // throw ngraph_error("Layout conversion input tensor is missing layout information");
    }

    set_output_type(0, output_layout->get_element_type(), output_layout->get_shape());
    get_output_tensor_ptr(0)->set_tensor_layout(output_layout);
}

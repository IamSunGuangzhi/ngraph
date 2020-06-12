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

#include "ngraph/pattern/op/any_output.hpp"
#include "ngraph/log.hpp"
#include "ngraph/pattern/matcher.hpp"

using namespace std;
using namespace ngraph;

constexpr NodeTypeInfo pattern::op::AnyOutput::type_info;

const NodeTypeInfo& pattern::op::AnyOutput::get_type_info() const
{
    return type_info;
}

pattern::op::AnyOutput::AnyOutput(const std::shared_ptr<Node>& pattern)
    : Pattern(pattern->outputs())
{
    // size_t i = 0;
    // for (Output<Node> output : pattern->outputs())
    // {
    //     set_output_type(i++, output.get_element_type(), output.get_shape());
    // }
}

bool pattern::op::AnyOutput::match_value(Matcher* matcher,
                                         const Output<Node>& pattern_value,
                                         const Output<Node>& graph_value)
{
    NGRAPH_INFO << graph_value << " $$$$$ " << pattern_value;
    NGRAPH_INFO << *this;
    for (Input<Node> input : inputs())
    {
        Output<Node> output = input.get_source_output();
        NGRAPH_INFO << "compare " << output << " to " << graph_value;
        // if (matcher->match_value(output, graph_value))
        // {
        //     NGRAPH_INFO << "#########";
        //     return true;
        // }
    }
    return false;
}

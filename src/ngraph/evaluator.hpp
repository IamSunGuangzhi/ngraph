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

#pragma once

#include <map>
#include <stack>
#include <utility>

#include "ngraph/node.hpp"
#include "ngraph/shape.hpp"
#include "ngraph/type/element_type_traits.hpp"

namespace ngraph
{
    /// \brief Execute handlers on a subgraph to compute values
    ///
    ///
    template <typename V>
    class Evaluator
    {
    public:
        /// \brief values we compute for outputs
        using value_map = std::map<RawNodeOutput, V>;

        /// \brief Handler for a computation of a value about an op
        ///
        /// A handler is passed a Node* and a vector of computed input values. The handler should
        /// return a vector of computed output values.
        using op_handler = std::function<std::vector<V>(Node* op, std::vector<V>& inputs)>;

        /// \brief Table of ops with handlers
        using op_handler_map = std::map<Node::type_info_t, op_handler>;

        /// \brief construct  handler using the provided op handlers.
        ///
        /// Evaluations share previously computed values so that calls on multiple nodes can share
        /// work. All state is kept in the value map, which is accessible for clearing or seeding
        /// with
        /// Evaluator::get_value_map().
        ///
        /// \param Handlers for ops. Pairs of Node::type_info_t and handler functions.
        Evaluator(const op_handler_map& handlers, value_map& values)
            : m_handlers(handlers)
            , m_value_map(values)
        {
        }

        /// \brief Retrieves the value_map, which holds all Output<Node> value associations.
        value_map& get_value_map() { return m_value_map; }
        const value_map& get_value_map() const { return m_value_map; }
        /// \brief If set, handles all ops
        const op_handler& get_univeral_handler() const { return m_universal_handler; }
        /// \brief If set, handles all ops not in the handlers
        const op_handler& get_default_handler() const { return m_default_handler; }
        /// \brief If set, handles all ops
        void set_univeral_handler(const op_handler& handler) { m_universal_handler = handler; }
        /// \brief If set, handles all ops not in the handlers
        void set_default_handler(const op_handler& handler) { m_default_handler = handler; }
    protected:
        op_handler get_handler(Node* node)
        {
            op_handler handler = m_universal_handler;
            if (!handler)
            {
                auto it = m_handlers.find(node->get_type_info());
                if (it == m_handlers.end())
                {
                    handler = m_default_handler;
                }
                else
                {
                    handler = it->second;
                }
            }
            return handler;
        }

        class Inst;
        using InstPtr = std::unique_ptr<Inst>;
        using InstStack = std::stack<InstPtr>;

        /// \brief Intstructions for evaluations state machine
        class Inst
        {
        protected:
            Inst(Node* node)
                : m_node(node)
            {
            }

        public:
            virtual ~Inst() {}
            virtual void handle(Evaluator& evaluator, InstStack& inst_stack, Node* node) = 0;
            Node* get_node() { return m_node; }
        protected:
            Node* m_node;
        };

        /// \brief Ensure value has been analyzed
        class ValueInst : public Inst
        {
        public:
            ValueInst(const Output<Node>& value)
                : Inst(value.get_node())
                , m_index(value.get_index())
            {
            }

            ValueInst(const RawNodeOutput& value)
                : Inst(value.node)
                , m_index(value.index)
            {
            }

            void handle(Evaluator& evaluator, InstStack& inst_stack, Node* node) override
            {
                // Request to analyze this value if we can
                if (auto handler = evaluator.get_handler(node))
                {
                    // Ensure the inputs are processed and then execute the op handler
                    inst_stack.push(InstPtr(new ExecuteInst(node, handler)));
                    for (auto v : node->input_values())
                    {
                        inst_stack.push(InstPtr(new ValueInst(v)));
                    }
                }
                else
                {
                    // We don't know how to handle this op, so mark the outputs as unknown
                    for (auto output : node->outputs())
                    {
                        evaluator.get_value_map()[output] = V();
                    }
                }
            }

        private:
            int64_t m_index;
        };

        /// \brief All arguments have been handled; execute the node handler
        class ExecuteInst : public Inst
        {
        public:
            ExecuteInst(Node* node, op_handler& handler)
                : Inst(node)
                , m_handler(handler)
            {
            }

            void handle(Evaluator& evaluator, InstStack& inst_stack, Node* node) override
            {
                // Request to execute the handleer. Pass what we know about the inputs to the
                // handler and associate the results with the outputs
                std::vector<V> inputs;
                std::cout << "In evaluator.hpp, map size is " << evaluator.get_value_map().size() <<"\n";
                for (auto v : node->input_values())
                {
                    std::cout << "In evaluator.hpp, handle(), v : " << v <<"\n";
                    auto map_val = evaluator.get_value_map().at(v);
                    inputs.push_back(map_val);
                    //std::cout << "In evaluator.hpp, handle(),  pushed : " << map_val <<"\n";
                }
                std::vector<V> outputs = m_handler(node, inputs);
                for (size_t i = 0; i < outputs.size(); ++i)
                {
                    evaluator.get_value_map()[node->output(i)] = outputs[i];
                }
            }

        private:
            op_handler m_handler;
        };

    public:
        /// \brief Determine information about value
        V evaluate(const Output<Node>& value)
        {
            InstStack inst_stack;
            inst_stack.push(InstPtr(new ValueInst(value)));
            std::cout << "In evaluate, value node = " << value.get_node()->get_name() << ", before while starts\n";
            while (!inst_stack.empty())
            {
                InstPtr inst;
                std::swap(inst_stack.top(), inst);
                inst_stack.pop();
                auto node = inst->get_node();
                std::cout << "\tIn evaluate, value node = " << node->get_name() << ", out0 = " << node->output(0)<<"\n";
                if (m_value_map.find(node->output(0)) != m_value_map.end())
                {
                    std::cout << "\tevaluator.hpp, in if condition, already computed, " << node->output(0)<<"\n";
                    // Already computed
                    //continue;
                }
                std::cout << "\tevaluator.hpp, in else condition, calling inst->handle, " << node->output(0)<<"\n";
                inst->handle(*this, inst_stack, node);
                std::cout <<"\n\n";
            }
            std::cout << "In evaluate, value node, after while loop\n\n\n";
            return m_value_map.at(value);
        }

    protected:
        op_handler m_universal_handler;
        op_handler_map m_handlers;
        op_handler m_default_handler;
        value_map& m_value_map;
    };
}

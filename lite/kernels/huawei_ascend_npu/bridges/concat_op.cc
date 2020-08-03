// Copyright (c) 2019 PaddlePaddle Authors. All Rights Reserved.
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

#include "lite/kernels/huawei_ascend_npu/bridges/graph.h"
#include "lite/kernels/huawei_ascend_npu/bridges/utility.h"
#include "lite/kernels/npu/bridges/registry.h"

namespace paddle {
namespace lite {
namespace subgraph {
namespace huawei_ascend_npu {

int ConcatConverter(void* ctx, OpLite* op, KernelBase* kernel) {
  CHECK(ctx != nullptr);
  CHECK(op != nullptr);
  auto graph = static_cast<Graph*>(ctx);
  auto op_info = op->op_info();
  auto op_type = op_info->Type();
  auto scope = op->scope();
  VLOG(3) << "[NPU] Converting " << op_type << " ... ";

  // Get input and output vars and op attributes
  auto x_names = op_info->Input("X");
  auto axis = op_info->GetAttr<int>("axis");
  auto out_name = op_info->Output("Out").front();
  auto num = x_names.size();

  if (op_info->HasInput("AxisTensor")) {
    // axis node
    auto axis_name = op_info->Input("AxisTensor").front();
    auto axis_tensor = scope->FindMutableTensor(axis_name);
    std::shared_ptr<Node> axis_node = nullptr;
    if (graph->Has(axis_name)) {
      axis_node = graph->Get(axis_name);
    } else {
      axis_node = graph->Add(axis_name, *axis_tensor);
    }
    // concat node
    auto concat_node = graph->Add<ge::op::Concat>(out_name);
    auto concat_op = concat_node->data<ge::op::Concat>();
    // set axis input
    concat_op->set_input_concat_dim(*axis_node->data());
    TENSOR_UPDATE_INPUT(concat_op,
                        concat_dim,
                        ge::FORMAT_NCHW,
                        CvtPrecisionType(axis_node->precision()));
    // set dynamic input
    concat_op->set_attr_N(num);
    concat_op->create_dynamic_input_x(num);
    int idx = 0;
    for (auto& x_name : x_names) {
      auto x = scope->FindMutableTensor(x_name);
      auto x_dims = x->dims();
      std::shared_ptr<Node> x_node = nullptr;
      if (graph->Has(x_name)) {
        x_node = graph->Get(x_name);
      } else {
        x_node = graph->Add(x_name, *x);
      }
      concat_op->set_dynamic_input_x(idx, *x_node->data());
      TENSOR_UPDATE_DYNAMIC_INPUT(concat_op,
                                  x,
                                  idx,
                                  ge::FORMAT_NCHW,
                                  CvtPrecisionType(x_node->precision()));
      idx++;
    }
    TENSOR_UPDATE_OUTPUT(concat_op,
                         y,
                         ge::FORMAT_NCHW,
                         CvtPrecisionType(concat_node->precision()));
  } else {
    auto concat_node = graph->Add<ge::op::ConcatD>(out_name);
    auto concat_op = concat_node->data<ge::op::ConcatD>();
    concat_op->set_attr_concat_dim(axis);
    concat_op->set_attr_N(num);
    concat_op->create_dynamic_input_x(num);
    int idx = 0;
    for (auto& x_name : x_names) {
      auto x = scope->FindMutableTensor(x_name);
      auto x_dims = x->dims();
      std::shared_ptr<Node> x_node = nullptr;
      if (graph->Has(x_name)) {
        x_node = graph->Get(x_name);
      } else {
        x_node = graph->Add(x_name, *x);
      }
      concat_op->set_dynamic_input_x(idx, *x_node->data());
      TENSOR_UPDATE_DYNAMIC_INPUT(concat_op,
                                  x,
                                  idx,
                                  ge::FORMAT_NCHW,
                                  CvtPrecisionType(x_node->precision()));
      idx++;
    }
    TENSOR_UPDATE_OUTPUT(concat_op,
                         y,
                         ge::FORMAT_NCHW,
                         CvtPrecisionType(concat_node->precision()));
  }

  return SUCCESS;
}

}  // namespace huawei_ascend_npu
}  // namespace subgraph
}  // namespace lite
}  // namespace paddle

REGISTER_SUBGRAPH_BRIDGE(
    concat,
    kHuaweiAscendNPU,
    paddle::lite::subgraph::huawei_ascend_npu::ConcatConverter);

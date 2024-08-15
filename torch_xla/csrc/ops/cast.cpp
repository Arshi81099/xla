#include "torch_xla/csrc/ops/cast.h"

#include <torch/csrc/lazy/core/tensor_util.h>

#include "torch_xla/csrc/convert_ops.h"
#include "torch_xla/csrc/dtype.h"
#include "torch_xla/csrc/helpers.h"
#include "torch_xla/csrc/lowering_context.h"
#include "torch_xla/csrc/ops/infer_output_shape.h"
#include "torch_xla/csrc/ops/xla_ops.h"
#include "torch_xla/csrc/reduction.h"
#include "torch_xla/csrc/shape_helper.h"
#include "torch_xla/csrc/tensor_util.h"
#include "torch_xla/csrc/torch_util.h"
#include "xla/primitive_util.h"

namespace torch_xla {
namespace {

xla::Shape NodeOutputShape(const torch::lazy::Value& input,
                           xla::PrimitiveType type) {
  xla::Shape shape = GetXlaShape(input);
  shape.set_element_type(type);
  return shape;
}

}  // namespace

Cast::Cast(const torch::lazy::Value& input, xla::PrimitiveType type)
    : XlaNode(xla_cast, {input}, NodeOutputShape(input, type),
              /*num_outputs=*/1, torch::lazy::MHash(static_cast<int>(type))),
      type_(type) {}

Cast::Cast(const torch::lazy::Value& input, at::ScalarType dtype,
           std::optional<at::ScalarType> stype)
    : XlaNode(xla_cast, {input},
              NodeOutputShape(input,
                              MakeXlaPrimitiveType(dtype, /*device=*/nullptr)),
              /*num_outputs=*/1,
              torch::lazy::MHash(101, static_cast<int>(dtype),
                                 torch::lazy::OptionalOr<int>(stype, -1))),
      type_(MakeXlaPrimitiveType(dtype, /*device=*/nullptr)),
      dtype_(dtype),
      stype_(stype) {}

torch::lazy::NodePtr Cast::Clone(torch::lazy::OpList operands) const {
  return dtype_ ? torch_xla::MakeNode<Cast>(operands.at(0), *dtype_, stype_)
                : torch_xla::MakeNode<Cast>(operands.at(0), type_);
}

XlaOpVector Cast::Lower(LoweringContext* loctx) const {
  xla::XlaOp input = loctx->GetOutputOp(operand(0));
  const xla::Shape& input_shape = ShapeHelper::ShapeOfXlaOp(input);
  xla::PrimitiveType raw_from =
      stype_ ? XlaTypeFromTorchType(*stype_) : input_shape.element_type();
  xla::PrimitiveType raw_to = dtype_ ? XlaTypeFromTorchType(*dtype_) : type_;
  xla::XlaOp output =
      ConvertToRaw(input, input_shape.element_type(), raw_from, type_, raw_to);
  return ReturnOp(output, loctx);
}

std::string Cast::ToString() const {
  std::stringstream ss;
  ss << XlaNode::ToString()
     << ", type=" << xla::primitive_util::LowercasePrimitiveTypeName(type_);
  if (dtype_) {
    ss << ", dtype=" << *dtype_;
  }
  if (stype_) {
    ss << ", stype=" << *stype_;
  }
  return ss.str();
}

}  // namespace torch_xla

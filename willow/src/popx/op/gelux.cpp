// Copyright (c) 2019 Graphcore Ltd. All rights reserved.
#include <iterator>
#include <memory>

#include <popart/error.hpp>
#include <popart/op/gelu.hpp>
#include <popart/op/nll.hpp>
#include <popart/optimizer.hpp>
#include <popart/popx/devicex.hpp>
#include <popart/popx/op/gelux.hpp>
#include <popart/popx/opxmanager.hpp>

#include <popnn/NonLinearity.hpp>
#include <popops/Rearrange.hpp>

namespace popart {
namespace popx {

GeluOpx::GeluOpx(Op *op, Devicex *devicex)
    : ElementWiseUnaryOutplaceOpx(op, devicex, GeluComputex::get()) {
  verifyOp<GeluOp>(op, {Onnx::CustomOperators::Gelu_1});
}

poplar::Tensor GeluComputex::outplace(poplar::program::Sequence &prog,
                                      poplar::Graph &graph,
                                      const poplar::Tensor &tensor,
                                      const std::string &debug_prefix) const {
  auto out_tensor = cloneNcopy(prog, graph, tensor);
  inplace(prog, graph, out_tensor, debug_prefix);
  return out_tensor;
}

void GeluComputex::inplace(poplar::program::Sequence &prog,
                           poplar::Graph &graph,
                           const poplar::Tensor &tensor,
                           const std::string &debug_prefix) const {
  popnn::nonLinearityInPlace(
      graph, popnn::NonLinearityType::GELU, tensor, prog, debug_prefix);
}

GeluInplaceOpx::GeluInplaceOpx(Op *op, Devicex *devicex)
    : ElementWiseUnaryInplaceOpx(op, devicex, GeluComputex::get()) {
  verifyOp<GeluInplaceOp>(op, Onnx::CustomOperators::GeluInplace);
}

GeluGradOpx::GeluGradOpx(Op *op, Devicex *devicex) : Opx(op, devicex) {
  verifyOp<GeluGradOp>(op, Onnx::GradOperators::GeluGrad);
}

void GeluGradOpx::grow(poplar::program::Sequence &prog) const {
  const auto grad  = getInTensor(GeluGradOp::getGradInIndex());
  const auto input = getInTensor(GeluGradOp::getFwdArgInIndex());

  auto gradRearranged = popops::rearrange::regroupIfBeneficial(
      graph(), grad, input, prog, debugPrefix("regroup"));

  auto output = popnn::nonLinearityInputGradient(graph(),
                                                 popnn::NonLinearityType::GELU,
                                                 input,
                                                 gradRearranged,
                                                 prog,
                                                 debugPrefix("gelu_grad"));

  setOutTensor(GeluGradOp::getOutIndex(), output);
}

namespace {
OpxCreator<GeluOpx> geluOpxCreator(Onnx::CustomOperators::Gelu_1);
OpxCreator<GeluInplaceOpx>
    geluInplaceOpxCreator(Onnx::CustomOperators::GeluInplace);
OpxCreator<GeluGradOpx> geluGradOpxCreator(Onnx::GradOperators::GeluGrad);
} // namespace

} // namespace popx
} // namespace popart

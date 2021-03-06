// Copyright (c) 2019 Graphcore Ltd. All rights reserved.
#include <popart/error.hpp>
#include <popart/ir.hpp>
#include <popart/op/lrn.hpp>
#include <popart/popx/devicex.hpp>
#include <popart/popx/op/lrnx.hpp>
#include <popart/popx/opxmanager.hpp>

#include <poplar/Tensor.hpp>
#include <poplin/Norms.hpp>
#include <popops/ElementWise.hpp>
#include <popops/Expr.hpp>

namespace poplar {
using Shape = std::vector<std::size_t>;
}

namespace pe = popops::expr;

namespace popart {
namespace popx {

LRNOpx::LRNOpx(Op *op, Devicex *devicex) : Opx(op, devicex) {
  verifyOp<LRNOp>(op, {Onnx::Operators::LRN_1});
}

namespace {
poplar::Tensor getScale(poplar::Graph &graph,
                        const poplar::Tensor &input,
                        poplar::program::Sequence &prog,
                        const float alpha,
                        const float bias,
                        const int64_t size,
                        const std::string id_str) {
  auto square     = popops::square(graph, input, prog, id_str);
  auto square_sum = graph.clone(square);
  prog.add(poplar::program::Copy(square, square_sum));
  auto channels = input.dim(1);

  auto left  = ((size - 1) / 2);
  auto right = size - left;

  for (auto i = -left; i < right; ++i) {
    // i == 0 added by default,
    if ((i != 0L) &&
        (channels - std::max<int64_t>(0L, i)) - std::max<int64_t>(0L, -i) > 0)
      popops::addInPlace(graph,
                         square_sum.slice(std::max<int64_t>(0L, -i),
                                          channels - std::max<int64_t>(0L, i),
                                          1),
                         square.slice(std::max<int64_t>(0L, i),
                                      channels - std::max<int64_t>(0L, -i),
                                      1),
                         prog,
                         id_str);
  }

  auto scale = popops::map(
      graph,
      pe::Add(pe::Const(bias), pe::Mul(pe::Const(alpha / size), pe::_1)),
      {square_sum},
      prog,
      id_str);

  return scale;
}
} // namespace

void LRNOpx::grow(poplar::program::Sequence &prog) const {
  auto op          = getOp<LRNOp>();
  const auto input = getInTensor(LRNOp::getInIndex());

  auto scale = getScale(graph(),
                        input,
                        prog,
                        op.getAlpha(),
                        op.getBias(),
                        op.getSize(),
                        debugPrefix("scale"));

  auto output =
      popops::map(graph(),
                  pe::Mul(pe::_1, pe::Pow(pe::_2, pe::Const(-op.getBeta()))),
                  {input, scale},
                  prog,
                  debugPrefix("output"));

  setOutTensor(LRNOp::getOutIndex(), output);
}

LRNGradOpx::LRNGradOpx(Op *op, Devicex *devicex) : Opx(op, devicex) {
  verifyOp<LRNGradOp>(op, Onnx::GradOperators::LRNGrad);
}

void LRNGradOpx::grow(poplar::program::Sequence &prog) const {
  auto op              = getOp<LRNGradOp>();
  const auto input     = getInTensor(LRNGradOp::getInIndex());
  const auto fwd_input = getInTensor(LRNGradOp::getFwdInInIndex());

  auto scale = getScale(graph(),
                        fwd_input,
                        prog,
                        op.getAlpha(),
                        op.getBias(),
                        op.getSize(),
                        debugPrefix("scale"));

  auto output = popops::map(
      graph(),
      pe::Mul(
          pe::_1,
          pe::Sub(
              pe::Pow(pe::_3, pe::Const(-op.getBeta())),
              pe::Mul(pe::Mul(pe::Square(pe::_2),
                              pe::Const(2.f * op.getAlpha() * op.getBeta())),
                      pe::Pow(pe::_3, pe::Const(-op.getBeta() - 1.f))))),
      {input, fwd_input, scale},
      prog,
      debugPrefix("grad"));

  setOutTensor(LRNGradOp::getOutIndex(), output);
}

namespace {
OpxCreator<LRNOpx> batchNormOpxCreator({Onnx::Operators::LRN_1});
OpxCreator<LRNGradOpx> batchNormGradOpxCreator(Onnx::GradOperators::LRNGrad);
} // namespace

} // namespace popx
} // namespace popart

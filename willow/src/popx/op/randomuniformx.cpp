// Copyright (c) 2020 Graphcore Ltd. All rights reserved.
#include <poprand/RandomGen.hpp>
#include <popart/op/randomuniform.hpp>
#include <popart/popx/op/randomuniformx.hpp>

namespace popart {
namespace popx {

RandomUniformOpx::RandomUniformOpx(Op *op, Devicex *devicex)
    : Opx(op, devicex) {
  verifyOp<RandomUniformOp>(op, Onnx::Operators::RandomUniform_1);
}

void RandomUniformOpx::grow(poplar::program::Sequence &prog) const {
  auto &op        = getOp<RandomUniformOp>();
  auto outputInfo = op.outInfo(op.getOutIndex());
  auto shape      = vXtoY<int64_t, std::size_t>(outputInfo.shape());
  auto poplarType = popType(op.outInfo(op.getOutIndex()));

  auto refTensor = graph().addVariable(
      poplarType, shape, poplar::VariableMappingMethod::LINEAR, "refTensor");

  auto output = poprand::uniform(graph(),
                                 &getInTensor(op.getSeedInIndex()),
                                 op.getSeedModifier(),
                                 refTensor,
                                 poplarType,
                                 op.getLow(),
                                 op.getHigh(),
                                 prog);

  setOutTensor(op.getOutIndex(), output);
}

namespace {
OpxCreator<RandomUniformOpx>
    randomUniformOpxCreator(Onnx::Operators::RandomUniform_1);
}

} // namespace popx
} // namespace popart

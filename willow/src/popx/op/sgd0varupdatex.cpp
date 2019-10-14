#include <popops/Collectives.hpp>
#include <popops/ElementWise.hpp>
#include <popops/ScaledAdd.hpp>
#include <popart/error.hpp>
#include <popart/ir.hpp>
#include <popart/op/sgd0varupdate.hpp>
#include <popart/op/varupdate.hpp>
#include <popart/popx/devicex.hpp>
#include <popart/popx/op/sgd0varupdatex.hpp>
#include <popart/popx/opxmanager.hpp>

namespace pe = popops::expr;

namespace popart {
namespace popx {

SGD0VarUpdateOpx::SGD0VarUpdateOpx(Op *op, Devicex *devicex)
    : VarUpdateOpx(op, devicex) {
  verifyOp<SGD0VarUpdateOp>(op, Onnx::CustomOperators::SGD0VarUpdate);
}

void SGD0VarUpdateOpx::grow(poplar::program::Sequence &prog) const {

  // Weight update (matching pytorch implementation)
  //  w <- w * (1 - lr * wd) - (lr/ls) * weight_gradient
  //
  // lr = learning rate
  // ls = loss scaling
  // wd = weight decay
  //
  // This is expressed as
  //
  // w <- w * weightDecayScaleFactor - scaledLearningRate * weight_gradient
  //
  // The (1 - lr * wd) and (lr/ls) calculations are done in SGD::setTensorData

  auto vu_op = getOp<SGD0VarUpdateOp>();

  // (1) update weights with weight decay

  // non-const weight decay scale factor
  if (!vu_op.initWdsf0.isConst()) {

    popops::mapInPlace(graph(),
                       pe::Mul(pe::_1, pe::_2),
                       {getInTensor(SGD0VarUpdateOp::getVarToUpdateInIndex()),
                        getInTensor(SGD0VarUpdateOp::getWdsf0InIndex())},
                       prog,
                       debugPrefix("nonConstWeightDecay"));
  }

  // const weight decay scale factor
  else {
    float scaleFactor = vu_op.initWdsf0.val();
    if (scaleFactor != 1.0f) {
      popops::mapInPlace(
          graph(),
          pe::Mul(pe::_1, pe::Const(scaleFactor)),
          {getInTensor(SGD0VarUpdateOp::getVarToUpdateInIndex())},
          prog,
          debugPrefix("constWeightDecay"));
    }
  }

  // (2) subtract scaled gradients
  poplar::Tensor weightDeltas =
      getInTensor(SGD0VarUpdateOp::getUpdaterInIndex());

  // TODO: where does this go when there is replication?
  if (dv_p->getReplicationFactor() > 1) {
    weightDeltas =
        popops::replicatedAllReduce(graph(),
                                    weightDeltas,
                                    popops::Operation::ADD,
                                    prog,
                                    debugPrefix("allReduce_Add"),
                                    {{"useReplicatedImplementation", "true"}});
  }

  // non-const scaled learning rate case
  if (!vu_op.initSlr0.isConst()) {
    popops::scaledSubtractFrom(
        graph(),
        getInTensor(SGD0VarUpdateOp::getVarToUpdateInIndex()), // weights
        weightDeltas,                                          // weightDeltas
        getInTensor(SGD0VarUpdateOp::getSlr0InIndex()),
        prog,
        debugPrefix("nonConstScaledSubtract"));
  }

  // const scaled learning rate case
  else {
    popops::scaledSubtractFrom(
        graph(),
        getInTensor(vu_op.getVarToUpdateInIndex()), // weights
        weightDeltas,                               // weightDeltas
        vu_op.initSlr0.val(),
        prog,
        debugPrefix("scaledSubtract"));
  }

  // output is a reference to the updated input
  setOutTensor(SGD0VarUpdateOp::getUpdatedVarOutIndex(),
               getInTensor(SGD0VarUpdateOp::getVarToUpdateInIndex()));
}

namespace {
OpxCreator<SGD0VarUpdateOpx>
    sgd0VarUpdateOpxCreator(Onnx::CustomOperators::SGD0VarUpdate);
} // namespace

} // namespace popx
} // namespace popart
#include <willow/conv.hpp>
#include <willow/error.hpp>
#include <willow/popx/convx.hpp>
#include <willow/popx/devicex.hpp>
#include <willow/tensor.hpp>
#include <willow/util.hpp>

namespace willow {
namespace popx {

const poplin::ConvParams &ConvOpx::getParams() const { return params; }

std::vector<TensorId> ConvOpx::mustExistBeforeCreate(int) const {
  // both creating weights and input are done
  // without requiring the pre-existance of any
  // other poplar::Tensor
  return {};
}

ConvOpx::ConvOpx(Op *op, Devicex *devicex) : Opx(op, devicex) {
  if (op->opType != OpType::CONV) {
    throw error("cannot create ConvOpx from " + op->op_type());
  }

  ConvOp *cOp = getConvOp();
  if (cOp->dataIn()->info.rank() != 4 || cOp->weightsIn()->info.rank() != 4) {
    throw error("Poplar only supports convolutions with 2 spatial dimensions");
  }

  std::vector<unsigned> zeros(cOp->nSpatialDims, 0);
  std::vector<bool> falses(cOp->nSpatialDims, false);
  std::vector<unsigned> ones(cOp->nSpatialDims, 1);

  // we assume that the output type is the same as the input
  auto popOutType = popType(cOp->dataIn()->info);

  params = poplin::ConvParams(popOutType,          // dType,
                              cOp->batchSize,      // batchSize,
                              cOp->spatialD_szt(), // inputFieldShape,
                              cOp->spatialK_szt(), // kernelShape,

                              cOp->nInChans,       // inputChannels,
                              cOp->getNOutChans(), // outputChannels,
                              cOp->group,          // numConvGroups,

                              zeros,                // inputTruncationLower,
                              zeros,                // inputTruncationUpper,
                              ones,                 // inputDilation,
                              cOp->lowerPads_u32(), // inputPaddingLower,
                              cOp->upperPads_u32(), // inputPaddingUpper
                              falses,               // flipInput,

                              zeros,                // kernelTruncationLower,
                              zeros,                // kernelTruncationUpper,
                              cOp->dilations_u32(), // kernelDilation,
                              zeros,                // kernelPaddingLower,
                              zeros,                // kernelPaddingUpper,
                              falses,               // flipKernel,

                              zeros,              // outputTruncationLower,
                              zeros,              // outputTruncationUpper,
                              cOp->strides_u32(), // stride,
                              zeros,              // outputPaddingLower,
                              zeros               // outputPaddingUpper.
  );
}

bool ConvOpx::createsEquiv(int ind0, Opx *opx1, int ind1) const {
  // if opx1 is not a ConvOpx, it does not create the same poplar::Tensor
  if (opx1->getOp()->opType != OpType::CONV) {
    return false;
  }

  // if opx1 (which we now know is ConvOpx) would create the tensor at
  // a different input index, creation is not equivalent
  if (ind0 != ind1) {
    return false;
  }

  // finally, check that the convolution parameters are the same
  ConvOpx *rhs = dynamic_cast<ConvOpx *>(opx1);
  if (getParams() != rhs->getParams()) {
    return false;
  }

  return true;
}

ConvOp *ConvOpx::getConvOp() const { return dynamic_cast<ConvOp *>(getOp()); }

bool ConvOpx::canCreateInput(int) const { return true; }

poplar::Tensor ConvOpx::createInput(int index) const {

  if (index == getConvOp()->weightsInIndex()) {
    return poplin::createWeights(
        getDevx()->graph(),                                      // graph
        params,                                                  // params
        getOp()->str(),                                          // name
        enigma::toPoplibsConvOptions(getDevx()->fwdConvOptions), // options
        &getDevx()->convCache                                    // cache
    );
  } else {
    throw error("conv opx cannot create tensor at this index yet");
  }
}

ConvDataGradOpx::ConvDataGradOpx(Op *op, Devicex *devicex) : Opx(op, devicex) {
  if (op->opType != OpType::CONVDATAGRAD) {
    throw error("cannot create ConvDataGradOpx from " + op->op_type());
  }
}

ConvDataGradOp *ConvDataGradOpx::getConvDataGradOp() const {
  return dynamic_cast<ConvDataGradOp *>(getOp());
}

ConvWeightsGradOpx::ConvWeightsGradOpx(Op *op, Devicex *devicex)
    : Opx(op, devicex) {
  if (op->opType != OpType::CONVWEIGHTSGRAD) {
    throw error("cannot create ConvWeightsGradOpx from " + op->op_type());
  }
}

ConvWeightsGradOp *ConvWeightsGradOpx::getConvWeightsGradOp() const {
  return dynamic_cast<ConvWeightsGradOp *>(getOp());
}

} // namespace popx
} // namespace willow
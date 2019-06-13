#include <poponnx/error.hpp>
#include <poponnx/op/reshape.hpp>
#include <poponnx/popx/op/reshapex.hpp>
#include <poponnx/popx/opxmanager.hpp>
#include <poponnx/tensor.hpp>

namespace poponnx {
namespace popx {

// Test note : scale by 1.0001 in grad op makes the test fail. Good.
void ReshapeOpx::grow(poplar::program::Sequence &prog) const {
  // not in-place, so cloning input
  auto outTensor = cloneNcopy(prog, getInTensor(ReshapeOp::getInIndex()));
  outTensor = outTensor.reshape(outInfo(ReshapeOp::getOutIndex()).shape_szt());
  setOutTensor(ReshapeOp::getOutIndex(), outTensor);
}

void ReshapeInplaceOpx::grow(poplar::program::Sequence &) const {
  auto outTensor = getInTensor(ReshapeOp::getInIndex());
  outTensor = outTensor.reshape(outInfo(ReshapeOp::getOutIndex()).shape_szt());
  setOutTensor(ReshapeOp::getOutIndex(), outTensor);
}

ReshapeBaseOpx::ReshapeBaseOpx(Op *op, Devicex *devicex) : Opx(op, devicex) {
  verifyOp<ReshapeBaseOp>(op);
}

ReshapeOpx::ReshapeOpx(Op *op, Devicex *devicex) : ReshapeBaseOpx(op, devicex) {
  verifyOp<ReshapeOp>(op);
}

ReshapeInplaceOpx::ReshapeInplaceOpx(Op *op, Devicex *devicex)
    : ReshapeBaseOpx(op, devicex) {
  verifyOp<ReshapeInplaceOp>(op);
}

InputCreatorType ReshapeBaseOpx::getInputCreatorType(InIndex) const {
  return InputCreatorType::CANUNWIND;
}

poplar::Tensor ReshapeBaseOpx::unwindTensorLayout(poplar::Tensor tensor,
                                                  InIndex,
                                                  OutIndex) const {
  return tensor.reshape(inInfo(ReshapeOp::getInIndex()).shape_szt());
}

ReshapeGradOpx::ReshapeGradOpx(Op *op, Devicex *devicex)
    : ReshapeOpx(op, devicex) {
  verifyOp<ReshapeGradOp>(op, Onnx::GradOperators::ReshapeGrad);
}

namespace {
OpxCreator<ReshapeOpx> reshapeOpxCreator(Onnx::Operators::Reshape_5);
OpxCreator<ReshapeInplaceOpx>
    reshapeInplaceOpxCreator(Onnx::CustomOperators::ReshapeInplace);
OpxCreator<ReshapeGradOpx>
    reshapeGradOpxCreator(Onnx::GradOperators::ReshapeGrad);
} // namespace

} // namespace popx
} // namespace poponnx

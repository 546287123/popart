#include <memory>
#include <poponnx/graph.hpp>
#include <poponnx/op/mul.hpp>
#include <poponnx/op/pow.hpp>
#include <poponnx/op/reducesum.hpp>
#include <poponnx/op/subtract.hpp>
#include <poponnx/optimizer.hpp>
#include <poponnx/patterns/powarg0gradoppattern.hpp>
#include <poponnx/popx/devicex.hpp>
#include <poponnx/tensor.hpp>
#include <poponnx/tensorinfo.hpp>

namespace poponnx {

bool PowArg0GradOpPattern::matches(Op *op) const {
  return op->isConvertibleTo<PowArg0GradOp>();
}

std::vector<const Tensor *> PowArg0GradOpPattern::touches(Op *) const {
  return {};
}

// grad_out = grad_in *arg1 * arg0 ^(arg1 - 1)
bool PowArg0GradOpPattern::apply(Op *op) const {
  auto grad_in  = op->inTensor(PowArg0GradOp::getGradInIndex());
  auto fwd_in0  = op->inTensor(PowArg0GradOp::getFwdArg0InIndex());
  auto fwd_in1  = op->inTensor(PowArg0GradOp::getFwdArg1InIndex());
  auto grad_out = op->outTensor(PowArg0GradOp::getOutIndex());

  // we assume this dynamic_cast call has been confirmed
  // to be valid via a previous call to PowArg0GradOpPattern::matches
  auto axes = dynamic_cast<PowArg0GradOp *>(op)->getReductionAxes();

  // Create a 1-dim constant tensor of value 1.0f with same type as fwd_in1
  TensorInfo resultInfo(fwd_in1->info.dataType(), {1});
  std::vector<float> resultData(1, 1.0f);
  op->getGraph().getTensors().addConstInit(
      "ones", resultInfo, reinterpret_cast<void *>(resultData.data()));

  // create the new ops
  auto sub    = makeReplacementOpInIr(Onnx::AiOnnx::OpSet9::Sub, op);
  auto pow    = makeReplacementOpInIr(Onnx::AiOnnx::OpSet9::Pow, op);
  auto mul_1  = makeReplacementOpInIr(Onnx::AiOnnx::OpSet9::Mul, op);
  auto mul_2  = makeReplacementOpInIr(Onnx::AiOnnx::OpSet9::Mul, op);
  auto reduce = dynamic_cast<ReduceSumOp *>(
      makeReplacementOpInIr(Onnx::AiOnnx::OpSet9::ReduceSum, op));
  reduce->setAxes(axes);
  // do not keep reduced dims
  reduce->setKeepDims(0l);

  // Remove the PowArg0GradOp
  op->disconnectAllInputs();
  op->disconnectAllOutputs();
  op->getGraph().eraseOp(op->id);

  // Connect up the new ops
  sub->connectInTensor(0, fwd_in1->id);
  sub->connectInTensor(1, "ones");
  sub->createAndConnectOutTensor(0, createIntermediateTensorId(grad_in->id));
  sub->outInfo(0) = npOut(sub->inInfo(0), sub->inInfo(1));

  pow->connectInTensor(0, fwd_in0->id);
  pow->connectInTensor(1, sub->outTensor(0)->id);
  pow->createAndConnectOutTensor(0, createIntermediateTensorId(grad_in->id));
  pow->outInfo(0) = npOut(pow->inInfo(0), pow->inInfo(1));

  mul_1->connectInTensor(0, fwd_in1->id);
  mul_1->connectInTensor(1, pow->outTensor(0)->id);
  mul_1->createAndConnectOutTensor(0, createIntermediateTensorId(grad_in->id));
  mul_1->outInfo(0) = npOut(mul_1->inInfo(0), mul_1->inInfo(1));

  mul_2->connectInTensor(0, grad_in->id);
  mul_2->connectInTensor(1, mul_1->outTensor(0)->id);
  mul_2->createAndConnectOutTensor(0, createIntermediateTensorId(grad_in->id));
  mul_2->outInfo(0) = npOut(mul_2->inInfo(0), mul_2->inInfo(1));

  reduce->connectInTensor(0, mul_2->outTensor(0)->id);
  reduce->connectOutTensor(0, grad_out->id);

  return true;
}

namespace {
static PatternCreator<PowArg0GradOpPattern>
    PowArg0GradOpPattern(PreAliasPatternType::POWARG0GRADOP, "PowArg0GradOp");
}

} // namespace poponnx
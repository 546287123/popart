#include <poponnx/ir.hpp>
#include <poponnx/op/nll.hpp>
#include <poponnx/op/softmax.hpp>
#include <poponnx/patterns/softmaxgraddirect.hpp>

namespace poponnx {

// NLLGRAD (0) -> x -> SOFTMAXGRAD.
const OperatorIdentifier &SoftmaxGradDirect::get0() const {
  return Onnx::CustomGradOperators::NllGrad;
}

// NLLGRAD -> x -> SOFTMAXGRAD (1).
const OperatorIdentifier &SoftmaxGradDirect::get1() const {
  return Onnx::GradOperators::SoftmaxGrad;
}

OpId SoftmaxGradDirect::moveMergedIntoIr(Op *opRoot) const {
  // The root of the pattern is an NLLGrad,
  // we need to move from it to the SoftmaxOp
  Ir &ir      = opRoot->getIr();
  Op *nllgrad = opRoot;

  return ir.moveIntoIr(std::unique_ptr<Op>(new SoftmaxGradDirectOp(
      dynamic_cast<NllGradOp *>(nllgrad)->nlll(), nllgrad->getSettings())));
}

namespace {
static PatternCreator<SoftmaxGradDirect>
    PreUniReplPattern(PreAliasPatternType::SOFTMAXGRADDIRECT,
                      "SoftmaxGradDirect");
}

} // namespace poponnx

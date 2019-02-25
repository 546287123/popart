#include <poponnx/error.hpp>
#include <poponnx/op/sign.hpp>
#include <poponnx/popx/devicex.hpp>
#include <poponnx/popx/op/signx.hpp>
#include <poponnx/popx/opxmanager.hpp>

#include <popops/ElementWise.hpp>
#include <popops/Expr.hpp>
#include <popops/Zero.hpp>

namespace pe = popops::expr;

namespace poponnx {
namespace popx {

SignOpx::SignOpx(Op *op, Devicex *devicex) : ElementWiseUnaryOpx(op, devicex) {
  verifyOp<SignOp>(op, {Onnx::Operators::Sign_9});
}

void SignOpx::grow(poplar::program::Sequence &prog) const {

  insert(outId(SignOp::getOutIndex()),
         popops::map(graph(),
                     popops::expr::UnaryOpType::SIGNUM,
                     get(inId(SignOp::getInIndex())),
                     prog,
                     idStr()));
}

SignGradOpx::SignGradOpx(Op *op, Devicex *devicex) : Opx(op, devicex) {
  verifyOp<SignGradOp>(op, Onnx::GradOperators::SignGrad);
}

void SignGradOpx::grow(poplar::program::Sequence &) const {

  auto outTensor =
      graph().addConstant(popType(outInfo(SignGradOp::getOutIndex())),
                          outInfo(SignGradOp::getOutIndex()).shape_szt(),
                          0,
                          idStr());

  insert(outId(SignGradOp::getOutIndex()), outTensor);
}

namespace {
OpxCreator<SignOpx> signOpxCreator(Onnx::Operators::Sign_9);
OpxCreator<SignGradOpx> signGradOpxCreator(Onnx::GradOperators::SignGrad);
} // namespace

} // namespace popx
} // namespace poponnx

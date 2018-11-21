#ifndef GUARD_NEURALNET_NEGATE_HPP
#define GUARD_NEURALNET_NEGATE_HPP

#include <poponnx/ir.hpp>

namespace willow {

class NegateOp : public Op {
public:
  NegateOp(const OpConstructorBundle &);
  NegateOp(const onnx::NodeProto &node, Ir *pir);
  std::unique_ptr<Op> clone() const override;
  std::vector<std::unique_ptr<Op>> getGradOps() final;
  void setup() override;
};

class NegateGradOp : public NegateOp {
public:
  NegateGradOp(NegateOp *fwdOp);
  std::unique_ptr<Op> clone() const final;

  const std::vector<GradInOutMapper> &gradInputInfo() const final;
  const std::map<int, int> &gradOutToNonGradIn() const final;
};

} // namespace willow

#endif

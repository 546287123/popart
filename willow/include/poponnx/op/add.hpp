#ifndef GUARD_NEURALNET_ADD_HPP
#define GUARD_NEURALNET_ADD_HPP

#include <poponnx/ir.hpp>
#include <poponnx/op/reducesum.hpp>

namespace willow {

class AddOp : public Op {
public:
  AddOp(const onnx::NodeProto &node, Ir *pir);
  std::unique_ptr<Op> clone() const final;
  std::vector<std::unique_ptr<Op>> getGradOps() final;
  void setup() final;

  // Current implementation places arg0 input at index 0, and arg1 input
  // at index 1.
  static int arg0Index();
  static int arg1Index();
};

class AddArg0GradOp : public ReduceSumOp {
public:
  AddArg0GradOp(AddOp *, const std::vector<int64_t> &axes);
  const std::vector<GradInOutMapper> &gradInputInfo() const final;
  const std::map<int, int> &gradOutToNonGradIn() const final;
  void setup() final;

private:
  TensorInfo forward_op_arg_info;
};

class AddArg1GradOp : public ReduceSumOp {
public:
  AddArg1GradOp(AddOp *, const std::vector<int64_t> &axes);
  const std::vector<GradInOutMapper> &gradInputInfo() const final;
  const std::map<int, int> &gradOutToNonGradIn() const final;
  void setup() final;

private:
  TensorInfo forward_op_arg_info;
};

} // namespace willow

#endif
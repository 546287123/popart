#ifndef GUARD_NEURALNET_REDUCEPROD_HPP
#define GUARD_NEURALNET_REDUCEPROD_HPP

#include <poponnx/op.hpp>
#include <poponnx/op/reduce.hpp>

namespace poponnx {

class ReduceProdOp : public ReduceOp {
public:
  ReduceProdOp(const OperatorIdentifier &_opid,
               const std::vector<int64_t> &axes,
               const int64_t keepdims,
               const Op::Settings &settings);
  std::unique_ptr<Op> clone() const final;
  std::vector<std::unique_ptr<Op>> getGradOps() final;
};

class ReduceProdGradOp : public ReduceGradOp {
public:
  ReduceProdGradOp(const ReduceProdOp &fwdOp, const Shape &backward_shape);
  std::unique_ptr<Op> clone() const final;
  const std::vector<GradInOutMapper> &gradInputInfo() const final;
  static InIndex getFwdInInIndex() { return 1; }
  const std::vector<int64_t> &getAxes() const;

private:
  std::vector<int64_t> axes;
};

} // namespace poponnx

#endif

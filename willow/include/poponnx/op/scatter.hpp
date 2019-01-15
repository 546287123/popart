#ifndef GUARD_NEURALNET_SCATTER_HPP
#define GUARD_NEURALNET_SCATTER_HPP

#include <poponnx/op.hpp>

namespace poponnx {

class ScatterOp : public Op {
public:
  ScatterOp(const OperatorIdentifier &_opid,
            Ir *_ir,
            const std::string &name = "",
            const Attributes &_attr = {});

  std::unique_ptr<Op> clone() const final;
  std::vector<std::unique_ptr<Op>> getGradOps() final;
  void setup() final;

  // Which axis to scatter on.
  int64_t getAxis() const;

  static InIndex dataInIndex() { return 0; }
  static InIndex indicesInIndex() { return 1; }
  static InIndex updatesInIndex() { return 2; }
  static InIndex outIndex() { return 0; }

private:
  int64_t axis = 0;
};

// This is a scatter of zeros into the grad input. This is because these
// elements are replaced in the forward op by the update input tensor.
class ScatterDataGradOp : public Op {
public:
  ScatterDataGradOp(ScatterOp *op, int64_t axis);

  std::unique_ptr<Op> clone() const final;
  const std::vector<GradInOutMapper> &gradInputInfo() const final;
  const std::map<int, int> &gradOutToNonGradIn() const final;
  void setup() final;

  // Which axis the forward op scattered on.
  int64_t getAxis() const;

  static InIndex gradInIndex() { return 0; }
  static InIndex indicesInIndex() { return 1; }
  static InIndex gradOutIndex() { return 0; }

private:
  int64_t axis;
};

// This is a gather of elements from the grad input based on the indices used in
// the forward op.
class ScatterUpdateGradOp : public Op {
public:
  ScatterUpdateGradOp(ScatterOp *op, int64_t axis);

  std::unique_ptr<Op> clone() const final;
  const std::vector<GradInOutMapper> &gradInputInfo() const final;
  const std::map<int, int> &gradOutToNonGradIn() const final;
  void setup() final;

  // Which axis the forward op scattered on.
  int64_t getAxis() const;

  static InIndex gradInIndex() { return 0; }
  static InIndex indicesInIndex() { return 1; }
  static InIndex gradOutIndex() { return 0; }

private:
  int64_t axis;
};

} // namespace poponnx

#endif
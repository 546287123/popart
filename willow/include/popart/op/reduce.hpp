// Copyright (c) 2018 Graphcore Ltd. All rights reserved.
#ifndef GUARD_NEURALNET_REDUCE_HPP
#define GUARD_NEURALNET_REDUCE_HPP

#include <popart/op.hpp>
#include <popart/vendored/optional.hpp>

namespace popart {

class ReduceOp : public Op {
public:
  ReduceOp(const OperatorIdentifier &_opid,
           const nonstd::optional<std::vector<int64_t>> &axes,
           const int64_t keepdims,
           const Op::Settings &settings);

  std::unique_ptr<Op> clone() const override;
  void setup() override;

  // A list of integers, along which to reduce. These axes will either be
  // removed or have size 1, depending on the value of getKeepDims.
  const std::vector<int64_t> &getAxes() const;

  // Keep the reduced dimensions or not. A value of `true` means this op will
  // preserve the rank of the input tensor, inserting 1 at reduced axes
  bool getKeepDims() const;

  void setAxes(std::vector<int64_t> value);
  void setKeepDims(int64_t value);

  static InIndex getInIndex() { return 0; }
  static OutIndex getOutIndex() { return 0; }

  void appendOutlineAttributes(OpSerialiserBase &) const override;

  bool canBeReplacedByIdentity() override;

  float getSubgraphValue() const final { return getLowSubgraphValue(); }

  const Shape &backwardShape() const;

protected:
  // The input shape, with '1' inserted in reduction axes.
  // This is the same as the output shape if keepdims is true.
  Shape backward_shape;
  std::vector<int64_t> axes;
  int64_t keepdims;

private:
  // Axes are passed in with nonstd::optional and hence may not
  // be set at all at time of construction. Because this does
  // not get resolved until the call to setup() the ReduceOp will
  // need to remember if default arguments were used. It does
  // this in has_default_axes.
  bool has_default_axes;
};

class ReduceGradOp : public Op {
public:
  ReduceGradOp(const AiGraphcoreOpIdV1 &opid,
               const ReduceOp &fwdOp,
               const Shape &backward_shape);
  std::unique_ptr<Op> clone() const override;
  void setup() override;

  // A list of integers, along which have been reduced.
  const std::vector<int64_t> &getAxes() const;

  const std::vector<GradInOutMapper> &gradInputInfo() const override;
  const std::map<int, int> &gradOutToNonGradIn() const final;
  const Shape &backwardShape() const;

  static InIndex getInIndex() { return 0; }
  static OutIndex getOutIndex() { return 0; }

  float getSubgraphValue() const final { return getLowSubgraphValue(); }

protected:
  TensorInfo outputTensorInfo;
  // Copied from constructing ReduceOp. In this context, it is
  // the shape of this grad Op's input, but with '1's inserted where
  // broadcasts are required to obtain the gradient of the fwd Op's input
  const Shape backward_shape;
  std::vector<int64_t> axes;
};

} // namespace popart

#endif

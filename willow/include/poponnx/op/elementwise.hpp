#ifndef GUARD_NEURALNET_ELEMENTWISEUNARY_HPP
#define GUARD_NEURALNET_ELEMENTWISEUNARY_HPP

#include <poponnx/op.hpp>

namespace poponnx {

// Base class for elementwise unary operations
class ElementWiseUnaryOp : public Op {
public:
  ElementWiseUnaryOp(const OperatorIdentifier &_opid,
                     const Op::Settings &settings);
  void setup() final;

  static InIndex getInIndex() { return 0; }
  static OutIndex getOutIndex() { return 0; }

  // Making this function override and not final, as there
  // may be a more / less expensive to compute non-linearity.
  float getSubgraphValue() const override { return 0.1f; }

  // The default for ElementWise Ops is that they can
  // appear in sub-graphs.
  bool supportsCaching() const override { return true; }
};

class ElementWiseInplaceUnaryOp : public ElementWiseUnaryOp {
public:
  ElementWiseInplaceUnaryOp(const OperatorIdentifier &_opid,
                            const Op::Settings &settings)
      : ElementWiseUnaryOp(_opid, settings) {}

  view::Region modifies(InIndex index) const final { return uses(index); }
  view::Region aliases(InIndex index) const final { return uses(index); }
  // "uses" is still the full region
  // "fwdRegMap" is still the identity
  // "bwdRegMap" is still the identity
};

// Base class for gradients of element-wise, non-linear, unary operations
// Non-linear elementwise ops gradients take both the input, and gradient
// output of the corresponding forward operation as inputs.
class ElementWiseNonLinearUnaryGradOp : public Op {
public:
  ElementWiseNonLinearUnaryGradOp(const OperatorIdentifier &_opid,
                                  const ElementWiseUnaryOp &fwdOp);
  const std::vector<GradInOutMapper> &gradInputInfo() const final;
  const std::map<int, int> &gradOutToNonGradIn() const final;
  void setup() final;

  // This grad op takes 2 inputs,
  // 1) the gradient of the output of the corresponding forward op, and
  // 2) the input of the forward op.
  // The indices at which these two tensors are inputs to this grad op are
  // getGradInIndex and getFwdArgInIndex respectively.
  static InIndex getGradInIndex() { return 0; }
  static InIndex getFwdArgInIndex() { return 1; }
  static OutIndex getOutIndex() { return 0; }

  float getSubgraphValue() const final { return 0.1f; }
};

// Base class for elementwise binary operations
class ElementWiseBinaryOp : public Op {
public:
  ElementWiseBinaryOp(const OperatorIdentifier &_opid,
                      const Op::Settings &settings);
  void setup() final;

  // Current implementation places arg0 input at index 0, and arg1 input
  // at index 1.
  static InIndex getArg0InIndex() { return 0; }
  static InIndex getArg1InIndex() { return 1; }
  static OutIndex getOutIndex() { return 0; }

  float getSubgraphValue() const final { return 0.1f; }
};

} // namespace poponnx

#endif

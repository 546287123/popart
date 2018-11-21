#ifndef GUARD_NEURALNET_SUBTRACT_ARG1_GRAD_PATTERN_HPP
#define GUARD_NEURALNET_SUBTRACT_ARG1_GRAD_PATTERN_HPP

#include <poponnx/patterns.hpp>

namespace willow {

// Replace a SubtractArg1GradOp with a negate followed by a reducesum
class SubtractArg1GradOpPattern : public Pattern {
public:
  // Does op at the root of the
  // pattern make a match?
  bool matches(Op *) const override;
  // If this Pattern were to be applied at op, which
  // Tensors in the subgraph centered (rooted) on op
  // would be touched?
  std::vector<const Tensor *> touches(Op *) const override;
  // apply the pattern,
  // changes the graph of the op
  void apply(Op *) const override;
};

} // namespace willow

#endif

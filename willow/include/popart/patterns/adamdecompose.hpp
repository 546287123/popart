// Copyright (c) 2020 Graphcore Ltd. All rights reserved.
#ifndef GUARD_NEURALNET_ADAMDECOMPOSE_PATTERN_HPP
#define GUARD_NEURALNET_ADAMDECOMPOSE_PATTERN_HPP

#include <popart/patterns/patterns.hpp>

namespace popart {

class AdamDecompose : public PreAliasPattern {
public:
  bool matches(Op *) const final;
  std::vector<const Tensor *> touches(Op *) const final;
  bool apply(Op *) const final;
};

} // namespace popart

#endif

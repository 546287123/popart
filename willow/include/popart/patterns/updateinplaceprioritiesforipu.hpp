// Copyright (c) 2018 Graphcore Ltd. All rights reserved.
#ifndef GUARD_NEURALNET_UPDATE_INPLACE_PRIORITIES_FOR_IPU_PATTERN_HPP
#define GUARD_NEURALNET_UPDATE_INPLACE_PRIORITIES_FOR_IPU_PATTERN_HPP

namespace popart {

class AddOp;

class UpdateInplacePrioritiesForIpu : public Pattern {
public:
  void apply(Op *) const;

private:
  void applyImpl(AddOp &) const;
};

} // namespace popart

#endif

// Copyright (c) 2020 Graphcore Ltd. All rights reserved.
#ifndef GUARD_NEURALNET_ACCUMULATEX_HPP
#define GUARD_NEURALNET_ACCUMULATEX_HPP

#include <popart/names.hpp>
#include <popart/popx/op/varupdatex.hpp>

namespace popart {
namespace popx {

class AccumulateOpx : public VarUpdateOpx {
public:
  AccumulateOpx(Op *, Devicex *);
  void grow(poplar::program::Sequence &) const final;

  // can create the accumulator input Tensor (@Var index)
  // from the weight gradient tensor (@Updater index)
  poplar::Tensor createInput(InIndex, const std::string &name) const final;
  InputCreatorType getInputCreatorType(InIndex) const final;
  std::vector<TensorId> mustExistBeforeCreate(InIndex) const final;
  bool hasCreatorViewChangers(InIndex index) const final;
  ViewChangers getCreatorViewChangers(InIndex index) const final;
};

} // namespace popx
} // namespace popart

#endif

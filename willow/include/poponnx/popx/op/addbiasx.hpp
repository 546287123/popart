#ifndef GUARD_NEURALNET_ADD_BIASX_HPP
#define GUARD_NEURALNET_ADD_BIASX_HPP

#include <poponnx/names.hpp>
#include <poponnx/popx/op/identityx.hpp>
#include <poponnx/popx/op/reducesumx.hpp>
#include <poponnx/popx/opx.hpp>

namespace poponnx {

class AddBiasOp;
class AddBiasGradOp;

namespace popx {

class AddBiasOpx : public Opx {
public:
  AddBiasOpx(Op *, Devicex *);
  void grow(poplar::program::Sequence &) const override;

  std::vector<TensorId> mustExistBeforeCreate(int index0) const override;
  InputCreatorType getInputCreatorType(InIndex) const final;
  poplar::Tensor createInput(InIndex index,
                             const std::string &name) const final;
  bool createsEquiv(int index0, const Opx *opx1, int index1) const final;
};

class AddBiasInplaceOpx : public AddBiasOpx {
public:
  AddBiasInplaceOpx(Op *, Devicex *);
  void grow(poplar::program::Sequence &) const final;
};

class AddBiasDataGradOpx : public Opx {
public:
  AddBiasDataGradOpx(Op *, Devicex *);
  void grow(poplar::program::Sequence &) const final;
};

class AddBiasBiasGradOpx : public ReduceSumOpx {
public:
  AddBiasBiasGradOpx(Op *, Devicex *);
};

} // namespace popx
} // namespace poponnx

#endif

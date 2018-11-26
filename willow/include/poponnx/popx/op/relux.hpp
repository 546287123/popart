#ifndef GUARD_NEURALNET_RELUX_HPP
#define GUARD_NEURALNET_RELUX_HPP

#include <poponnx/names.hpp>
#include <poponnx/popx/opx.hpp>

namespace willow {

class ReluOp;
class ReluInplaceOp;
class ReluGradOp;

namespace popx {

class ReluOpx : public Opx {
public:
  ReluOpx(Op *, Devicex *);
  ReluOp *getReluOp() const;
  void grow(poplar::program::Sequence &) const final;
};

class ReluInplaceOpx : public Opx {
public:
  ReluInplaceOpx(Op *, Devicex *);
  ReluInplaceOp *getReluInplaceOp() const;
  void grow(poplar::program::Sequence &) const final;
};

class ReluGradOpx : public Opx {
public:
  ReluGradOpx(Op *, Devicex *);
  ReluGradOp *getReluGradOp() const;
  void grow(poplar::program::Sequence &) const final;
};

} // namespace popx
} // namespace willow

#endif

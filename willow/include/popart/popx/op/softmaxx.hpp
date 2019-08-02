#ifndef GUARD_NEURALNET_SOFTMAXXXXX_HPP
#define GUARD_NEURALNET_SOFTMAXXXXX_HPP

#include <popart/names.hpp>
#include <popart/popx/op/elementwisex.hpp>

namespace popart {

class SoftmaxOp;
class SoftmaxInplaceOp;
class SoftmaxGradOp;
class SoftmaxGradDirectOp;
class NlllWithSoftmaxGradDirectOp;

namespace popx {

class SoftmaxComputex : public EwuComputex {

public:
  SoftmaxComputex(int64_t ax, bool ens, const std::vector<size_t> &os)
      : axis(ax), enableNonStable(ens), outShape(os) {}

  poplar::Tensor outplace(poplar::program::Sequence &,
                          poplar::Graph &,
                          const poplar::Tensor &,
                          const std::string &) const final;

  void inplace(poplar::program::Sequence &,
               poplar::Graph &,
               const poplar::Tensor &,
               const std::string &) const final;

  static std::unique_ptr<EwuComputex>
  get(int64_t axis, bool ens, const std::vector<size_t> &os) {
    return std::unique_ptr<EwuComputex>(new SoftmaxComputex(axis, ens, os));
  }

  poplar::Tensor reshape(const poplar::Tensor &) const final;

  void setAxis(int64_t a) { axis = a; }

private:
  int64_t axis;
  bool enableNonStable;
  std::vector<size_t> outShape;
};

class SoftmaxOpx : public ElementWiseUnaryOutplaceOpx {
public:
  SoftmaxOpx(Op *, Devicex *);
};

class SoftmaxInplaceOpx : public ElementWiseUnaryInplaceOpx {
public:
  SoftmaxInplaceOpx(Op *, Devicex *);
};

// compute dL/dv from v and dp, where p = softmax(v)
class SoftmaxGradOpx : public ElementWiseUnaryOpx {
public:
  SoftmaxGradOpx(Op *, Devicex *);
  void grow(poplar::program::Sequence &) const final;
};

// compute dL/dv from lab and p, where p = softmax(v), L = nll(p, lab)
class SoftmaxGradDirectOpx : public Opx {
public:
  SoftmaxGradDirectOpx(Op *, Devicex *);
  void grow(poplar::program::Sequence &) const final;
};

// As above, but combines the loss calculation to reduce redundancy
class NlllWithSoftmaxGradDirectOpx : public Opx {
public:
  NlllWithSoftmaxGradDirectOpx(Op *, Devicex *);
  void grow(poplar::program::Sequence &) const final;
};

} // namespace popx
} // namespace popart

#endif
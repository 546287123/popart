#ifndef GUARD_NEURALNET_CONVX_HPP
#define GUARD_NEURALNET_CONVX_HPP

#include <willow/names.hpp>
#include <willow/popx/enigma.hpp>
#include <willow/popx/opx.hpp>

#pragma clang diagnostic push // start ignoring warnings
#pragma clang diagnostic ignored "-Weverything"
#include <poplin/Convolution.hpp>
#pragma clang diagnostic pop // stop ignoring warnings

namespace willow {

class ConvOp;
class ConvWeightsGradOp;
class ConvDataGradOp;

namespace popx {

class ConvOpx : public Opx {
public:
  ConvOpx(Op *, Devicex *);
  ConvOp *getConvOp() const;
  virtual poplar::Tensor createInput(int index) const override final;
  virtual bool canCreateInput(int index) const override final;
  virtual bool createsEquiv(int, Opx *, int) const override final;
  virtual std::vector<TensorId>
  mustExistBeforeCreate(int index0) const override final;
  const poplin::ConvParams &getParams() const;

private:
  poplin::ConvParams params;
  enigma::ConvOptions opts;
};

class ConvDataGradOpx : public Opx {
public:
  ConvDataGradOpx(Op *, Devicex *);
  ConvDataGradOp *getConvDataGradOp() const;
};

class ConvWeightsGradOpx : public Opx {
public:
  ConvWeightsGradOpx(Op *, Devicex *);
  ConvWeightsGradOp *getConvWeightsGradOp() const;
};

} // namespace popx
} // namespace willow

#endif
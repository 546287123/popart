#ifndef GUARD_NEURALNET_VOLE_HPP
#define GUARD_NEURALNET_VOLE_HPP

#include <willow/graph.hpp>

namespace willow {

// Examples of Ops with receptive fields include
// ConvOp and AveragePoolOp
class HasReceptiveFieldOp : public Op {
public:
  HasReceptiveFieldOp(const onnx::NodeProto &node, Graph *pgraph);
  // C++ rule of 3 for destructor, copy con, assignment op.

  virtual void setup() override final;
  int nSpatialDims;
  int64_t batchSize, nInChans;
  std::vector<int64_t> dilations;
  std::vector<int64_t> pads;
  std::vector<int64_t> strides;
  // the spatial dimensions of the operator
  //   : kernel size for convolution
  //   : window size for pooling
  std::vector<int64_t> spatial;

private:
  std::vector<int64_t> getOutShape() const;
  virtual int64_t getNOutChans() const = 0;
  // set the public vector "spatial"
  virtual void setSpatial() = 0;
  // anything else that a sub-class needs to do should go here:
  virtual void setup0() = 0;
};

} // namespace willow

#endif
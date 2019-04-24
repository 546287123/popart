#ifndef GUARD_NEURALNET_BASESORT_HPP
#define GUARD_NEURALNET_BASESORT_HPP

#include <poponnx/op.hpp>

namespace poponnx {

class BaseSortOp : public Op {
public:
  BaseSortOp(const OperatorIdentifier &_opid,
             int64_t axis,
             const Op::Settings &settings);

  int64_t getAxis() const;

  void appendAttributes(OpSerialiserBase &) const override;

  static int getInIndex() { return 0; }

protected:
  // confirm that the axis is within the input tensor's rank
  void validateAxis() const;

private:
  const int64_t axis;
};

} // namespace poponnx

#endif
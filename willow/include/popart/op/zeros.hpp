// Copyright (c) 2020 Graphcore Ltd. All rights reserved.
#ifndef GUARD_NEURALNET_ZEROS_HPP
#define GUARD_NEURALNET_ZEROS_HPP

#include <memory>
#include <vector>

#include <popart/op/shapeorlike.hpp>

namespace popart {

class ZerosBaseOp : public ShapeOrLikeOp {
public:
  ZerosBaseOp(const OperatorIdentifier &opid_,
              const OptionalDataType &dataType_,
              const Op::Settings &settings_);

  static std::vector<DataType> supportedDataTypes();

  std::vector<DataType> getSupportedDataTypes() const {
    return supportedDataTypes();
  }
};

class ZerosOp : public ZerosBaseOp {
public:
  ZerosOp(const OperatorIdentifier &opid_,
          const std::vector<int64_t> &shape_,
          const OptionalDataType &dataType_,
          const Op::Settings &settings_);

  std::unique_ptr<Op> clone() const final;

  void setup() final;

private:
  std::vector<int64_t> shape;
};

class ZerosLikeOp : public ZerosBaseOp {
public:
  ZerosLikeOp(const OperatorIdentifier &opid_, const Op::Settings &settings_);

  static InIndex getInIndex() { return 0; }

  void setup() final;

  std::unique_ptr<Op> clone() const final;

  std::unique_ptr<ZerosOp> foldInputTensor(const Op::Settings &) const;
};

class Op;

// Sets the gradient of a zero grad op to zeros everywhere
class UnaryZeroGradOp : public ZerosLikeOp {
public:
  UnaryZeroGradOp(const OperatorIdentifier &opid_,
                  const Op::Settings &settings_);

  static std::vector<std::unique_ptr<Op>>
  getGradOpVector(const Op::Settings &settings_);

  const std::vector<GradInOutMapper> &gradInputInfo() const {
    static const std::vector<GradInOutMapper> inInfo = {
        {getInIndex(), 0, GradOpInType::In}};
    return inInfo;
  }
  const std::map<int, int> &gradOutToNonGradIn() const {
    static const std::map<int, int> outInfo = {{getInIndex(), 0}};
    return outInfo;
  }
};

} // namespace popart

#endif

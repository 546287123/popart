#ifndef GUARD_NEURALNET_IPUCOPY_HPP
#define GUARD_NEURALNET_IPUCOPY_HPP

#include <poponnx/op.hpp>

namespace poponnx {

using SourceIpuMap = std::map<TensorId, uint64_t>;

class IpuCopyOp : public Op {
public:
  IpuCopyOp(const OperatorIdentifier &_opid,
            uint64_t _destIpu,
            const Op::Settings &settings_);
  std::unique_ptr<Op> clone() const final;
  void setup() final;

  uint64_t getDestIpu() const { return destIpu; }
  const SourceIpuMap &getSourceIpus() const;
  uint64_t getSourceIpu(const TensorId &tenId) const;
  uint64_t getSourceIpu() const;

  void appendAttributes(OpSerialiserBase &) const override;

  float getSubgraphValue() const final { return getLowSubgraphValue(); }

  bool isOutlineable() const override { return false; }

  bool isIpuCopyOp() const override;

  void connectInTensor(InIndex, TensorId, uint64_t sourceIpu);

private:
  void connectInTensor(InIndex, TensorId) override {
    throw error(
        "Must supply a sourceIpu when calling "
        "IpuCopyOp::connectInTensor(InIndex, TensorId, uint64_t sourceIpu)");
  }

  SourceIpuMap sourceIpus;
  uint64_t destIpu;
};
} // namespace poponnx

#endif

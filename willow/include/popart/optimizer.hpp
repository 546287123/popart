#ifndef GUARD_NEURALNET_OPTIMIZER_HPP
#define GUARD_NEURALNET_OPTIMIZER_HPP

#include <memory>
#include <popart/compoundscalarhelper.hpp>
#include <popart/names.hpp>
#include <popart/optimizervalue.hpp>
#include <popart/optimizervaluemap.hpp>
#include <popart/tensorinfo.hpp>
#include <popart/tensornames.hpp>

namespace popart {

enum class OptimizerType { SGD = 0, NTYPES };

// The base Optimizer class
class Optimizer {
public:
  virtual ~Optimizer() = default;
  Optimizer(OptimizerValue lossScaling);
  Optimizer(const Optimizer &) = default;

  // If a Graph has been constructed with this Optimizer, can it be updated with
  // "other", without requiring a change to compute Graph? For example, a
  // VarUpdate which has a constant scaled learning rate cannot be modified to
  // have a variable scaled learning rate
  virtual bool validReplacement(const Optimizer &other) const = 0;

  virtual OptimizerType type() const               = 0;
  virtual std::string type_s() const               = 0;
  virtual std::unique_ptr<Optimizer> clone() const = 0;

  // (re)set the data in Tensor from a relevant value stored by this Optimizer.
  // The particular value used is determined from the Tensor's name/type
  virtual void resetTensorData(Tensor &) const = 0;
  virtual void setTensorData(Tensor &) const   = 0;

  // Create a VarUpdate Op for a specific weight Tensor using this Optimizer,
  // and get the names of inputs to the VarUpdate Op fo a specific Tensor
  virtual std::unique_ptr<Op> createOp(const Tensor &weight, Graph &) const = 0;

  virtual std::vector<TensorId> getInputIds(const Tensor &weight,
                                            bool enableGradAccl,
                                            int64_t acclFact) const = 0;

  // Unique non-const optimizers
  virtual std::vector<std::tuple<TensorId, TensorInfo>>
  getOptimizerInputs(const Tensor &weight,
                     bool enableGradAccl,
                     int64_t acclFact) const = 0;

  const OptimizerValue &lossScaling() const { return ls; }
  float getLossScalingVal() const { return ls.val(); }

  static TensorId getLossScalingTensorId(DataType);

private:
  OptimizerValue ls;
};

// Equation derivation based on the non-Nesterov pytorch implementation
// https://pytorch.org/docs/stable/_modules/torch/optim/sgd.html#SGD :
//
// g = gradient computed in backwards pass
// g = g + wd * w
// v = v * mm + (1 - dm) * g
// w = w - lr * v
//
// which is equivalent to
//
// g = gradient computed in backwards pass
// v = v * mm + (1 - dm) * g + (1 - dm) * wd * w
// w = w - lr * v
//
// if we include loss scaling, we factor ls out of g first:
//
// g = gradient computed in backwards pass * ls
// v = v * mm + (1 - dm) / ls * g + (1 - dm) * wd * w
// w = w - lr * v
//
// if we want to keep velocity (v) a factor vs larger throughout for numerical
// reasons, we
// (1) multiply the term added to it by scalar factor vs
// (2) make sure it is initialised with a factor vs larger (T12001)
// (3) divide lr by vs:
//
// v = v * mm + (1 - dm) * vs / ls * g + (1 - dm) * wd * vs * w
// w = w - lr / vs * v.
//
// if there is gradient accumulation, this becomes:
//
// v = v * mm + (1 - dm) * vs / ls * sum_micro_batches(g) +
//                                                  + (1 - dm) * wd * vs * w
// w = w - lr / vs * v.
//
// which has 2 parts, one part in the loop:
//    v <- v + (1 - dm) * vs / ls * g_i for each micro batch i's gradient
//
// and one part out the loop:
//    w <- w - lr / vs * v
//    v <- v * mm + (1 - dm) * wd * vs * w.   (done once up front too,
//                                                      see test comments)
//
//
// if in addition there is data replication by factor rf, the equations become
// in the loop:
//    v <- v + (1 - dm) * vs / ls * rf * g_i
//
// and outside the loop:
//    v <- reduction across IPUs of vs
//    v <- v / rf
//    w <- w - lr / vs * v
//    v <- v * mm + (1 - dm) * wd * vs * w.
//
// where the scalar factors corresponding to pytorch are,
//   mm : momentum
//   dm : dampening
//   wd : weight decay
//   lr : learning rate
//
// the optional scaling factors to improve numerical stability are
//   ls : loss scaling
//   vs : velocity scaling
//
// and the term to accelerate training is
//   rf : data replication factor.
//
// In the case where there is no gradient accumulation and no momentum (mm = 0),
// there is no need for a persistant v Tensor, and the weight update reduces to,
//
// w <- w * {1 -  lr * (1 - dm) * wd} -  g * { lr * (1 - dm) / ls }   (1)
//          ^^^^^^^^^^^^^^^^^^^^^^^^^        ~~~~~~~~~~~~~~~~~~~~~~
//                    |                               |
//   weight decay scale factor 0                      |
//                                         scaled learning rate 0
//
// In this simpler case, all is done in a single Op of type SGD0VarUpdateOp
//
// where the sum is over the accumulationFactor mini-batches which make up the
// batch.
//
//
// Note that all compound scalar terms above are always calculated on host.
//
// To summarise, there are *atomic* scalars and *compound* scalars.
//
// The atomic scalars are mm, dm, wd, lr, ls, vs, rf.
//
// The compound scalars for the simple case of no persistent v tensor are,
//
// Compound scalars for the case where there is no gradient accumulation (SGD0):
//
//  - weightDecayScaleFactor0 (wdsf0) =
//      1 - lr * (1 - dm) * wd
//
//  - scaledLearningRate0 (slr0) =
//      lr *  ( 1 - dm) / ls
//
// Compound scalars for the case where there IS gradient accumulation (SGD1):
//
//  - weightDecayScaleFactor1 (wdsf1) =
//      (1 - dm) * wd * vs
//
//  - dampeningScaleFactor1 (dpsf1) =
//      (1 - dm) * vs * rf / ls
//
//  - scaledLearningRate1 (slr1) =
//      lr / vs
//
//  - momentum1 (mm1) =
//      mm
//
//
// Note that the user sets atomic scalars (not compound scalars)
//
// Note that all atomic scalar terms except loss scaling and replication factor
// can be Tensor specific.
//
// Constructing an SGD Optimizer is done in 2 steps;
//
// (1) Construct SGD with default values
// (2) Set Tensor specific values
//
// Any OptimizerValue can be set as isConst if it will not change during
// training. This can result in faster/smaller code. For a compound scalar to be
// isConst, all of its constituent atomic scalars must be isConst
//
// Currently rf != 1 is not supported for the case where mm != 0. The plan for
// enabling this: (1) make 1 Op which updates both w and g, i.e. does everything
// outside the loop. (2) support aliasing and modifying Ops with more than 1
// output. T12001 (above)

class SGD : public Optimizer {

public:
  static OptimizerValue getUnsetMomentum() {
    return {0.0f, true}; // no momentum, ever
  }

  static OptimizerValue getUnsetDampening() {
    return {0.0f, true}; // no dampening, ever
  }

  static OptimizerValue getUnsetVelocityScaling() {
    return {1.0f, true}; // no velocity scaling, ever
  }

  static OptimizerValue getUnsetWeightDecay() {
    return {0.0f, true}; // no weight decay, ever
  }

  static OptimizerValue getUnsetLossScaling() {
    return {1.0f, true}; // no loss scaling, ever
  }

  static OptimizerValue getUnsetLearningRate() {
    return {0.1f, true}; // a learning rate of 0.1 forever
  }

public:
  // Does "w" have specific OptimizerValues, or will it is default?
  bool hasSpecific(const Tensor &w) const;

  // SGD constructor with all 6 parameteers
  // ----------------
  SGD(OptimizerValue default_lr,
      OptimizerValue default_wd,
      OptimizerValue default_mm,
      OptimizerValue default_dp,
      OptimizerValue default_vs,
      OptimizerValue ls);

  // Example:
  //
  // SGD({{"defaultLearningRate", {0.02, False}},
  //      {"defaultMomentum":{0.6, True}}});
  //
  // will create an SGD Optimizer which has a constant momentum of 0.6 and a
  // changeable learning rate initially of 0.02. All OptimizerValues not present
  // in the map will take values from the getUnset* functions.
  //
  // Construct from pair instead of OptimizerValue for pybind11 support
  //
  SGD(const std::map<std::string, std::pair<float, bool>> &);
  static SGD fromDefaultMap(const std::map<std::string, OptimizerValue> &);

  SGD(const SGD &) = default;
  ~SGD()           = default;

  OptimizerType type() const final { return OptimizerType::SGD; }
  std::string type_s() const final { return "SGD"; }

  std::unique_ptr<Optimizer> clone() const final;

  std::unique_ptr<Op> createOp(const Tensor &weight, Graph &) const final;

  // The names of the inputs for the VarUpdateOp the Var Tensor "weight". In the
  // returned vector,  a "" is used as a placeholder for constant inputs
  std::vector<TensorId> getInputIds(const Tensor &weight,
                                    bool enableGradAccl,
                                    int64_t acclFact) const final;

  // The names and infos of the optimizer Tensors
  std::vector<std::tuple<TensorId, TensorInfo>>
  getOptimizerInputs(const Tensor &weight,
                     bool enableGradAccl,
                     int64_t acclFact) const final;

  bool validReplacement(const Optimizer &other) const final;

  void resetTensorData(Tensor &) const final;
  void setTensorData(Tensor &) const final;

  // Tensor "opt" has an id, based on which it matches a compound scalar which
  // this object can compute from the atomic scalars
  float getStoredValue(const TensorId &optId) const;

  void insertSpecific(const TensorId &,
                      OptimizerValue lr,
                      OptimizerValue wd,
                      OptimizerValue mm,
                      OptimizerValue dp,
                      OptimizerValue vs);

  // insert OptimizerValues specific to one Tensor. The keys of the map should
  // be the names of atomic optimizer scalars, such as "momentum",
  // "learningRate". The map does not need to be complete. If it is not
  // complete, the default values already set for the SGD will be used.
  void insertSpecific(const TensorId &,
                      const std::map<std::string, std::pair<float, bool>> &);

  // If velocity (accumulation) is required, either because of gradient
  // accumulation or because of momentum : return true, otherwise return false.
  bool requiresAccl(const Tensor &weight,
                    bool gradAcclEnabled,
                    int64_t gradAcclFactor) const;

  const OptimizerValueMap &learningRates() const { return lrs; }
  const OptimizerValueMap &weightDecays() const { return wds; }
  const OptimizerValueMap &momentums() const { return mms; }
  const OptimizerValueMap &dampenings() const { return dps; }
  const OptimizerValueMap &velocityScalings() const { return vss; }

private:
  void runValueChecks(float lr, float wd, float mm, float dp, float vs) const;

  // The atomic scalars
  // ------------------
  // learning rates
  OptimizerValueMap lrs;

  // weight decays
  OptimizerValueMap wds;

  // momentums
  OptimizerValueMap mms;

  // dampenings
  OptimizerValueMap dps;

  // velocity scalings
  OptimizerValueMap vss;

  // The compound scalars
  // --------------------
  // No Accumulation Tensor needed (SGD0)
  ScaledLearningRate0Helper slr0helper;
  WeightDecayScaleFactor0Helper wdsf0helper;

  // Accumulation Tensor needed (SGD1)
  ScaledLearningRate1Helper slr1helper;
  WeightDecayScaleFactor1Helper wdsf1helper;
  DampeningScaleFactor1Helper dpsf1helper;
  Momentum1Helper mm1helper;

  OptimizerValue
  getLossScalingOrDefault(const std::map<std::string, OptimizerValue> &) const;

  // int argument only to disambiguate from the other SGD constructor
  SGD(const std::map<std::string, OptimizerValue> &, int);

  static std::map<std::string, OptimizerValue>
  getComplete(const std::map<std::string, OptimizerValue> &);
};

// This class is kept to be backwards compatible with the Python API, should be
// removed at some point in the future.
class ConstSGD : public SGD {
public:
  ConstSGD(float lr, float wd = 0, float ls = 1)
      : SGD({lr, true},
            {wd, true},
            getUnsetMomentum(),
            getUnsetDampening(),
            getUnsetVelocityScaling(),
            {ls, true}) {}
};

} // namespace popart

#endif

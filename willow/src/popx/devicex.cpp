#include <algorithm>
#include <cctype>
#include <fstream>
#include <random>

#include <poplin/codelets.hpp>
#include <popnn/codelets.hpp>
#include <popops/ElementWise.hpp>
#include <popops/codelets.hpp>
#include <poprand/RandomGen.hpp>
#include <poprand/codelets.hpp>
#include <poputil/exceptions.hpp>
#include <poponnx/devicemanager.hpp>
#include <poponnx/error.hpp>
#include <poponnx/ir.hpp>
#include <poponnx/logging.hpp>
#include <poponnx/makeunique.hpp>
#include <poponnx/op.hpp>
#include <poponnx/popx/devicex.hpp>
#include <poponnx/popx/devicexmanager.hpp>
#include <poponnx/popx/opx.hpp>
#include <poponnx/popx/opxmanager.hpp>
#include <poponnx/popx/poplaroptionsx.hpp>
#include <poponnx/pritask.hpp>
#include <poponnx/tensor.hpp>
#include <poponnx/tensordata.hpp>

namespace poponnx {
namespace popx {

const std::string randomSeedId = "randomSeed";

void Devicex::weightsToHost() {

  if (useSyntheticData() == false) {
    logging::devicex::debug("Writing weights to host");
    pEngine->disableExecutionProfiling();
    pEngine->run(PopPrograms::ProgramIndex::WEIGHTSTOHOST);
    logging::devicex::debug("Writing weights to host complete.");
  }
}

void Devicex::readWeights(const IWeightsIO &weights) {

  // Better to do this the otherway round
  for (auto id : ir().getTensorIds(TensorType::Variable)) {
    if (weights.contains(id)) {
      MutableVoidData stepout = weights.weight(id);
      hostStreamToHost(stepout, id);
    }
  }
}

void Devicex::writeWeights(const IWeightsIO &weights) {
  // Better to do this the otherway round
  for (auto id : ir().getTensorIds(TensorType::Variable)) {
    if (weights.contains(id)) {
      auto tensor             = ir().getTensor(id);
      MutableVoidData stepout = weights.weight(id);
      tensor->tensorData()->resetData(stepout.info, stepout.data);
    }
  }
}

void Devicex::weightsToHost(
    const std::map<TensorId, MutableVoidData> &onnxModelData) {

  if (useSyntheticData() == false) {
    logging::devicex::debug("Writing weights to host");
    // write weights from IPU to host stream memory points

    pEngine->disableExecutionProfiling();
    pEngine->run(PopPrograms::ProgramIndex::WEIGHTSTOHOST);

    logging::devicex::debug("Writing weights to ONNX ModelProto");
    // copy from the host stream memory points to the
    // addresses on onnxModelData
    for (auto id : ir().getTensorIds(TensorType::Variable)) {
      auto found = onnxModelData.find(id);
      if (found == onnxModelData.end()) {
        throw error("No TensorId " + id + " in final host destination map");
      }
      MutableVoidData mv_data = found->second;
      hostStreamToHost(mv_data, id);
    }
  }
}

poplar::Tensor Devicex::getConst(const poplar::Type &type,
                                 const std::vector<size_t> &shape,
                                 double val,
                                 const std::string &name) {
  auto tensor = masterGraph().addConstant(type, shape, val, name);
  masterGraph().setTileMapping(tensor, 0);
  return tensor;
}

PopTensors::PopTensors(const Ir &ir_) : ir(ir_) {}

void PopTensors::insert(TensorId id, const poplar::Tensor &pt) {
  auto found = tensors_.find(id);
  if (found != tensors_.end()) {
    throw error("ILE: poplar::Tensor " + id + " already in map");
  }

  if (!ir.containsTensor(id)) {
    throw error("ILE: no tensor named " + id +
                " in ir, is this a valid poplar::Tensor?");
  }

  // confirm shapes agree (up to squeezing out the extra 1s)
  auto irTensorStr   = ir.getTensor(id)->str();
  auto expectedShape = ir.getTensor(id)->info.shape_szt();

  if (pt.shape() != expectedShape) {
    std::stringstream ss;
    ss << "poplar::Tensor " << id << " of unexpected shape. "
       << "Poplar tensor shape: ";
    appendSequence(ss, pt.shape());
    ss << ". Expected (Ir) tensor shape: ";
    appendSequence(ss, expectedShape);
    ss << ". This for tensor " << irTensorStr;
    throw error(ss.str());
  }

  // confirm types agree
  auto expectedType = popType(ir.getTensor(id)->info);
  if (pt.elementType() != expectedType) {
    std::stringstream ss;
    ss << "poplar::Tensor " << id << " of unexpected Type. "
       << "Poplar tensor type : " << pt.elementType();
    ss << ". Expected (Ir) tensor type : " << expectedType;
    ss << ". This for tensor " << irTensorStr;
    throw error(ss.str());
  }

  tensors_[id] = pt;
}

const poplar::Tensor &PopTensors::get(TensorId id) const {
  auto found = tensors_.find(id);
  if (found == tensors_.end()) {
    throw error("no poplar::Tensor " + id);
  }
  return found->second;
}

const std::map<TensorId, poplar::Tensor> &PopTensors::getTensors() const {
  return tensors_;
}

PopPrograms::PopPrograms(const int repeatCount_) : repeatCount(repeatCount_) {
  if (repeatCount_ <= 0) {
    throw error("Program repeat count must be greater than zero");
  }
}

poplar::program::Sequence &PopPrograms::streamWeightsFromHostFragment() {
  return seqs[static_cast<int>(ProgramFragmentIndex::STREAMWEIGHTSFROMHOST)];
}

poplar::program::Sequence &PopPrograms::copyWeightsBetweenIpusFragment() {
  return seqs[static_cast<int>(ProgramFragmentIndex::COPYWEIGHTSBETWEENIPUS)];
}

poplar::program::Sequence &PopPrograms::streamOptimizerFromHostFragment() {
  return seqs[static_cast<int>(ProgramFragmentIndex::STREAMOPTIMIZERFROMHOST)];
}

poplar::program::Sequence &PopPrograms::copyOptimizerBetweenIpusFragment() {
  return seqs[static_cast<int>(ProgramFragmentIndex::COPYOPTIMIZERBETWEENIPUS)];
}

poplar::program::Sequence &PopPrograms::programFragment() {
  return seqs[static_cast<int>(ProgramFragmentIndex::PROGRAM)];
}

poplar::program::Sequence &PopPrograms::setRandomSeedFragment() {
  return seqs[static_cast<int>(ProgramFragmentIndex::SETRANDOMSEED)];
}

poplar::program::Sequence &PopPrograms::weightsToHostFragment() {
  return seqs[static_cast<int>(ProgramFragmentIndex::WEIGHTSTOHOST)];
}

poplar::program::Sequence PopPrograms::weightsFromHost() {
  poplar::program::Sequence prog;
  prog.add(streamWeightsFromHostFragment());
  if (!copyWeightsBetweenIpusFragment().isEmpty()) {
    prog.add(copyWeightsBetweenIpusFragment());
  }
  return prog;
}

poplar::program::Sequence PopPrograms::optimizerFromHost() {
  poplar::program::Sequence prog;
  prog.add(streamOptimizerFromHostFragment());
  if (!copyOptimizerBetweenIpusFragment().isEmpty()) {
    prog.add(copyOptimizerBetweenIpusFragment());
  }
  return prog;
}

poplar::program::Sequence PopPrograms::program() {
  poplar::program::Sequence prog;
  prog.add(programFragment());

  poplar::program::Sequence outer;
  outer.add(setRandomSeedFragment());
  outer.add(poplar::program::Repeat(repeatCount, prog));

  return outer;
}

poplar::program::Sequence PopPrograms::weightsToHost() {
  return weightsToHostFragment();
}

std::vector<poplar::program::Program> PopPrograms::progs() {
  std::vector<poplar::program::Program> ps(ProgramIndex::N);

  ps[ProgramIndex::WEIGHTSFROMHOST]   = weightsFromHost();
  ps[ProgramIndex::OPTIMIZERFROMHOST] = optimizerFromHost();
  ps[ProgramIndex::PROGRAM]           = program();
  ps[ProgramIndex::WEIGHTSTOHOST]     = weightsToHost();

  return ps;
}

poplar::program::Sequence &
PopPrograms::programFragment(PopPrograms::ProgramFragmentIndex index) {
  return seqs[static_cast<int>(index)];
}

poplar::program::Sequence &PopPrograms::programFragment(const Scope &scope) {
  if (scope.empty()) {
    return programFragment();
  } else {
    return scopeSeqs.at(scope.str());
  }
}

bool PopPrograms::containsFragment(const Scope &scope) {
  if (scope.empty()) {
    return true;
  } else {
    return scopeSeqs.find(scope.str()) != scopeSeqs.end();
  }
}

void PopPrograms::createFragment(const Scope &scope) {
  scopeSeqs.insert({scope.str(), {}});
}

poplar::Graph &Devicex::rootGraph() { return *pRootGraph; }

const poplar::Graph &Devicex::rootGraph() const { return *pRootGraph; }

poplar::Graph &Devicex::masterGraph() { return *pMasterGraph; }

poplar::Graph &Devicex::graph(int64_t virtualGraphIndex) {
  if (virtualGraphIndex < 0 || virtualGraphIndex >= virtualGraphs.size()) {
    throw error("Invalid virtual graph index {} ({} available)",
                virtualGraphIndex,
                virtualGraphs.size());
  }
  return virtualGraphs.at(virtualGraphIndex);
}

Devicex::Devicex(const Ir &ir, std::shared_ptr<DeviceInfo> deviceInfo_)
    : poponnx::Device(ir),
      progs(PopPrograms(ir.getDataFlow().batchesPerStep())), tensors(ir),
      deviceInfo(deviceInfo_), prepareHasBeenCalled(false) {

  logging::devicex::info("Setting selected device: {}", *deviceInfo);

  if (!deviceInfo->attach()) {
    throw error("failed to attach to device");
  }

  // TODO (see T5100) : if inference, forward should be INFERENCE_FWD
  for (auto it : ir.getSessionOptions().convolutionOptions) {
    fwdConvOptions.options[it.first] = it.second;
    bwdConvOptions.options[it.first] = it.second;
    wuConvOptions.options[it.first]  = it.second;
  }

  if (ir.getExecutionMode() == Ir::ExecutionMode::TRAINING) {
    fwdConvOptions.options["pass"] = "TRAINING_FWD";
    lstmOptions.set("inferenceOnly", "false");
  } else {
    fwdConvOptions.options["pass"] = "INFERENCE_FWD";
    lstmOptions.set("inferenceOnly", "true");
  }

  bwdConvOptions.options["pass"] = "TRAINING_BWD";
  wuConvOptions.options["pass"]  = "TRAINING_WU";

  // Not sure what these options should be
  if (ir.getExecutionMode() == Ir::ExecutionMode::TRAINING) {
    fwdMmOptions.options.insert({"fullyConnectedPass", "TRAINING_FWD"});
  } else {
    fwdMmOptions.options.insert({"fullyConnectedPass", "INFERENCE_FWD"});
  }

  bwdMmLhsOptions.options.insert({"fullyConnectedPass", "TRAINING_BWD"});
  bwdMmRhsOptions.options.insert({"fullyConnectedPass", "TRAINING_WU"});

  engineOptions.set("target.workerStackSizeInBytes", "0x200");
  for (auto it : ir.getSessionOptions().engineOptions) {
    engineOptions.set(it.first, it.second);
  }

  for (auto it : ir.getSessionOptions().reportOptions) {
    reportOptions.set(it.first, it.second);
  }
}

void Devicex::weightsFromHost() {
  if (useSyntheticData() == false) {
    logging::devicex::debug("Writing weights from host, ");
    pEngine->disableExecutionProfiling();
    pEngine->run(PopPrograms::ProgramIndex::WEIGHTSFROMHOST);
    logging::devicex::debug("done.");
  }
}

void Devicex::optimizerFromHost() {
  if (useSyntheticData() == false) {
    logging::devicex::debug("Writing optimizer from host, ");
    pEngine->disableExecutionProfiling();
    pEngine->run(PopPrograms::ProgramIndex::OPTIMIZERFROMHOST);
    logging::devicex::debug("done.");
  }
}

void Devicex::hostToHostStream(
    void *dst,                 // destination of copy (a step tensor)
    const void *src,           // source of copy
    const TensorInfo &dstInfo, // the info for dst
    const TensorInfo &srcInfo, // user provided info for src
    TensorId id // for clear error message, we need the id of the tensor
) {

  // confirm that the shapes of dst and src agree
  if (dstInfo.shape() != srcInfo.shape()) {
    std::stringstream ss;
    ss << "Shape discrepency for tensor " << id
       << ",\nStep tensor info (user) : ";
    srcInfo.append(ss);
    ss << "\nStep tensor info (expected) : ";
    dstInfo.append(ss);
    ss << ",\nBatches per step : " << ir().getDataFlow().batchesPerStep()
       << '.';
    throw error(ss.str());
  }

  // Log the name and shape of the tensor
  logging::devicex::debug("       {} {}", id, srcInfo.shape());

  auto srcType = srcInfo.dataType();
  auto dstType = dstInfo.dataType();

  // check type compatibility
  if (srcType == dstType) {
    // copy the full step data from src to dst
    std::memcpy(dst, src, srcInfo.nbytes());
  }

  else if (srcType == DataType::INT64 && dstType == DataType::INT32) {
    logging::devicex::debug("Copying (host) tensor {} from INT64 to INT32", id);
    auto dst_int32 = static_cast<int *>(dst);
    auto src_int64 = static_cast<const int64_t *>(src);
    for (auto i = 0; i < dstInfo.nelms(); ++i) {
      dst_int32[i] = static_cast<int>(src_int64[i]);
    }
  }
  // add more custom copies here. Design decision: don't
  // just blindly cast, if the user provides an int
  // tensor when a float tensor is expected they might
  // have made a mistake.

  else {
    std::stringstream ss;
    ss << "Type discrepency for tensor " << id
       << ". User provided : " << srcInfo.data_type()
       << " and expected : " << dstInfo.data_type()
       << ". Consider a custom copy here (as memcpy cannot be used)";
    throw error(ss.str());
  }
}

// Copy from the host end of a d2h stream, to some final host memory.
// This is the step which follows a copy from device to host.
// poplar::Streams cannot write to an arbitrary dynamic address,
// they are connected to a fixed host address. This function copies
// from that fixed address to a dynamic address (mv_data).
void Devicex::hostStreamToHost(const MutableVoidData &mv_data, TensorId id) {

  // The host end of the poplar::Stream,
  // we will try to copy from here
  auto src = static_cast<const void *>(d2hBuffers.at(id).data());

  auto dst = mv_data.data;

  // size of the host end of the poplar stream.
  // It is a char vector, so this is in bytes.
  int64_t nbytes_src = d2hBuffers.at(id).size();

  // number of bytes of the destination.
  int64_t nbytes_dst = mv_data.info.nbytes();

  // display which tensors are being copied
  logging::devicex::debug("       {} {}", id, ir().getTensor(id)->info.shape());

  // We confirm that the sizes of src and dst are the same
  if (nbytes_src != nbytes_dst) {
    std::stringstream errms;
    errms << "sizes (in bytes) of src (" << nbytes_src << ") and dst ("
          << nbytes_dst << ") differ in hostStreamToHost for " << id;
    throw error(errms.str());
  }

  std::memcpy(dst, src, nbytes_src);
}

void Devicex::anchorsHostToHostStreams(const IStepIO &stepio) {

  if (useSyntheticData() == false) {
    std::string prefix = "     ";
    logging::devicex::debug(prefix + "Copying to h2d stream address(es) ");
    for (Tensor *tensor : ir().dataStreamTensors()) {
      ConstVoidData stepin = stepio.in(tensor->id);

      // where to write to on host,
      auto dst = static_cast<void *>(h2dBuffers.at(tensor->id).data());
      // where to read from on host,
      auto src = stepin.data;

      // we calculate the TensorInfo for dst. If batchesPerStep() = 1, then
      // it has the same dimensions as tensor->info. Otherwise it has has
      // an extra dimension of size batchesPerStep() to accommmodate all
      // step anchor tensors.
      auto stepDstShape = tensor->info.shape();
      if (ir().getDataFlow().batchesPerStep() > 1) {
        stepDstShape.insert(stepDstShape.begin(),
                            ir().getDataFlow().batchesPerStep());
      }
      // if the replicationFactor is greater than 1 then add an extra
      // dimension of size replicationFactor so we can report mutliple
      // copies of the tensor
      // Q: Should replicated tensors be combined before returning?
      if (getReplicationFactor() > 1) {
        stepDstShape.insert(stepDstShape.begin(), getReplicationFactor());
      }
      TensorInfo dstInfo{tensor->info.dataType(), stepDstShape};

      // the info of the user provided src step tensor
      TensorInfo srcInfo = stepin.info;

      hostToHostStream(dst, src, dstInfo, srcInfo, tensor->id);
    }
  }
}

void Devicex::anchorsHostFromHostStreams(const IStepIO &stepio) {

  if (useSyntheticData() == false) {
    std::string prefix = "     ";
    logging::devicex::debug(prefix + "Copying from d2h stream address(es) ");
    for (TensorId anchorId : ir().getDataFlow().anchors()) {
      MutableVoidData stepout = stepio.out(anchorId);
      hostStreamToHost(stepout, anchorId);
    }
  }
}

void Devicex::run(const IStepIO &stepio) {
  if (!prepareHasBeenCalled) {
    throw error("Devicex::prepare() must be called before"
                " Devicex::run(const IStepIO &) is called.");
  }
  logging::devicex::debug("Performing one step: ");
  anchorsHostToHostStreams(stepio);

  pEngine->enableExecutionProfiling();
  pEngine->run(PopPrograms::ProgramIndex::PROGRAM);

  anchorsHostFromHostStreams(stepio);
}

std::unique_ptr<Opx> Devicex::createOpx(Op *op) {

  auto opx = OpxManager::createOpx(op, this);

  if (!opx) {
    if (op->opid == Onnx::Operators::Constant_1 ||
        op->opid == Onnx::Operators::Constant_9) {
      throw error("ILE: No Opx for {}", op->opid);
    } else {
      throw error("Could not create opx for '{}'", op->opid);
    }
  }

  return opx;
}

Opx *Devicex::getOpx(OpId id) { return opxs.at(id).get(); }

TaskId Devicex::taskWhichCreates(TensorId id) const {
  Tensor *tensor = ir().getTensor(id);
  // streamed and init tensors are created with
  // tasks with names from initTensorTaskId
  // These tensors are recognisable as having no producing Op.
  if (tensor->hasProducer() == false) {
    return initTensorTaskId(id);
  }

  else {
    return opTaskId(tensor->getProducer());
  }
}

std::vector<InputCreatorCandidate>
Devicex::getCreatorEndpoints(Tensor *tensor,
                             std::vector<OpxInAndOutIndex> pathFromInput,
                             bool excludeEndpointsFromPath,
                             bool includeDeadends) {

  std::vector<InputCreatorCandidate> endpoints;
  for (Op *op : tensor->consumers.getOps()) {
    auto conOpId = op->id;
    Opx *opx     = getOpx(conOpId);

    for (int inIndex : op->input->indices(tensor)) {
      auto updatedPath = pathFromInput;

      switch (opx->getInputCreatorType(inIndex)) {
      // Opx has poplar call to layout tensor at this
      // inIndex
      case InputCreatorType::CANCREATE: {
        if (!excludeEndpointsFromPath) {
          updatedPath.push_back({opx, inIndex, -1}); // note: no valid outIndex
        }
        endpoints.push_back({inIndex, opx, updatedPath});
        break;
      }
      // Recursively search the DAG downstream of the op until we
      // have set of endpoints that can create the tensor
      case InputCreatorType::CANUNWIND: {
        for (auto &ind_ten : op->output->tensorMap()) {
          auto nextOutputTensor = ind_ten.second;
          auto outIndex         = ind_ten.first;
          updatedPath.push_back({opx, inIndex, outIndex});
          for (auto candidate :
               getCreatorEndpoints(nextOutputTensor, updatedPath)) {
            endpoints.push_back(candidate);
          }
        }
        break;
      }
      // Consuming op can't create tensor
      case InputCreatorType::DEADEND: {
        if (includeDeadends) {
          if (!excludeEndpointsFromPath) {
            updatedPath.push_back(
                {opx, inIndex, -1}); // note: no valid outIndex
          }
          endpoints.push_back({inIndex, opx, updatedPath});
        }
        break;
      }
      default: {
        throw error("InputCreatorType not implemented for Opx of OpId {}",
                    op->id);
      }
      }
    }
  }
  return endpoints;
}

// Design decision : leave the option for a Tensor to be
// created based on complex global criteria open.
PriTask Devicex::initTensorTask(Tensor *tensor) {

  auto errorbase = [&tensor]() {
    std::stringstream ss;
    ss << "Failed to add tensor " << tensor->id << '.';
    tensor->consumers.append(ss);
    return ss.str();
  };

  // Search of the graph to get the candidate Opxs that
  // know how to create this tensor.
  // The pathFromInput argument is an empty vector, as
  // we are starting the search from the root (input)
  std::vector<InputCreatorCandidate> candidates =
      getCreatorEndpoints(tensor, {});

  if (candidates.size() > 1) {
    // check that all creators are in agreement on how
    // to create the poplar::Tensor. If they are, just keep
    // the first one.
    bool allEquivalent = true;
    auto cand0         = candidates[0];
    for (int i = 1; i < candidates.size(); ++i) {
      auto cand1 = candidates[i];
      if (!cand0.opx->createsEquiv(cand0.index, cand1.opx, cand1.index)) {
        allEquivalent = false;
        break;
      }
    }

    // they're all equivalent, select the first candidate as the creator
    if (allEquivalent) {
      candidates.resize(1);
    } else {
      logging::devicex::warn("Input tensor '{}' has multiple creator "
                             "candidates, but they are not in agreement",
                             tensor->id);
    }
  }

  // 1. A unique candidate creator will create the tensor
  // 2. The tensor will be unwound (have its layout modified)
  //    by view-changing opxs on the path from the input to
  //    the candidate candidate
  if (candidates.size() == 1) {
    Opx *creator       = candidates[0].opx;
    int inIndex        = candidates[0].index;
    auto pathFromInput = candidates[0].getPathFromInput();

    auto f = [this, creator, inIndex, pathFromInput, tensor]() {
      logging::devicex::debug("Creating poplar::Tensor {}", tensor->id);
      // tensors.insert(tensor->id, creator->createInput(inIndex));
      poplar::Tensor input = creator->createInput(inIndex, tensor->str());

      // Reverse the path,
      // The first element is now the Opx producing a tensor consumed by
      // the candidate.
      // The last element is now the Opx consuming the input we are mapping.
      auto pathToInput = pathFromInput;
      std::reverse(pathToInput.begin(), pathToInput.end());

      for (auto opxOnPath : pathToInput) {
        input = opxOnPath.opx->unwindTensorLayout(
            input, opxOnPath.inIndex, opxOnPath.outIndex);
      }
      tensors.insert(tensor->id, input);
      efficientlyCreatedInputTensors.insert(tensor->id);
    };
    // the inputs of creator which must have poplar::Tensors
    // before creator creates input tensor at index inIndex.
    std::vector<TaskId> deps;
    for (TensorId tenId : creator->mustExistBeforeCreate(inIndex)) {
      TaskId dep = taskWhichCreates(tenId);
      deps.push_back(dep);
    }

    // Discussion with David Norman suggests creating tensors as
    // late as possible gives better IPU memory use, so
    // giving this low priority.
    return {-1e6,
            initTensorTaskId(tensor->id), // the task name
            deps,
            f};
  }

  else if (candidates.size() > 1) {
    throw error(errorbase() + "\nConflicting creator candidates.");
  }

  else {

    auto f = [this, tensor]() {
      logging::devicex::warn("Creating input tensor '{}' linearly. No "
                             "operator specific allocator found",
                             tensor->id);

      // Get paths to both creator candidates and deadends, and print for debug
      std::vector<InputCreatorCandidate> endpoints =
          getCreatorEndpoints(tensor, {}, false, true);
      int endpointId = 1;
      logging::devicex::debug("Printing paths to {} endpoint(s) found when "
                              "searching for a creator candidate for {}",
                              endpoints.size(),
                              tensor->id);
      for (auto endpoint : endpoints) {
        auto path = endpoint.getPathFromInput();
        logging::devicex::debug("  Path to endpoint {}, starting from input",
                                endpointId);
        for (auto opxOnPath : path) {
          Op *opOnPath = opxOnPath.opx->op_p;
          logging::devicex::debug(
              "    Op {} : {}", opOnPath->str(), opOnPath->name());
        }
        endpointId += 1;
      }

      // Find the ipu the op that consumes with tensor is on and create the
      // tensor on that graph
      std::vector<int64_t> ipus;
      for (auto *op : tensor->consumers.getOps()) {

        int64_t index = -1;
        if (op->getVirtualGraphId())
          index = *(op->getVirtualGraphId());

        // The copyToIpu op assume that the tensor will already
        // have been copied to the ipu from another op
        if (op->opid != Onnx::CustomOperators::IpuCopy) {

          auto &graph = getOpx(op->id)->graph();

          if (ipus.end() == std::find(ipus.begin(), ipus.end(), index)) {

            auto newTensor = graph.addVariable(
                popType(tensor->info), tensor->info.shape_szt(), tensor->str());
            poputil::mapTensorLinearly(graph, newTensor);

            tensors.insert(tensor->id, newTensor);
            linearlyCreatedInputTensors.insert(tensor->id);
            ipus.push_back(index);
          }
        }
      }
    };

    return {1e6, initTensorTaskId(tensor->id), {}, f};
  }
}

PriTask Devicex::initRandomSeed() {
  auto streamFromHostTask = [this]() {
    logging::devicex::debug("Initializing random seed.", randomSeedId);

    auto seedTensor =
        masterGraph().addVariable(poplar::UNSIGNED_INT, {2}, randomSeedId);
    masterGraph().setTileMapping(seedTensor, 0);

    auto &sq = progs.setRandomSeedFragment();

    if (!useSyntheticData()) {
      auto dataStream = rootGraph().addHostToDeviceFIFO(
          h2dId(randomSeedId),
          seedTensor.elementType(),
          seedTensor.numElements() * getReplicationFactor());

      sq.add(poplar::program::Copy(
          dataStream, rootGraph().getNonReplicatedTensor(seedTensor)));
    }

    poprand::setSeed(
        masterGraph(), seedTensor, 0, sq, fmt::format("{}/set", randomSeedId));
  };

  return {
      +1e6,              // high priority
      "initRandomSeed",  // name of this task
      {},                // depends on
      streamFromHostTask // what to run when the task is executed
  };
}

void Devicex::connectRandomSeedStream() {
  std::default_random_engine randomGenerator;
  auto replicationFactor = getReplicationFactor();

  auto callback = [randomGenerator, replicationFactor](void *ptr) mutable {
    std::uniform_int_distribution<uint64_t> distribution;
    uint64_t *data = reinterpret_cast<uint64_t *>(ptr);

    logging::devicex::debug("     Updating random seed");
    for (int i = 0; i < replicationFactor; i++) {
      data[i] = distribution(randomGenerator);
      logging::devicex::debug("       {}", data[i]);
    }
  };

  pEngine->connectStreamToCallback(h2dId(randomSeedId), callback);
}

template <typename T> void Devicex::setInitVal(Tensor *tensor) {

  auto nonReplicatedTensor =
      (rootGraph()).getNonReplicatedTensor(tensors.get(tensor->id));

  for (unsigned i = 0; i < getReplicationFactor(); ++i) {
    rootGraph().setInitialValue<T>(
        nonReplicatedTensor[i],
        poplar::ArrayRef<T>(
            static_cast<const T *>(tensor->tensorData()->data()),
            tensor->info.nelms()));
  }
}

// Using specialised poplar function for setting init val for FLOAT16
void Devicex::setInitValHalf(Tensor *tensor) {

  auto nonReplicatedTensor =
      (rootGraph()).getNonReplicatedTensor(tensors.get(tensor->id));

  for (unsigned i = 0; i < getReplicationFactor(); ++i) {
    rootGraph().setInitialValueHalf(
        nonReplicatedTensor[i],
        poplar::ArrayRef<uint16_t>(
            static_cast<const uint16_t *>(tensor->tensorData()->data()),
            tensor->info.nelms()));
  }
}

PriTask Devicex::setInitTensorValTask(Tensor *tensor) {
  // See T6254. Currently we just use setInitialValue for all constant tensors
  auto f = [this, tensor]() {
    // see T5925 for making a more compact way of matching
    // types than using this switch statement
    switch (tensor->info.dataType()) {
    case DataType::FLOAT: {
      setInitVal<float>(tensor);
      break;
    }
    case DataType::INT32: {
      setInitVal<int>(tensor);
      break;
    }
    case DataType::FLOAT16: {
      setInitValHalf(tensor);
      break;
    }

    case DataType::UNDEFINED:
    case DataType::UINT8:
    case DataType::INT8:
    case DataType::INT64:
    case DataType::BOOL:
    case DataType::UINT16:
    case DataType::INT16:
    case DataType::STRING:
    case DataType::DOUBLE:
    case DataType::UINT32:
    case DataType::UINT64:
    case DataType::COMPLEX64:
    case DataType::COMPLEX128:
    case DataType::BFLOAT16:
    default: {
      throw error(
          "setInitTensorValTask not implemented for Tensor {} of Type {}. ",
          tensor->id,
          tensor->info.data_type());
    }
    }
  };

  return {// priority unimportant
          0,
          // name of this task
          setInitTensorValTaskId(tensor->id),
          // poplar::Tensor must exist. Other that this, this task can be
          // performed any time
          {initTensorTaskId(tensor->id)},
          f};
}

PriTask Devicex::streamFromHostTask(Tensor *tensor) {
  auto f = [this, tensor]() {
    std::vector<int64_t> ipus;
    for (auto *op : tensor->consumers.getOps()) {

      // Assume another op will copy the tensor for an ipucopy
      if (op->opid != Onnx::CustomOperators::IpuCopy) {
        auto &graph = getOpx(op->id)->graph();

        int64_t index = -1;
        if (op->getVirtualGraphId())
          index = *(op->getVirtualGraphId());

        // Only stream the tensor once for all op's that consume it on an ipu
        if (std::find(ipus.begin(), ipus.end(), index) == ipus.end()) {

          logging::devicex::debug(
              "Creating host-to-device FIFO {} copied to ipu:{}",
              tensor->id,
              index);

          if (tensor->tensorType() == TensorType::Variable ||
              tensor->tensorType() == TensorType::Stream ||
              tensor->tensorType() == TensorType::Const) {
            fromHostStreams.emplace(
                tensor->id,
                rootGraph().addHostToDeviceFIFO(h2dId(tensor->id),
                                                popType(tensor->info),
                                                tensor->info.nelms()));
          } else if (tensor->tensorType() == TensorType::Const) {
            throw error("Constants are not streamed to device");
          } else {
            fromHostStreams.emplace(
                tensor->id,
                graph.addHostToDeviceFIFO(h2dId(tensor->id),
                                          popType(tensor->info),
                                          tensor->info.nelms()));
          }

          ipus.push_back(index);
        }
      }
    }
  };

  return {
      0,                                // priority unimportant
      streamFromHostTaskId(tensor->id), // name of this task
      {initTensorTaskId(tensor->id)},   // poplar::Tensor must exist
      f                                 // what to run when the task is executed
  };
}

PriTask Devicex::streamToHostTask(Tensor *tensor) {
  auto f = [this, tensor]() {
    logging::devicex::debug("Creating device-to-host FIFO {}", tensor->id);

    // TODO - figure out which graph the stream copy comes from
    // auto &graph = getOpx(tensor->getProducer()->id)->graph();
    auto &graph = rootGraph();

    toHostStreams.emplace(tensor->id,
                          graph.addDeviceToHostFIFO(d2hId(tensor->id),
                                                    popType(tensor->info),
                                                    tensor->info.nelms()));
  };

  return {
      0,                              // priority unimportant
      streamToHostTaskId(tensor->id), // name of this task
      {taskWhichCreates(tensor->id)}, // poplar::Tensor must exist
      f                               // what to run when the task is executed
  };
}

poplar::program::Sequence &Devicex::programFragment() {
  return progs.programFragment(PopPrograms::ProgramFragmentIndex::PROGRAM);
}

poplar::program::Sequence &Devicex::programFragment(const Scope &scope) {
  return progs.programFragment(scope);
}

PriTask Devicex::opTask(Op *op, double priority, TaskId prevOpTaskId) {

  OpId id  = op->id;
  Opx *opx = getOpx(id);

  // although priority should guarantee that this
  // task is only run after inputs are all created,
  // we add a dependency to the input tensors, just
  // in case someone plays with the priorities.
  // Moreover, we must state the copy-from-host deps
  std::vector<TaskId> deps;
  for (auto t_inds : op->input->indicesMap()) {
    Tensor *tensor = t_inds.first;

    auto creatorTask = taskWhichCreates(tensor->id);
    // Make sure we only add the creatorTask once in the dependency list
    if (std::find(deps.begin(), deps.end(), creatorTask) == deps.end())
      deps.push_back(creatorTask);

    // if the tensor is streamed on, we must wait
    // 'til the Copy has happened
    if (tensor->tensorType() == TensorType::Stream) {
      if (useSyntheticData() == false)
        deps.push_back(fromHostTaskId(tensor->id));
    }
  }

  // Depends on previous op task. This preserves op ordering from ir.
  // Note: the first opTask has no previous opTask
  if (prevOpTaskId != "") {
    // Add dependency only if not already added
    if (std::find(deps.begin(), deps.end(), prevOpTaskId) == deps.end()) {
      deps.push_back(prevOpTaskId);
    }
  }

  auto f = [opx, this]() {
    logging::devicex::debug("Creating output tensors for " +
                            opx->op_p->debugName());
    opx->grow(programFragment(opx->op_p->getScope()));
  };
  return {priority, opTaskId(op), deps, f};
}

InputCreatorCandidate::InputCreatorCandidate(
    int conIndex_,
    Opx *opx_,
    std::vector<OpxInAndOutIndex> pathFromInput_)
    : index(conIndex_), opx(opx_), pathFromInput(pathFromInput_) {}

std::vector<OpxInAndOutIndex> InputCreatorCandidate::getPathFromInput() {
  return pathFromInput;
}

unsigned Devicex::getReplicationFactor() const {

  unsigned replicationFactor = 1;
  if (ir().getSessionOptions().enableReplicatedGraphs) {
    replicationFactor =
        static_cast<unsigned>(ir().getSessionOptions().replicatedGraphCount);
  }
  return replicationFactor;
}

// go all the way to creating the engine and connecting streams
void Devicex::prepare() {

  logging::devicex::info("Poplar version: {}", poplar::versionString());
  logging::devicex::info("Poplar release githash: {}", poplar::packageHash());

  // Do not like the dynamic_cast is there a better way to handle this?
  auto &popDevice = dynamic_cast<DevicexInfo &>(*deviceInfo).getDevice();

  // Create the top level graph
  pRootGraph.reset(new poplar::Graph(popDevice));

  // Create the master graph
  logging::devicex::debug("Creating master graph with replication factor {}",
                          getReplicationFactor());

  pMasterGraph.reset(new poplar::Graph(
      pRootGraph->createReplicatedGraph(getReplicationFactor())));

  if (ir().getSessionOptions().enableVirtualGraphs) {
    auto numIPUs     = masterGraph().getTarget().getNumIPUs();
    auto tilesPerIPU = masterGraph().getTarget().getTilesPerIPU();

    for (unsigned ipu = 0; ipu < numIPUs; ++ipu) {
      unsigned startTile = ipu * tilesPerIPU;
      unsigned endTile   = (ipu + 1) * tilesPerIPU;
      virtualGraphs.emplace_back(
          masterGraph().createVirtualGraph(startTile, endTile));
      logging::devicex::info(
          "Created virtual graph {} from {} to {}", ipu, startTile, endTile);
    }

    // Make sure that the virtual graph information is valid
    for (Op *op : ir().getOpSchedule({})) {
      if (op->getVirtualGraphId()) {
        int64_t index = *(op->getVirtualGraphId());
        if (index < 0 || index >= numIPUs) {
          throw error("{} has been assigned to an invalid virtual graph {}",
                      op->debugName(),
                      index);
        }
      }
    }
  }

  popops::addCodelets(rootGraph());
  poplin::addCodelets(rootGraph());
  popnn::addCodelets(rootGraph());
  poprand::addCodelets(rootGraph());

  std::vector<Op *> ops = ir().getOpSchedule({});

  // create the scope programs
  for (auto op : ops) {
    if (!progs.containsFragment(op->getScope())) {
      progs.createFragment(op->getScope());
    }
  }

  // Outling the op's if the session options is enabled
  if (ir().getSessionOptions().enableOutlining) {
    ops = outline.getOutlineView(ops, ir());
  }

  // create an Opx for every Op
  for (Op *op : ops) {
    opxs[op->id] = createOpx(op);
  }

  PriTasks tasks;

  // weights (variables):
  // 1) make tensor,
  // 2) make stream from host,
  // 3) create write prog,
  // 4) make stream to host,
  // 5) create read prog.
  for (auto id : ir().getTensorIds(TensorType::Variable)) {
    Tensor *tensor = ir().getTensor(id);
    // 1
    tasks.add(initTensorTask(tensor));

    if (useSyntheticData() == false) {
      // 2
      tasks.add(streamFromHostTask(tensor));
      // 3
      tasks.add(fromHostTask(tensor,
                             progs.streamWeightsFromHostFragment(),
                             progs.copyWeightsBetweenIpusFragment()));
      // 4
      tasks.add(streamToHostTask(tensor));
      // 5
      tasks.add(toHostTask(tensor, progs.weightsToHostFragment()));
    }
  }

  // constants:
  // 1) make tensor,
  // 2) set initial value.
  for (auto id : ir().getTensorIds(TensorType::Const)) {
    Tensor *tensor = ir().getTensor(id);
    // 1
    tasks.add(initTensorTask(tensor));
    // 2
    tasks.add(setInitTensorValTask(tensor));
  }

  // stream-to-device tensors : 1)  make tensor 2) make stream
  for (auto id : ir().getTensorIds(TensorType::Stream)) {
    Tensor *tensor = ir().getTensor(id);
    // 1
    tasks.add(initTensorTask(tensor));

    if (useSyntheticData() == false) {
      // 2
      tasks.add(streamFromHostTask(tensor));
    }
  }

  // graph inputs : 1) make tensor
  for (auto id : ir().getGraphInputIds()) {
    Tensor *tensor = ir().getTensor(id);
    // 1
    tasks.add(initTensorTask(tensor));
  }

  // Init the random seed
  tasks.add(initRandomSeed());

  // Depending on anchor return types specified by the user, some
  // tensors may need to be added to the graph to keep track of
  // batch count.
  if (ir().getDataFlow().isBatchCountingRequired()) {
    tasks.add(initBatchCounterTensorsTask());
    tasks.add(updateBatchCoutTask(progs.programFragment()));
  }

  // stream-to-host tensors : 1) make streams 2) make copy programs
  // note that the order in which tasks are added does not matter,
  // they will be topologically sorted before running
  if (useSyntheticData() == false) {
    for (auto anchorId : ir().getDataFlow().anchors()) {
      Tensor *tensor = ir().getTensor(anchorId);

      // 1
      tasks.add(streamToHostTask(tensor));
      // 2
      switch (ir().getDataFlow().art(anchorId).id()) {
      // Copy program runs after every batch
      case (AnchorReturnTypeId::ALL): {
        tasks.add(toHostTask(tensor, programFragment()));
        break;
      }
      // Copy program runs at the end of the step
      case (AnchorReturnTypeId::FINAL): {
        tasks.add(toHostEveryNBatchesTask(
            tensor, ir().getDataFlow().batchesPerStep(), programFragment()));
        break;
      }
      // Copy program runs at the end of every N batches
      case (AnchorReturnTypeId::EVERYN): {
        tasks.add(toHostEveryNBatchesTask(
            tensor, ir().getDataFlow().art(anchorId).rp(), programFragment()));
        break;
      }
      }
    }

    // create Program to write optimizer tensors to device
    for (Tensor *tensor : ir().optimizerTensors()) {
      tasks.add(fromHostTask(tensor,
                             progs.streamOptimizerFromHostFragment(),
                             progs.copyOptimizerBetweenIpusFragment()));
    }

    for (Tensor *tensor : ir().dataStreamTensors()) {
      tasks.add(fromHostTask(tensor, programFragment(), programFragment()));
    }
  }

  double priority     = 0.;
  TaskId prevOpTaskId = "";
  // 'ops' are in the order of the Ir's schedule
  for (int i = 0; i < ops.size(); ++i) {
    Op *op    = ops[i];
    auto task = opTask(op, priority, prevOpTaskId);
    tasks.add(task);
    prevOpTaskId = task.name;
    priority -= 1.;
  }

  for (auto &task : tasks.getLinearised()) {
    task.f();
  }

  if (ir().getSessionOptions().exportPoplarVertexGraph) {
    std::ofstream strm;
    strm.open("poplar_vertex_graph.dot", std::ios::out);
    masterGraph().outputVertexGraph(strm, progs.progs());
  }

  if (ir().getSessionOptions().exportPoplarComputationGraph) {
    std::ofstream strm;
    strm.open("poplar_compute_graph.dot", std::ios::out);
    masterGraph().outputComputeGraph(strm, progs.progs());
  }

  if (!ir().getSessionOptions().compileEngine) {
    logging::devicex::info("Not compiling engine by request");
    return;
  }

  logging::devicex::info("Starting Engine compilation");

  auto progressLogger = [](int progress, int total) {
    if (total != 0) {
      float percentage = std::floor(100.0f * static_cast<float>(progress) /
                                    static_cast<float>(total));
      logging::devicex::debug("Engine compilation {}% complete", percentage);
    }
  };

  // Enigma moves the graph into the engine and then set the graphs to 0
  pEngine.reset(new poplar::Engine(
      rootGraph(), progs.progs(), engineOptions, progressLogger));
  logging::devicex::info("Engine compiled");

  pEngine->load(popDevice);
  logging::devicex::info("Engine loaded");

  if (useSyntheticData() == false) {
    logging::devicex::debug("Connecting initializer streams");
    for (auto id : ir().getTensorIds(TensorType::Variable)) {
      Tensor *tensor = ir().getTensor(id);
      pEngine->connectStream(h2dId(id), tensor->tensorData()->data());
    }

    // Random seed
    connectRandomSeedStream();

    logging::devicex::debug("Connecting optimizer streams");
    for (Tensor *tensor : ir().optimizerTensors()) {
      pEngine->connectStream(h2dId(tensor->id), tensor->tensorData()->data());
    }

    auto engineToStream =
        [this](char *data0, int64_t n_bytes, PopStreamId streamId) {
          // Poplar has no const void * version, disappointing
          auto addr0 = static_cast<void *>(data0);
          auto addr1 = static_cast<void *>(data0 + n_bytes);
          // connect the stream (circular buffer)
          pEngine->connectStream(streamId, addr0, addr1);
        };

    logging::devicex::debug(
        "Creating host buffers for h2d streams, and connecting");
    for (Tensor *tensor : ir().dataStreamTensors()) {
      PopStreamId streamId = h2dId(tensor->id);
      // allocate host memory, where the poplar::Stream will read data from
      int64_t n_bytes = ir().getDataFlow().batchesPerStep() *
                        tensor->info.nbytes() * getReplicationFactor();
      h2dBuffers[tensor->id] = std::vector<char>(n_bytes);
      char *data0            = h2dBuffers[tensor->id].data();
      engineToStream(data0, n_bytes, streamId);
    }

    logging::devicex::debug(
        "Creating host buffers for anchor d2h streams, connecting");
    for (TensorId anchorId : ir().getDataFlow().anchors()) {
      PopStreamId streamId = d2hId(anchorId);
      Tensor *tensor       = ir().getTensor(anchorId);
      int64_t batch_bytes  = tensor->info.nbytes();
      int64_t n_bytes;
      switch (ir().getDataFlow().art(anchorId).id()) {
      case (AnchorReturnTypeId::FINAL): {
        n_bytes = batch_bytes * getReplicationFactor();
        break;
      }
      case (AnchorReturnTypeId::EVERYN): {
        n_bytes = batch_bytes *
                  (ir().getDataFlow().batchesPerStep() /
                   ir().getDataFlow().art(anchorId).rp()) *
                  getReplicationFactor();
        break;
      }
      case (AnchorReturnTypeId::ALL): {
        n_bytes = batch_bytes * ir().getDataFlow().batchesPerStep() *
                  getReplicationFactor();
        break;
      }
      }
      d2hBuffers[anchorId] = std::vector<char>(n_bytes);
      char *data0          = d2hBuffers[tensor->id].data();
      engineToStream(data0, n_bytes, streamId);
    }

    logging::devicex::debug(
        "Creating host buffers for weight d2h streams, connecting");

    for (auto initId : ir().getTensorIds(TensorType::Variable)) {
      PopStreamId streamId = d2hId(initId);
      Tensor *tensor       = ir().getTensor(initId);
      int64_t n_bytes      = tensor->info.nbytes();
      d2hBuffers[initId]   = std::vector<char>(n_bytes);
      char *data0          = d2hBuffers[initId].data();
      engineToStream(data0, n_bytes, streamId);
    }
  }

  prepareHasBeenCalled = true;
}

TaskId Devicex::streamFromHostTaskId(TensorId id) const {
  return "streamFromHostTask_" + id;
}

TaskId Devicex::setInitTensorValTaskId(TensorId id) const {
  return "setInitTensorValTask_" + id;
}

TaskId Devicex::streamToHostTaskId(TensorId id) const {
  return "streamToHostTask_" + id;
}

TaskId Devicex::fromHostTaskId(TensorId id) const {
  return "fromHostTask_" + id;
}

TaskId Devicex::toHostTaskId(TensorId id) const { return "toHostTask_" + id; }

TaskId Devicex::initBatchCounterTensorsTaskId() const {
  return "initBatchCounterTensorsTask";
}

TaskId Devicex::updateBatchCoutTaskId() const { return "updateBatchCoutTask"; }

TaskId Devicex::initTensorTaskId(TensorId id) const {
  return "initTensorTaskId_" + id;
}

TaskId Devicex::opTaskId(Op *op) const {

  std::stringstream ss;
  ss << "fromOpTask_" << op->id << '_' << op->opid;
  return ss.str();
}

PopStreamId Devicex::h2dId(TensorId id) const { return "h2d_" + id; }

PopStreamId Devicex::d2hId(TensorId id) const { return "d2h_" + id; }

// For a replicated tensor we stream the tensor into the first replicated
// graph (0) and then copy that tensor to the other replicated graphs
//
// We also want all the stream'ed copies to be contigous and all the
// inter-replicated graphs copied to be contigous so that poplar can combine
// them together. It does not combine if we interleave them. So we pass the
// streamSq and the copySq seperately so collected together and executed one
// after the other.

PriTask Devicex::fromHostTask(Tensor *tensor,
                              poplar::program::Sequence &streamSq,
                              poplar::program::Sequence &copySq) const {

  auto f = [&streamSq, &copySq, tensor, this]() {
    bool rearrange_on_host = tensor->tensorType() != TensorType::Stream;

    logging::devicex::debug("Adding poplar::program::Copy from host " +
                            tensor->id);

    // getNonReplicatedTensor is not a const method so have to have a const_cast
    // T8378
    auto nonReplicatedTensor =
        const_cast<poplar::Graph &>(rootGraph())
            .getNonReplicatedTensor(tensors.get(tensor->id));

    if (tensor->tensorType() == TensorType::Variable) {

      // Copy the variable from the stream into the non replicated tensor index
      // 0 then copy it to the the other indices
      streamSq.add(poplar::program::Copy(
          fromHostStreams.at(tensor->id), nonReplicatedTensor[0], true));

      for (unsigned i = 1; i < getReplicationFactor(); ++i) {
        copySq.add(poplar::program::Copy(nonReplicatedTensor[0],
                                         nonReplicatedTensor[i]));
      }
    } else {

      // For a stream we copy 'n' lots of data from the stream into each index
      // for the replicated tensor
      for (unsigned i = 0; i < getReplicationFactor(); ++i) {
        streamSq.add(poplar::program::Copy(fromHostStreams.at(tensor->id),
                                           nonReplicatedTensor[i],
                                           rearrange_on_host));
      }
    }
  };

  return {-1e6, // writes to device: always as late as possible
          fromHostTaskId(tensor->id),
          {
              streamFromHostTaskId(tensor->id), // poplar::Stream created
              initTensorTaskId(tensor->id)      // poplar::Tensor created
          },
          f};
}

PriTask Devicex::toHostTask(Tensor *tensor,
                            poplar::program::Sequence &sq) const {

  auto f = [&sq, tensor, this]() {
    logging::devicex::debug("Adding poplar::program::Copy to host " +
                            tensor->id);

    bool rearrange_on_host = tensor->tensorType() != TensorType::Stream;

    // getNonReplicatedTensor is not a const method
    // T8378
    auto nonReplicatedTensor =
        const_cast<poplar::Graph &>(rootGraph())
            .getNonReplicatedTensor(tensors.get(tensor->id));

    if (tensor->tensorType() == TensorType::Variable) {
      // Copy from the first replicated graph (all graphs should be in sync
      // and therefore have similar values).
      sq.add(poplar::program::Copy(
          nonReplicatedTensor[0], toHostStreams.at(tensor->id), true));
    } else {
      // Copy from the each of the replicated graphs
      for (unsigned i = 0; i < getReplicationFactor(); ++i) {
        sq.add(poplar::program::Copy(nonReplicatedTensor[i],
                                     toHostStreams.at(tensor->id),
                                     rearrange_on_host));
      }
    }
  };

  return {+1e6, // writes to host: always as early as possible
          toHostTaskId(tensor->id),
          {
              // the dependencies:
              streamToHostTaskId(tensor->id), // poplar::Stream creation task,
              taskWhichCreates(tensor->id)    // poplar::Tensor creation task.
          },
          f};
}

PriTask Devicex::initBatchCounterTensorsTask() {

  auto f = [this]() {
    logging::devicex::debug("Adding batch counter tensors");

    // Add scalar tensors outside of the ir to track the batch
    // Id and decide when to execute the copy to the host
    for (ReturnPeriod N : ir().getDataFlow().rps()) {
      // Add to map so copy task can access
      batchCountingTensors[N] = masterGraph().addVariable(poplar::INT, {});
      batchCountCheckingTensors[N] =
          masterGraph().addVariable(poplar::BOOL, {});

      getConst(poplar::INT, {}, N, "batchCounter");

      poputil::mapTensorLinearly(masterGraph(), batchCountingTensors[N]);
      poputil::mapTensorLinearly(masterGraph(), batchCountCheckingTensors[N]);
    }

    // Make sure const 1 tensor exists
    getConst(poplar::INT, {}, 1, "one");
  };

  return {+1e6, // followed by writes to host: always as early as possible
          initBatchCounterTensorsTaskId(),
          {},
          f};
}

PriTask Devicex::updateBatchCoutTask(poplar::program::Sequence &sq) {

  auto f = [&sq, this]() {
    logging::devicex::debug("Adding batch count checker program");

    // Placeholder 'do nothing' branch if not running assign program
    poplar::program::Sequence emptyseq;

    // Increment the batch count at the at the earliest point
    // the anchor tensor is required, and check if it is a
    // copy batch
    for (ReturnPeriod N : ir().getDataFlow().rps()) {
      popops::addInPlace(masterGraph(),
                         batchCountingTensors[N],
                         getConst(poplar::INT, {}, 1, "batchCount/one"),
                         sq);

      batchCountCheckingTensors[N] =
          popops::eq(masterGraph(),
                     batchCountingTensors[N],
                     getConst(poplar::INT, {}, N, "batchCount/n"),
                     sq);

      // Reset batch count once it has reached N
      auto zero = getConst(poplar::INT, {}, 0, "batchCount/zero");
      sq.add(poplar::program::If(
          batchCountCheckingTensors[N],
          poplar::program::Copy(zero, batchCountingTensors[N]),
          emptyseq));
    }
  };

  return {+1e6, // followed by writes to host: always as early as possible
          updateBatchCoutTaskId(),
          {
              initBatchCounterTensorsTaskId() // poplar::Tensor creation task
          },
          f};
}

PriTask Devicex::toHostEveryNBatchesTask(Tensor *tensor,
                                         int N,
                                         poplar::program::Sequence &sq) {

  auto f = [&sq, tensor, N, this]() {
    logging::devicex::debug(
        "Adding conditional poplar::program::Copy to host " + tensor->id);

    poplar::Tensor isNthBatch = batchCountCheckingTensors.at(N);

    poplar::program::Sequence copyseq;

    bool rearrange_on_host = tensor->tensorType() != TensorType::Stream;

    auto nonReplicatedTensor =
        rootGraph().getNonReplicatedTensor(tensors.get(tensor->id));

    // Program to copy the anchor tensor and reset batch count

    if (tensor->tensorType() == TensorType::Variable) {
      // Copy from the first replicated graph (all graphs should be in sync
      // and therefore have similar values).
      copyseq.add(poplar::program::Copy(
          nonReplicatedTensor[0], toHostStreams.at(tensor->id), true));
    } else {

      // Copy from the each of the replicated graphs
      for (unsigned i = 0; i < getReplicationFactor(); ++i) {
        copyseq.add(poplar::program::Copy(nonReplicatedTensor[i],
                                          toHostStreams.at(tensor->id),
                                          rearrange_on_host));
      }
    }

    // Placeholder 'do nothing' branch if not running copy program
    poplar::program::Sequence emptyseq;

    sq.add(poplar::program::If(isNthBatch, copyseq, emptyseq));
  };

  return {+1e6, // writes to host: always as early as possible
          toHostTaskId(tensor->id),
          {
              // the dependencies:
              updateBatchCoutTaskId(),        // updating poplar::Tensor task,
              streamToHostTaskId(tensor->id), // poplar::Stream creation task,
              taskWhichCreates(tensor->id)    // poplar::Tensor creation task.
          },
          f};
}

std::string Devicex::getSummaryReport() const {
  if (pEngine == nullptr) {
    throw error(
        "Session must have been prepared before a report can be fetched");
  }
  const auto &g_prof = pEngine->getGraphProfile();
  const auto &e_prof = pEngine->getExecutionProfile();

  std::stringstream ss;
  printProfileSummary(ss, g_prof, e_prof, reportOptions);

  pEngine->resetExecutionProfile();
  return ss.str();
}

std::string Devicex::getGraphReport(bool use_cbor) const {
  if (pEngine == nullptr) {
    throw error(
        "Session must have been prepared before a report can be fetched");
  }
  std::stringstream ss;
  auto report = pEngine->getGraphProfile();
  if (use_cbor) {
    serializeToCBOR(ss, report);
  } else {
    serializeToJSON(ss, report);
  }

  return ss.str();
}

std::string Devicex::getExecutionReport(bool use_cbor) const {
  if (pEngine == nullptr) {
    throw error(
        "Session must have been prepared before a report can be fetched");
  }
  std::stringstream ss;
  auto report = pEngine->getExecutionProfile();

  if (use_cbor) {
    serializeToCBOR(ss, report);
  } else {
    serializeToJSON(ss, report);
  }

  pEngine->resetExecutionProfile();
  return ss.str();
}

TensorTileMap Devicex::getTensorTileMap() const {
  TensorTileMap map;
  for (const auto &t : tensors.getTensors()) {
    std::vector<TensorIntervalList> mapping;
    for (auto tile : pMasterGraph->getTileMapping(t.second)) {
      TensorIntervalList intervalList;
      std::transform(tile.begin(),
                     tile.end(),
                     std::back_inserter(intervalList),
                     [](poplar::Interval i) {
                       return std::pair<size_t, size_t>(i.begin(), i.end());
                     });
      mapping.emplace_back(intervalList);
    }
    map.insert(std::make_pair(t.first, mapping));
  }
  return map;
}

poplar::Type popType(const TensorInfo &info) {
  switch (info.dataType()) {
  case DataType::FLOAT: {
    return poplar::FLOAT;
  }
  case DataType::INT32: {
    return poplar::INT;
  }
  case DataType::FLOAT16: {
    return poplar::HALF;
  }
  case DataType::BOOL: {
    return poplar::BOOL;
  }

  case DataType::UNDEFINED:
  case DataType::UINT8:
  case DataType::INT8:
  case DataType::UINT16:
  case DataType::INT16:
  case DataType::INT64:
  case DataType::STRING:
  case DataType::BFLOAT16:
  case DataType::DOUBLE:
  case DataType::UINT32:
  case DataType::UINT64:
  case DataType::COMPLEX64:
  case DataType::COMPLEX128:
  default:
    throw error("Is there a poplar type for " + info.data_type() + "?");
  }
}

// piggy-backing on TensorInfo's data_type()
// function to get a string of the DataType
poplar::Type popType(DataType type) { return popType(TensorInfo(type, {1})); }

std::set<TensorId> Devicex::getLinearlyCreatedInputTensors() const {
  return linearlyCreatedInputTensors;
}
std::set<TensorId> Devicex::getEfficientlyCreatedInputTensors() const {
  return efficientlyCreatedInputTensors;
}

bool Devicex::useSyntheticData() {
  return (ir().getSessionOptions().ignoreData);
}

} // namespace popx
} // namespace poponnx

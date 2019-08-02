#ifndef GUARD_NEURALNET_ONNXCONSTEXPR_HPP
#define GUARD_NEURALNET_ONNXCONSTEXPR_HPP

namespace popart {

class Graph;

class OnnxConstExprUtil {
public:
  static bool isConst(const onnx::NodeProto &);
  static void processNode(const onnx::NodeProto &, Graph *);

private:
  static void processConstantNode(const onnx::NodeProto &, Graph *);
  static void processShapeNode(const onnx::NodeProto &, Graph *);
  static void processConstantOfShapeNode(const onnx::NodeProto &, Graph *);
};

} // namespace popart

#endif
// Copyright (c) 2020 Graphcore Ltd. All rights reserved.
#define BOOST_TEST_MODULE GraphEdgeMapTest
#include <boost/test/unit_test.hpp>

#include <map>
#include <memory>
#include <sstream>
#include <utility>
#include <vector>

#include <popart/filereader.hpp>
#include <popart/graph.hpp>
#include <popart/graphid.hpp>
#include <popart/ir.hpp>
#include <popart/opidentifier.hpp>
#include <popart/scheduler.hpp>
#include <popart/tensorinfo.hpp>
#include <popart/topocons.hpp>

#include "dummy_test_graphs.hpp"

using namespace popart;

using EdgeMap = std::map<OpId, std::unordered_set<OpId>>;

namespace {

std::string mkFailureMsg(const EdgeMap &expected, const EdgeMap &actual) {

  const auto append = [](std::ostringstream &oss, const EdgeMap &edgeMap) {
    oss << "[ ";

    for (const auto &consumersOfOpId : edgeMap) {
      oss << "{ " << consumersOfOpId.first << ": [ ";
      for (const auto &opid : consumersOfOpId.second) {
        oss << opid << " ";
      }
      oss << "] }, ";
    }

    oss << " ]";
  };

  std::ostringstream oss;

  oss << "critical check expectedMap == actualMap has failed  ";
  append(oss, expected);
  oss << "  !=  ";
  append(oss, actual);

  return oss.str();
}

template <typename TC> struct EdgeMapTestCase {
  Ir ir;
  Graph graph = {ir, GraphId::root()};

  // Subclasses implement `initTestGraph_` and `mkExpectedEdges_`.

  void initTestGraph() {
    TC &tc = static_cast<TC &>(*this);
    tc.initTestGraph_();
  }

  EdgeMap mkExpectedEdges() {
    TC &tc = static_cast<TC &>(*this);
    return tc.mkExpectedEdges_();
  }

protected:
  EdgeMap::value_type mp(const EdgeMap::value_type::first_type &&a,
                         const EdgeMap::value_type::second_type &&b) {
    return std::make_pair(std::move(a), std::move(b));
  }
};

struct SingleOpTestCase : EdgeMapTestCase<SingleOpTestCase> {

  void initTestGraph_() { test_graphs::initSingleOpTestGraph(graph); }

  EdgeMap mkExpectedEdges_() {
    return {mp(0, {})}; // add0 is OpId 0; has no dependents.
  }
};

struct DiamondTestCase : EdgeMapTestCase<DiamondTestCase> {

  void initTestGraph_() { test_graphs::initDiamondTestGraph(graph); }

  EdgeMap mkExpectedEdges_() {
    return {
        mp(0, {1, 2, 3}), // add0
        mp(1, {3, 4}),    // relu1
        mp(2, {3, 4, 6}), // conv2
        mp(3, {4}),       // LRN3
        mp(4, {5, 6}),    // concat4 (no dependents)
        mp(5, {6}),       // nll5
        mp(6, {})         // nllgrad6
    };
  }
};

} // namespace

using TestCaseTypes = std::tuple<SingleOpTestCase, DiamondTestCase>;

BOOST_AUTO_TEST_CASE_TEMPLATE(GraphEdgeMapTest, TestCase, TestCaseTypes) {
  TestCase tc;

  tc.initTestGraph();
  const auto expectedMap = tc.mkExpectedEdges();
  const auto actualMap   = tc.graph.getEdgeMap();

  // NB: BOOST_REQUIRE_EQUAL can't handle printing a map<OpId, unordered_set>,
  // so we construct a nice error message manually.
  if (expectedMap != actualMap) {
    BOOST_FAIL(mkFailureMsg(expectedMap, actualMap));
  }
}

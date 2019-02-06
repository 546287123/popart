#include <poponnx/logging.hpp>
#include <poponnx/patterns/patterns.hpp>

namespace poponnx {

Patterns::Patterns(PatternsLevel level) {

  switch (level) {
  case PatternsLevel::NONE: {
  } break;

  // The default set of patterns
  case PatternsLevel::DEFAULT:
  case PatternsLevel::ALL: {
    // right now we will enable all the options, maybe later there will be a
    // subset
    auto patternList = PreAliasPatternManager::getPatternList();
    for (auto pattern : patternList) {
      settings.insert(std::pair<PreAliasPatternType, bool>(pattern, true));
    }
    inplaceEnabled = true;
  } break;
  }
}

Patterns::Patterns(std::vector<PreAliasPatternType> types) {

  for (auto type : types) {
    settings.insert(std::pair<PreAliasPatternType, bool>(type, true));
  }
}

Patterns Patterns::create(std::vector<std::string> strings) {
  Patterns patterns(PatternsLevel::NONE);

  for (auto p : strings) {
    if (p == "InPlace") {
      patterns.enableInPlace(true);
    } else {
      auto type = PreAliasPatternManager::convertPreAliasPatternType(p);
      if (type) {
        patterns.settings.insert(
            std::pair<PreAliasPatternType, bool>(*type, true));
      } else {
        if (p == "Inplace") {
          throw error("Unknown pattern {}, did you mean InPlace?", p);
        } else {
          throw error("Unknown pattern {}", p);
        }
      }
    }
  }

  return patterns;
}

bool Patterns::isPatternEnabled(PreAliasPatternType t) {

  auto it = settings.find(t);
  if (it != settings.end()) {
    return it->second;
  }

  return false;
}

Patterns &Patterns::enablePattern(PreAliasPatternType t, bool v) {
  logging::ir::warn("Pattern {} {}", static_cast<int>(t), v);
  settings[t] = v;
  return *this;
}

std::vector<std::unique_ptr<PreAliasPattern>> Patterns::getPreAliasList() {

  std::vector<std::unique_ptr<PreAliasPattern>> patterns;

  for (auto p : settings) {
    if (p.second) {
      patterns.emplace_back(PreAliasPatternManager::createPattern(p.first));
    }
  }

  return patterns;
}

std::ostream &operator<<(std::ostream &os, const Patterns &patterns) {

  for (auto setting : patterns.settings) {
    os << PreAliasPatternManager::getPatternName(setting.first) << " ";
  }

  return os;
}

} // namespace poponnx

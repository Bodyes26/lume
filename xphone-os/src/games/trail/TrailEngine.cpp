#include <cstddef>
#include "TrailEngine.h"

namespace trail {

TrailEngine::TrailEngine() {
  _save.clear();
}

bool TrailEngine::loadStory(const uint8_t* storyData, uint32_t storySize) {
  _reader = StoryReader(storyData, storySize);
  if (!_reader.isValid()) {
    _state = EngineState::Unloaded;
    return false;
  }
  return true;
}

bool TrailEngine::startNewGame() {
  if (!_reader.isValid()) return false;
  _save.clear();

  const auto* meta = _reader.meta();
  const auto* header = _reader.header();
  if (!meta || !header) return false;

  _save.storyHash = storyHash(header->id);
  _save.chapter = 0;
  _save.nodeId = 0;
  _save.day = 1;
  _save.distance = 0;

  // Initialize start resources
  for (int i = 0; i < meta->resourceCount && i < kMaxResources; ++i) {
    _save.resources[i] = meta->resources[i].start;
  }

  // Initialize companions (all start present with 100 health by default, or as defined)
  for (int i = 0; i < meta->companionCount && i < kMaxCompanions; ++i) {
    _save.setCompanion(i, true, 100);
  }

  if (!loadChapter(0)) return false;
  return advanceNode(0);
}

bool TrailEngine::restoreGame(const TrailSave& save) {
  if (!_reader.isValid()) return false;
  const auto* header = _reader.header();
  if (!header || save.storyHash != storyHash(header->id)) return false;

  _save = save;
  if (!loadChapter(_save.chapter)) return false;
  return advanceNode(_save.nodeId);
}

void TrailEngine::createSave(TrailSave& out) const {
  out = _save;
  // Compute checksum over preceding bytes before checksum field
  out.checksum = crc16(reinterpret_cast<const uint8_t*>(&out), offsetof(TrailSave, checksum));
}

bool TrailEngine::loadChapter(int chapterIndex) {
  if (!_reader.isValid()) return false;
  _chapterData = _reader.chapterData(chapterIndex, &_chapterBytes);
  if (!_chapterData || _chapterBytes < sizeof(ChapterHeader)) return false;

  memcpy(&_chapterHeader, _chapterData, sizeof(ChapterHeader));
  _save.chapter = static_cast<uint16_t>(chapterIndex);
  return true;
}

bool TrailEngine::advanceNode(uint16_t targetNodeId) {
  if (!_chapterData) return false;
  if (targetNodeId >= _chapterHeader.nodeCount) return false;

  if (!parseNode(targetNodeId, _currentNode)) return false;
  _save.nodeId = targetNodeId;

  // Transition state based on node type
  switch (_currentNode.type) {
    case NodeType::Narrative:
      _state = EngineState::Narrative;
      break;
    case NodeType::Minigame:
      if (_currentNode.minigameId >= 0) {
        startMinigame(_currentNode.minigameId);
      } else {
        _state = EngineState::Narrative;
      }
      break;
    case NodeType::ChapterEnd:
      _state = EngineState::ChapterEnded;
      break;
    case NodeType::GameOver:
      _state = EngineState::GameOver;
      break;
    case NodeType::Event:
    case NodeType::Check:
    case NodeType::Shop:
      _state = EngineState::Narrative;
      break;
  }
  return true;
}

bool TrailEngine::makeChoice(int choiceIndex, uint32_t randomRoll) {
  if (_state != EngineState::Narrative) return false;
  if (choiceIndex < 0 || choiceIndex >= _currentNode.choiceCount) return false;

  const Choice& ch = _currentNode.choices[choiceIndex];
  if (!evaluateConditions(ch.conditions, ch.conditionCount)) return false;

  // Apply choice effects
  applyEffects(ch.effects, ch.effectCount);

  // Minigame triggered by choice?
  if (ch.minigameId >= 0) {
    return startMinigame(ch.minigameId, randomRoll);
  }

  // Probability roll
  uint16_t next = ch.nextNode;
  if (ch.probability > 0 && ch.probability < 100) {
    uint32_t roll = (randomRoll ? randomRoll : 42) % 100;
    if (roll >= ch.probability) {
      next = ch.failNode;
    }
  }

  return advanceNode(next);
}

bool TrailEngine::advanceAuto(uint32_t /*randomRoll*/) {
  if (_currentNode.type == NodeType::Event) {
    applyEffects(_currentNode.effects, _currentNode.effectCount);
    return advanceNode(_currentNode.nextNode);
  }

  if (_currentNode.type == NodeType::Check) {
    bool pass = evaluateConditions(_currentNode.conditions, _currentNode.conditionCount);
    return advanceNode(pass ? _currentNode.passNode : _currentNode.failNode);
  }

  if (_currentNode.type == NodeType::ChapterEnd) {
    int nextChap = _save.chapter + 1;
    const auto* header = _reader.header();
    if (header && nextChap >= header->chapterCount) {
      _state = EngineState::StoryWon;
      return true;
    }
    if (loadChapter(nextChap)) {
      return advanceNode(0);
    }
    return false;
  }

  return false;
}

int16_t TrailEngine::resource(int id) const {
  if (id < 0 || id >= kMaxResources) return 0;
  return _save.resources[id];
}

void TrailEngine::setResource(int id, int16_t val) {
  if (id < 0 || id >= kMaxResources) return;
  const auto* meta = _reader.meta();
  if (meta && id < meta->resourceCount) {
    if (val < 0) val = 0;
    if (val > meta->resources[id].max) val = meta->resources[id].max;
  }
  _save.resources[id] = val;
}

bool TrailEngine::isResourceCritical(int id) const {
  if (id < 0 || id >= kMaxResources) return false;
  const auto* meta = _reader.meta();
  if (!meta || id >= meta->resourceCount) return false;
  return _save.resources[id] <= meta->resources[id].critical;
}

const char* TrailEngine::stringAt(uint16_t offset) const {
  if (!_chapterData) return "";
  uint32_t strBase = _chapterHeader.stringsOffset;
  if (strBase + offset >= _chapterBytes) return "";
  return reinterpret_cast<const char*>(_chapterData + strBase + offset);
}

const char* TrailEngine::nodeTitle() const {
  return stringAt(_currentNode.titleOff);
}

const char* TrailEngine::nodeText() const {
  return stringAt(_currentNode.textOff);
}

const char* TrailEngine::choiceLabel(int index) const {
  if (index < 0 || index >= _currentNode.choiceCount) return "";
  return stringAt(_currentNode.choices[index].labelOff);
}

bool TrailEngine::isChoiceAvailable(int index) const {
  if (index < 0 || index >= _currentNode.choiceCount) return false;
  const Choice& ch = _currentNode.choices[index];
  return evaluateConditions(ch.conditions, ch.conditionCount);
}

bool TrailEngine::evaluateCondition(const Condition& c) const {
  switch (c.type) {
    case CondType::ResourceMin:
      return resource(c.id) >= c.value;
    case CondType::ResourceMax:
      return resource(c.id) <= c.value;
    case CondType::CompanionAlive:
      return companionPresent(c.id) && companionHealth(c.id) > 0;
    case CondType::CompanionDead:
      return !companionPresent(c.id) || companionHealth(c.id) <= 0;
    case CondType::FlagSet:
      return flag(c.id);
    case CondType::FlagClear:
      return !flag(c.id);
  }
  return true;
}

bool TrailEngine::evaluateConditions(const Condition* conds, uint8_t count) const {
  for (int i = 0; i < count && i < kMaxConditions; ++i) {
    if (!evaluateCondition(conds[i])) return false;
  }
  return true;
}

void TrailEngine::applyEffect(const Effect& e) {
  switch (e.type) {
    case EffectType::ResourceDelta:
      setResource(e.id, resource(e.id) + e.value);
      break;
    case EffectType::ResourceSet:
      setResource(e.id, e.value);
      break;
    case EffectType::CompanionJoin:
      _save.setCompanion(e.id, true, e.value > 0 ? e.value : 100);
      break;
    case EffectType::CompanionLeave:
      _save.setCompanion(e.id, false, 0);
      break;
    case EffectType::CompanionHealth: {
      int curH = _save.companionHealth(e.id);
      int newH = curH + e.value;
      if (newH <= 0) {
        _save.setCompanion(e.id, false, 0);
      } else {
        _save.setCompanion(e.id, true, newH);
      }
      break;
    }
    case EffectType::FlagSet:
      setFlag(e.id, true);
      break;
    case EffectType::FlagClear:
      setFlag(e.id, false);
      break;
    case EffectType::Distance:
      _save.distance += e.value;
      break;
    case EffectType::Day:
      _save.day += e.value;
      break;
  }
}

void TrailEngine::applyEffects(const Effect* effs, uint8_t count) {
  for (int i = 0; i < count && i < kMaxEffects; ++i) {
    applyEffect(effs[i]);
  }
}

bool TrailEngine::parseNode(uint16_t nodeId, Node& out) {
  if (!_chapterData || nodeId >= _chapterHeader.nodeCount) return false;
  uint32_t nodeOffset = _chapterHeader.nodesOffset + nodeId * sizeof(Node);
  if (nodeOffset + sizeof(Node) > _chapterBytes) return false;

  memcpy(&out, _chapterData + nodeOffset, sizeof(Node));
  return true;
}

bool TrailEngine::parseMinigame(int minigameIdx) {
  if (!_chapterData || minigameIdx < 0 || minigameIdx >= _chapterHeader.minigameCount) return false;

  uint32_t mgBase = _chapterHeader.minigamesOffset;
  // Seek to minigame descriptor by parsing header
  const uint8_t* p = _chapterData + mgBase;
  for (int i = 0; i < minigameIdx; ++i) {
    const auto* desc = reinterpret_cast<const MinigameDescriptor*>(p);
    uint32_t size = sizeof(MinigameDescriptor) + desc->setupLen;
    for (int k = 0; k < 6; ++k) size += desc->keyLens[k];
    p += size;
    if (p >= _chapterData + _chapterBytes) return false;
  }

  const auto* desc = reinterpret_cast<const MinigameDescriptor*>(p);
  const uint8_t* codePtr = p + sizeof(MinigameDescriptor);

  const uint8_t* setupCode = desc->setupLen > 0 ? codePtr : nullptr;
  uint16_t setupLen = desc->setupLen;
  codePtr += setupLen;

  const uint8_t* keyCodes[6] = {};
  uint16_t keyLens[6] = {};
  for (int k = 0; k < 6; ++k) {
    keyLens[k] = desc->keyLens[k];
    keyCodes[k] = keyLens[k] > 0 ? codePtr : nullptr;
    codePtr += keyLens[k];
  }

  // Populate minigame string pointers from string pool
  _minigameStringCount = desc->stringCount <= 16 ? desc->stringCount : 16;
  for (int s = 0; s < _minigameStringCount; ++s) {
    _minigameStrings[s] = stringAt(desc->stringOffsets[s]);
  }

  _vm.load(setupCode, setupLen, keyCodes, keyLens, _minigameStrings, _minigameStringCount);
  return true;
}

bool TrailEngine::startMinigame(int minigameIndex, uint32_t rngSeed) {
  if (!parseMinigame(minigameIndex)) return false;
  if (rngSeed != 0) _vm.setRngSeed(rngSeed);
  _vm.start();
  _state = EngineState::Minigame;
  return true;
}

void TrailEngine::updateMinigameKey(Key key) {
  if (_state != EngineState::Minigame) return;
  _vm.handleKey(key);
  if (_vm.isOver()) {
    finishMinigame();
  }
}

void TrailEngine::finishMinigame() {
  if (_state != EngineState::Minigame) return;
  bool won = (_vm.state() == VMState::Won);

  if (_currentNode.type == NodeType::Minigame) {
    advanceNode(won ? _currentNode.winNode : _currentNode.loseNode);
  } else {
    _state = EngineState::Narrative;
  }
}

}  // namespace trail

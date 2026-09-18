// Host test for trail::TrailEngine and trail::TrailSaveManager.
//
// Tests story binary parsing, new game setup, node navigation,
// choice availability under conditions, effect applications,
// conditional branching, minigame integration, and save/restore.

#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

#include "games/trail/TrailEngine.cpp"
#include "games/trail/TrailVM.cpp"
#include "games/trail/TrailSave.cpp"

using namespace trail;

static int gChecks = 0;
#define CHECK(cond)                                            \
  do {                                                         \
    ++gChecks;                                                 \
    const bool _ok = (cond);                                   \
    if (!_ok) {                                                \
      printf("FAILED check at %s:%d\n", __FILE__, __LINE__);   \
      assert(false);                                           \
    }                                                          \
  } while (0)

namespace {

// Helper: build a synthetic .story binary buffer
std::vector<uint8_t> buildSyntheticStory() {
  std::vector<uint8_t> buf;

  // 1. File Header (64 bytes)
  StoryFileHeader hdr = {};
  hdr.magic[0] = 'L'; hdr.magic[1] = 'T'; hdr.magic[2] = 'R'; hdr.magic[3] = 'L';
  hdr.version = kFormatVersion;
  hdr.chapterCount = 1;
  strncpy(hdr.id, "test_story", sizeof(hdr.id));
  strncpy(hdr.title, "Storia di Prova", sizeof(hdr.title));

  const uint8_t* pHdr = reinterpret_cast<const uint8_t*>(&hdr);
  buf.insert(buf.end(), pHdr, pHdr + sizeof(hdr));

  // 2. Meta Block
  StoryMetaBlock meta = {};
  meta.resourceCount = 2;
  meta.companionCount = 2;

  strncpy(meta.resources[0].name, "Provviste", kResNameLen);
  meta.resources[0].icon = ResIcon::Bar;
  meta.resources[0].start = 80;
  meta.resources[0].max = 100;
  meta.resources[0].critical = 15;

  strncpy(meta.resources[1].name, "Salute", kResNameLen);
  meta.resources[1].icon = ResIcon::Bar;
  meta.resources[1].start = 100;
  meta.resources[1].max = 100;
  meta.resources[1].critical = 20;

  strncpy(meta.companions[0].name, "Marco", kCompNameLen);
  strncpy(meta.companions[0].trait, "Guida", kCompTraitLen);
  strncpy(meta.companions[1].name, "Fatima", kCompNameLen);
  strncpy(meta.companions[1].trait, "Mercante", kCompTraitLen);

  const uint8_t* pMeta = reinterpret_cast<const uint8_t*>(&meta);
  buf.insert(buf.end(), pMeta, pMeta + sizeof(meta));

  // 3. Chapter Offset Table (1 chapter)
  uint32_t chapterTableOffset = buf.size();
  uint32_t chapter0Offset = chapterTableOffset + sizeof(uint32_t); // immediately following
  const uint8_t* pOffset = reinterpret_cast<const uint8_t*>(&chapter0Offset);
  buf.insert(buf.end(), pOffset, pOffset + sizeof(uint32_t));

  // 4. Chapter 0 Block
  // Prepare strings pool
  std::vector<char> strPool;
  auto addStr = [&](const char* s) -> uint16_t {
    uint16_t off = strPool.size();
    while (*s) strPool.push_back(*s++);
    strPool.push_back('\0');
    return off;
  };

  uint16_t sTitle = addStr("Venezia");
  uint16_t sText0 = addStr("La carovana e pronta per la partenza. Cosa fai?");
  uint16_t sCh0_0 = addStr("Parti subito");
  uint16_t sCh0_1 = addStr("Compra provviste");
  uint16_t sText1 = addStr("Controllo flag...");
  uint16_t sText2 = addStr("Caccia nei boschi!");
  uint16_t sText3 = addStr("Capitolo completato.");
  uint16_t sMgHit = addStr("Colpito!");

  // Prepare Minigame 0
  MinigameDescriptor mgDesc = {};
  mgDesc.gridW = 4;
  mgDesc.gridH = 4;
  mgDesc.stringCount = 1;
  mgDesc.stringOffsets[0] = sMgHit;

  // Minigame setup bytecode: GINIT 4,4; GSET (1,1)=2; WIN(1) on confirm
  uint8_t mgSetup[] = { op::GINIT, 4, 4, op::PUSH, 2, 0, op::PUSH, 1, 0, op::PUSH, 1, 0, op::GSET, op::HALT };
  uint8_t mgConfirm[] = { op::PUSH, 1, 0, op::WIN, op::HALT };
  mgDesc.setupLen = sizeof(mgSetup);
  mgDesc.keyLens[static_cast<int>(Key::Confirm)] = sizeof(mgConfirm);

  std::vector<uint8_t> mgBytes;
  const uint8_t* pMgDesc = reinterpret_cast<const uint8_t*>(&mgDesc);
  mgBytes.insert(mgBytes.end(), pMgDesc, pMgDesc + sizeof(mgDesc));
  mgBytes.insert(mgBytes.end(), mgSetup, mgSetup + sizeof(mgSetup));
  mgBytes.insert(mgBytes.end(), mgConfirm, mgConfirm + sizeof(mgConfirm));

  // Prepare Nodes (4 nodes)
  std::vector<Node> nodes(4);

  // Node 0: Narrative
  nodes[0].type = NodeType::Narrative;
  nodes[0].titleOff = sTitle;
  nodes[0].textOff = sText0;
  nodes[0].choiceCount = 2;
  // Choice 0: "Parti subito" -> effects: provviste -10, flag 0 set -> next: 1
  nodes[0].choices[0].labelOff = sCh0_0;
  nodes[0].choices[0].nextNode = 1;
  nodes[0].choices[0].effectCount = 2;
  nodes[0].choices[0].effects[0] = { EffectType::ResourceDelta, 0, -10 };
  nodes[0].choices[0].effects[1] = { EffectType::FlagSet, 0, 1 };
  // Choice 1: "Compra provviste" -> condition: provviste <= 90 -> effects: provviste +15 -> next: 2
  nodes[0].choices[1].labelOff = sCh0_1;
  nodes[0].choices[1].nextNode = 2;
  nodes[0].choices[1].conditionCount = 1;
  nodes[0].choices[1].conditions[0] = { CondType::ResourceMax, 0, 90 };
  nodes[0].choices[1].effectCount = 1;
  nodes[0].choices[1].effects[0] = { EffectType::ResourceDelta, 0, 15 };

  // Node 1: Check (check flag 0 -> pass: 2, fail: 3)
  nodes[1].type = NodeType::Check;
  nodes[1].titleOff = sTitle;
  nodes[1].textOff = sText1;
  nodes[1].conditionCount = 1;
  nodes[1].conditions[0] = { CondType::FlagSet, 0, 1 };
  nodes[1].passNode = 2;
  nodes[1].failNode = 3;

  // Node 2: Minigame (minigame 0 -> win: 3, lose: 3)
  nodes[2].type = NodeType::Minigame;
  nodes[2].titleOff = sTitle;
  nodes[2].textOff = sText2;
  nodes[2].minigameId = 0;
  nodes[2].winNode = 3;
  nodes[2].loseNode = 3;

  // Node 3: ChapterEnd
  nodes[3].type = NodeType::ChapterEnd;
  nodes[3].titleOff = sTitle;
  nodes[3].textOff = sText3;

  // Assemble ChapterHeader
  ChapterHeader chHdr = {};
  chHdr.nodeCount = nodes.size();
  chHdr.minigameCount = 1;
  chHdr.stringCount = 8;
  chHdr.nodesOffset = sizeof(ChapterHeader);
  chHdr.minigamesOffset = chHdr.nodesOffset + nodes.size() * sizeof(Node);
  chHdr.stringsOffset = chHdr.minigamesOffset + mgBytes.size();
  chHdr.imagesOffset = chHdr.stringsOffset + strPool.size();
  chHdr.totalBytes = chHdr.imagesOffset;

  const uint8_t* pChHdr = reinterpret_cast<const uint8_t*>(&chHdr);
  buf.insert(buf.end(), pChHdr, pChHdr + sizeof(chHdr));

  const uint8_t* pNodes = reinterpret_cast<const uint8_t*>(nodes.data());
  buf.insert(buf.end(), pNodes, pNodes + nodes.size() * sizeof(Node));
  buf.insert(buf.end(), mgBytes.begin(), mgBytes.end());
  buf.insert(buf.end(), reinterpret_cast<const uint8_t*>(strPool.data()),
             reinterpret_cast<const uint8_t*>(strPool.data()) + strPool.size());

  return buf;
}

void testEngineLifecycle() {
  auto storyBytes = buildSyntheticStory();
  TrailEngine engine;

  CHECK(engine.loadStory(storyBytes.data(), storyBytes.size()));
  CHECK(engine.header() != nullptr);
  CHECK(strcmp(engine.header()->id, "test_story") == 0);
  CHECK(strcmp(engine.header()->title, "Storia di Prova") == 0);

  CHECK(engine.startNewGame());
  CHECK(engine.state() == EngineState::Narrative);
  CHECK(engine.currentChapter() == 0);
  CHECK(engine.currentNodeId() == 0);
  CHECK(engine.day() == 1);
  CHECK(engine.resource(0) == 80); // Provviste
  CHECK(engine.resource(1) == 100); // Salute
  CHECK(engine.companionPresent(0) == true);
  CHECK(engine.companionPresent(1) == true);

  // Check strings
  CHECK(strcmp(engine.nodeTitle(), "Venezia") == 0);
  CHECK(strcmp(engine.choiceLabel(0), "Parti subito") == 0);
  CHECK(strcmp(engine.choiceLabel(1), "Compra provviste") == 0);

  // Both choices available initially (Provviste=80 <= 90)
  CHECK(engine.isChoiceAvailable(0) == true);
  CHECK(engine.isChoiceAvailable(1) == true);

  // Make Choice 0: "Parti subito" -> Provviste becomes 80 - 10 = 70, flag 0 set
  CHECK(engine.makeChoice(0));
  CHECK(engine.resource(0) == 70);
  CHECK(engine.flag(0) == true);
  CHECK(engine.currentNodeId() == 1); // Check node

  // Advance Check node auto: flag 0 is set -> goes to Node 2 (Minigame)
  CHECK(engine.advanceAuto());
  CHECK(engine.state() == EngineState::Minigame);
  CHECK(engine.currentNodeId() == 2);

  // Play minigame: Confirm key -> WIN! -> advances to Node 3 (ChapterEnd)
  engine.updateMinigameKey(Key::Confirm);
  CHECK(engine.state() == EngineState::ChapterEnded);
  CHECK(engine.currentNodeId() == 3);

  // Advance ChapterEnd node auto -> StoryWon (only 1 chapter)
  CHECK(engine.advanceAuto());
  CHECK(engine.state() == EngineState::StoryWon);
}

void testSaveAndRestore() {
  auto storyBytes = buildSyntheticStory();
  TrailEngine engine;

  CHECK(engine.loadStory(storyBytes.data(), storyBytes.size()));
  CHECK(engine.startNewGame());

  // Make Choice 0: Provviste=70, flag 0 = true
  CHECK(engine.makeChoice(0));
  CHECK(engine.resource(0) == 70);
  CHECK(engine.flag(0) == true);

  // Create save snapshot
  TrailSave save;
  engine.createSave(save);

  // Verify save fields
  CHECK(save.chapter == 0);
  CHECK(save.nodeId == 1);
  CHECK(save.resources[0] == 70);
  CHECK(save.flagGet(0) == true);

  // Restore in fresh engine
  TrailEngine engine2;
  CHECK(engine2.loadStory(storyBytes.data(), storyBytes.size()));
  CHECK(engine2.restoreGame(save));

  CHECK(engine2.currentChapter() == 0);
  CHECK(engine2.currentNodeId() == 1);
  CHECK(engine2.resource(0) == 70);
  CHECK(engine2.flag(0) == true);
}

void testSaveManagerPreferences() {
  TrailSave save = {};
  save.storyHash = storyHash("test_story");
  save.chapter = 2;
  save.nodeId = 5;
  save.day = 12;
  save.resources[0] = 45;
  save.flagSet(3, true);

  CHECK(TRAIL_SAVES.save(save));

  TrailSave loaded = {};
  CHECK(TRAIL_SAVES.load(save.storyHash, loaded));
  CHECK(loaded.chapter == 2);
  CHECK(loaded.nodeId == 5);
  CHECK(loaded.day == 12);
  CHECK(loaded.resources[0] == 45);
  CHECK(loaded.flagGet(3) == true);

  CHECK(TRAIL_SAVES.clear(save.storyHash));
  TrailSave afterClear = {};
  CHECK(TRAIL_SAVES.load(save.storyHash, afterClear) == false);
}

}  // namespace

int main() {
  testEngineLifecycle();
  testSaveAndRestore();
  testSaveManagerPreferences();

  printf("trail_engine_test: %d checks passed\n", gChecks);
  return 0;
}

#include "addon.h"
#include "inline_candidates.h"

#include <fcitx-utils/event.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputmethodmanager.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/addonmanager.h>
#include <fcitx/addonfactory.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, const char *message) {
    if (!value) { throw std::runtime_error(message); }
}

// The desktop keyboard engine is statically linked into the fcitx5 binary.
// Embedded tests need only its inactive fallback entry; Rime is loaded from
// the installed shared addon and handles every composition tested below.
class KeyboardStub : public fcitx::InputMethodEngine {
public:
    std::vector<fcitx::InputMethodEntry> listInputMethods() override {
        std::vector<fcitx::InputMethodEntry> entries;
        entries.emplace_back("keyboard-us", "Test keyboard", "en", "keyboard");
        return entries;
    }
    void keyEvent(const fcitx::InputMethodEntry &, fcitx::KeyEvent &) override {}
};
class KeyboardFactory : public fcitx::AddonFactory {
public:
    fcitx::AddonInstance *create(fcitx::AddonManager *) override {
        return new KeyboardStub;
    }
};

class TestContext : public fcitx::InputContext {
public:
    explicit TestContext(fcitx::Instance &instance)
        : InputContext(instance.inputContextManager(), "english-hint-test") {
        setFocusGroup(instance.defaultFocusGroup());
        created();
    }
    ~TestContext() override { destroy(); }
    const char *frontend() const override { return "test"; }
    std::string committed;
private:
    void commitStringImpl(const std::string &text) override { committed += text; }
    void deleteSurroundingTextImpl(int, unsigned int) override {}
    void forwardKeyImpl(const fcitx::ForwardKeyEvent &) override {}
    void updatePreeditImpl() override {}
};

void key(TestContext &ic, fcitx::Instance &instance, const std::string &name) {
    fcitx::KeyEvent event(&ic, fcitx::Key(name), false);
    ic.keyEvent(event);
    instance.flushUI();
}
void type(TestContext &ic, fcitx::Instance &instance, const std::string &text) {
    for (char ch : text) { key(ic, instance, std::string(1, ch)); }
}
void write(const std::filesystem::path &path, const std::string &content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path);
    file << content;
    require(bool(file), "test fixture must be writable");
}
} // namespace

int main() {
    using namespace fcitx;
    using namespace fcitx::english_hint;
    char pattern[] = "/tmp/english-hint-rime-XXXXXX";
    const char *directory = mkdtemp(pattern);
    if (!directory) { return 1; }
    const std::filesystem::path root(directory);
    int result = 1;
    try {
        // Isolate all writable data; never load the user's Rime schema or key.
        for (const auto *kind : {"CONFIG", "DATA", "CACHE"}) {
            const auto path = (root / kind).string();
            setenv((std::string("XDG_") + kind + "_HOME").c_str(), path.c_str(), 1);
        }
        const auto configDir = root / "CONFIG" / "fcitx5";
        const auto dataDir = root / "DATA" / "fcitx5";
        setenv("FCITX_CONFIG_HOME", configDir.c_str(), 1);
        setenv("FCITX_DATA_HOME", dataDir.c_str(), 1);
        write(configDir / "conf/english-hint.conf",
              "[General]\nDebounceMs=2000\n[LLM]\nEndpoint=http://127.0.0.1:1/v1/chat/completions\n");
        write(dataDir / "rime/default.custom.yaml",
              "patch:\n  schema_list:\n    - schema: english_hint_test\n");
        write(dataDir / "rime/english_hint_test.schema.yaml", R"(schema:
  schema_id: english_hint_test
  name: English Hint Test
  version: "1"
switches:
  - name: ascii_mode
    reset: 0
    states: [Chinese, English]
engine:
  processors: [ascii_composer, key_binder, speller, selector, navigator, express_editor]
  segmentors: [ascii_segmentor, abc_segmentor, fallback_segmentor]
  translators: [table_translator]
speller:
  alphabet: abcdefghijklmnopqrstuvwxyz
translator:
  dictionary: english_hint_test
  enable_sentence: false
  enable_user_dict: false
menu:
  page_size: 5
key_binder:
  bindings:
    - {when: paging, accept: Page_Up, send: Page_Up}
    - {when: has_menu, accept: Page_Down, send: Page_Down}
)");
        const std::vector<std::string> words{
            "是", "时", "事", "十", "使", "世", "市", "式", "识", "师", "诗", "石"};
        std::string dictionary = "---\nname: english_hint_test\nversion: '1'\nsort: by_weight\n...\n";
        CandidateTranslations cacheEntries;
        for (std::size_t i = 0; i < words.size(); ++i) {
            dictionary += words[i] + "\tshi\t" + std::to_string(1000 - i) + "\n";
            cacheEntries.emplace_back(words[i], "English " + std::to_string(i));
        }
        write(dataDir / "rime/english_hint_test.dict.yaml", dictionary);
        PersistentCache cache(persistentCachePath());
        require(cache.append(cacheEntries), "fixture translations must be cached");

        char name[] = "english-hint-integration";
        char disable[] = "--disable=all";
        char enable[] = "--enable=keyboard,rime";
        char keep[] = "--keep";
        char *args[] = {name, disable, enable, keep};
        KeyboardFactory keyboard;
        StaticAddonRegistry registry{{"keyboard", &keyboard}};
        Instance instance(4, args);
        instance.addonManager().registerDefaultLoader(&registry);
        instance.initialize();
        require(instance.inputMethodManager().entry("rime"), "fcitx5-rime must be installed");
        InputMethodGroup group("Test");
        group.setDefaultLayout("us");
        group.inputMethodList().emplace_back("keyboard-us");
        group.inputMethodList().emplace_back("rime");
        group.setDefaultInputMethod("rime");
        instance.inputMethodManager().addEmptyGroup("Test");
        instance.inputMethodManager().setGroup(std::move(group));
        instance.inputMethodManager().setCurrentGroup("Test");
        TestContext ic(instance);
        ic.focusIn();
        instance.setCurrentInputMethod(&ic, "rime", true);
        instance.activate();
        require(instance.inputMethod(&ic) == "rime", "test context must activate real Rime");
        EnglishHint hint(&instance);
        std::unique_ptr<EventSourceTime> timer;
        int attempts = 0;
        timer = instance.eventLoop().addTimeEvent(CLOCK_MONOTONIC, now(CLOCK_MONOTONIC), 0,
            [&](EventSourceTime *source, uint64_t) {
                try {
                    ic.reset();
                    type(ic, instance, "shi");
                    auto list = originalCandidates(ic.inputPanel());
                    if (!list || list->empty()) {
                        require(++attempts < 150, "Rime fixture deployment timed out");
                        source->setTime(now(CLOCK_MONOTONIC) + 200000);
                        return true;
                    }
                    require(list->candidate(0).text().toString() == words[0],
                            "isolated deterministic Rime schema must be active");
                    for (bool vertical : {false, true}) {
                        EnglishHintFcitxConfig settings;
                        settings.general.mutableValue()->verticalBilingual.setValue(vertical);
                        settings.llm.mutableValue()->endpoint.setValue(
                            "http://127.0.0.1:1/v1/chat/completions");
                        RawConfig raw;
                        settings.save(raw);
                        hint.setConfig(raw);
                        for (const auto *selection : {"2", "mouse", "Alt+2"}) {
                            ic.reset();
                            ic.committed.clear();
                            type(ic, instance, "shi");
                            const auto panelList = ic.inputPanel().candidateList();
                            if (vertical) {
                                require(panelList->layoutHint() == CandidateLayoutHint::Vertical &&
                                            panelList->candidate(1).text().toString() == words[1] + "  English 1",
                                        "real Rime candidates must show inline translations");
                            } else {
                                require(panelList->candidate(1).text().toString() == words[1] &&
                                            ic.inputPanel().auxDown().toString().find("English 1") != std::string::npos,
                                        "default mode must retain original candidates and aux line");
                            }
                            if (std::string(selection) == "mouse") {
                                panelList->candidate(1).select(&ic);
                                instance.flushUI();
                            } else {
                                key(ic, instance, selection);
                            }
                            const std::string expected = std::string(selection) == "Alt+2"
                                                             ? "English 1" : words[1];
                            require(ic.committed == expected &&
                                        (!ic.inputPanel().candidateList() || ic.inputPanel().candidateList()->empty()),
                                    "real selection must commit the intended language and clear composition");
                        }
                        for (bool viaUi : {false, true}) {
                            ic.reset();
                            ic.committed.clear();
                            type(ic, instance, "shi");
                            if (viaUi) {
                                ic.inputPanel().candidateList()->toPageable()->next();
                                instance.flushUI();
                            } else {
                                key(ic, instance, "Next");
                            }
                            require(originalCandidates(ic.inputPanel())->candidate(0).text().toString() == words[5],
                                    "both UI and key paging must reach the second Rime page");
                            if (viaUi) {
                                ic.inputPanel().candidateList()->toPageable()->prev();
                                instance.flushUI();
                                require(originalCandidates(ic.inputPanel())->candidate(0).text().toString() == words[0],
                                        "UI previous-page control must return to the first page");
                                ic.inputPanel().candidateList()->toPageable()->next();
                                instance.flushUI();
                            } else {
                                key(ic, instance, "Prior");
                                require(originalCandidates(ic.inputPanel())->candidate(0).text().toString() == words[0],
                                        "Page Up must return to the first Rime page");
                                key(ic, instance, "Next");
                            }
                            key(ic, instance, "Alt+1");
                            require(ic.committed == "English 5",
                                    "English selection after paging must use the new page");
                        }
                    }
                    // Reload during composition must switch in both directions.
                    type(ic, instance, "shi");
                    RawConfig horizontal;
                    horizontal["General"]["VerticalBilingual"] = "False";
                    hint.setConfig(horizontal);
                    require(ic.inputPanel().candidateList()->candidate(0).text().toString() == words[0],
                            "hot mode switch must remove inline display");
                    RawConfig vertical;
                    vertical["General"]["VerticalBilingual"] = "True";
                    hint.setConfig(vertical);
                    require(ic.inputPanel().candidateList()->candidate(0).text().toString() == words[0] + "  English 0",
                            "hot mode switch must restore inline display");
                    ic.inputPanel().setAuxDown(Text("Rime-owned content"));
                    ic.updateUserInterface(UserInterfaceComponent::InputPanel, true);
                    require(ic.inputPanel().auxDown().toString() == "Rime-owned content" &&
                                ic.inputPanel().candidateList()->candidate(0).text().toString() == words[0],
                            "foreign auxiliary content must be preserved and inline display released");
                    ic.reset();
                    type(ic, instance, "shi");
                    RawConfig disabled;
                    disabled["General"]["Enabled"] = "False";
                    hint.setConfig(disabled);
                    require(ic.inputPanel().candidateList()->candidate(0).text().toString() == words[0] &&
                                ic.inputPanel().auxDown().empty(),
                            "disabling addon must remove its displays");
                    vertical["General"]["Enabled"] = "True";
                    hint.setConfig(vertical);
                    ic.setCapabilityFlags(CapabilityFlag::Password);
                    ic.updateUserInterface(UserInterfaceComponent::InputPanel, true);
                    const auto sensitiveList = ic.inputPanel().candidateList();
                    require(!sensitiveList || sensitiveList->empty() ||
                                sensitiveList->candidate(0).text().toString().find("English") == std::string::npos,
                            "sensitive context must remove English display");
                    ic.committed.clear();
                    key(ic, instance, "Alt+1");
                    require(ic.committed.find("English") == std::string::npos,
                            "sensitive context must never commit an English hint");
                    ic.setCapabilityFlags(CapabilityFlags{});
                    ic.reset();
                    ic.focusOut();
                    std::cout << "Real Rime: both displays, numeric/mouse/Alt selection, key/UI paging, reload and privacy guards passed\n";
                    result = 0;
                    instance.exit();
                    return false;
                } catch (const std::exception &error) {
                    std::cerr << error.what() << '\n';
                    instance.exit();
                    return false;
                }
            });
        timer->setEnabled(true);
        instance.setRunning(true);
        instance.eventLoop().exec();
        instance.setRunning(false);
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
    }
    std::filesystem::remove_all(root);
    return result;
}

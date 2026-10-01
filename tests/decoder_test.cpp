#include "../src/decoder.h"
#include "../src/textpositions.h"
#include "../src/moduleprofile.h"
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <spellingtrie.h>
#include <cctype>

static void check(bool condition, const char *name) {
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", name); std::exit(1); }
}
static bool has(bbime::Decoder &decoder, const char *text) {
    for (size_t i = 0; i < decoder.candidates().size(); ++i)
        if (decoder.candidates()[i].text == text) return true;
    return false;
}
static std::string fileBytes(const char *path) {
    std::FILE *file = std::fopen(path, "rb");
    check(file != 0, "synthetic user dictionary readable");
    std::string out;
    char buffer[4096];
    size_t count;
    while ((count = std::fread(buffer, 1, sizeof(buffer), file)) != 0)
        out.append(buffer, count);
    check(!std::ferror(file), "synthetic dictionary read complete");
    std::fclose(file);
    return out;
}
static void profileTest() {
    bbime::ModuleProfile defaults, parsed;
    const bbime::ModuleProfile::Values valid = defaults.values();
    check(bbime::ModuleProfile::parse(valid, parsed), "profile defaults accepted");
    check(parsed.values() == valid, "profile complete round trip");
    bbime::ModuleProfile::Values values = valid;
    values["input/startMode"] = "english";
    values["input/chineseMode"] = "full";
    values["input/shiftCursor"] = "false";
    check(bbime::ModuleProfile::parse(values, parsed) && parsed.startMode == "english" &&
          parsed.chineseMode == "full" && !parsed.shiftCursor, "profile preferences applied");
    const bbime::ModuleProfile::Values before = parsed.values();
    const char *badKeys[] = {"profile/version", "profile/engine", "input/startMode",
        "input/chineseMode", "input/shiftCandidates", "input/shiftCursor",
        "input/chinesePunctuation"};
    for (size_t i = 0; i < sizeof(badKeys) / sizeof(badKeys[0]); ++i) {
        values = valid;
        values[badKeys[i]] = "invalid";
        check(!bbime::ModuleProfile::parse(values, parsed), "invalid profile value rejected");
        check(parsed.values() == before, "profile failure keeps previous values");
        values = valid;
        values.erase(badKeys[i]);
        check(!bbime::ModuleProfile::parse(values, parsed), "missing field rejected");
    }
    const char *forbidden[] = {"local/learnSelections", "dictionary/path",
        "input/imeEnabled", "input/composition", "unknown/key"};
    for (size_t i = 0; i < sizeof(forbidden) / sizeof(forbidden[0]); ++i) {
        values = valid;
        values[forbidden[i]] = "true";
        check(!bbime::ModuleProfile::parse(values, parsed), "shared field whitelist");
    }
    values = valid;
    values["input/startMode"] = "full";
    check(!bbime::ModuleProfile::parse(values, parsed), "inconsistent Chinese modes rejected");
    const char *badBools[] = {"TRUE", "1", "", " true", "@Variant(foo)"};
    for (size_t i = 0; i < sizeof(badBools) / sizeof(badBools[0]); ++i) {
        values = valid;
        values["input/shiftCursor"] = badBools[i];
        check(!bbime::ModuleProfile::parse(values, parsed), "strict boolean value");
    }
}
int main(int argc, char **argv) {
    check(argc == 3 || argc == 5, "dictionary arguments");
    if (argc == 5) {
        bbime::Decoder probe;
        check(probe.open(argv[1], argv[2]), "probe dictionary load");
        const bool accepted = probe.setCode(argv[3], std::string(argv[4]) == "natural");
        std::printf("PROBE accepted=%d candidates=%lu\n", accepted ? 1 : 0,
                    static_cast<unsigned long>(probe.candidates().size()));
        return 0;
    }
    profileTest();
    const unsigned short emoji[] = {'A', 0xd83d, 0xde00, 0};
    check(bbime::Decoder::utf8(emoji) == "A\xf0\x9f\x98\x80", "decoder UTF8 surrogate pair");
    const unsigned short malformed[] = {0xd800, 'A', 0xdc00, 0};
    check(bbime::Decoder::utf8(malformed) == "\xef\xbf\xbd" "A\xef\xbf\xbd",
          "decoder UTF8 invalid surrogate replacement");
    const unsigned short text[] = {'A', 0x4e2d, 0xd83d, 0xde00, 'B'};
    const int offsets[] = {0, 1, 2, 4, 5};
    for (int i = 0; i < 5; ++i) {
        check(bbime::utf16Offset(text, 5, i, true) == offsets[i], "code point to UTF16");
        check(bbime::editorOffset(text, 5, offsets[i], true) == i, "UTF16 to code point");
        check(bbime::utf16Offset(text, 5, offsets[i], false) == offsets[i], "UTF16 cursor unit");
        check(bbime::editorOffset(text, 5, offsets[i], false) == offsets[i], "UTF16 selection unit");
    }
    check(bbime::utf16Offset(text, 5, -1, true) == 0, "negative cursor clamp");
    check(bbime::utf16Offset(text, 5, 100, true) == 5, "cursor end clamp");
    check(bbime::editorOffset(text, 5, 100, true) == 4, "selection end clamp");
    check(bbime::utf16Offset(text, 0, 1, true) == 0, "empty text cursor");
    const unsigned short unpaired[] = {0xd800, 'A', 0xdc00};
    check(bbime::utf16Offset(unpaired, 3, 2, true) == 2, "unpaired surrogate cursor");
    check(bbime::editorOffset(unpaired, 3, 3, true) == 3, "unpaired surrogate selection");
    const unsigned short pairs[] = {0xd83d, 0xde00, 0xd83d, 0xde01};
    check(bbime::utf16Offset(pairs, 4, 2, true) == 4, "adjacent surrogate pairs");
    check(bbime::editorOffset(pairs, 4, 4, true) == 2, "adjacent pair editor offsets");
    check(bbime::utf16Offset(text, 5, 3, true) - bbime::utf16Offset(text, 5, 2, true) == 2,
          "emoji selected replacement length");
    bbime::Decoder decoder;
    check(!decoder.setCode("ni", true) && decoder.choose(0).text.empty(), "unopened engine safe");
    check(!decoder.open(argv[1], argv[1]), "dictionary path alias rejected");
    check(!decoder.open(std::string(argv[1]) + ".missing", argv[2]), "failed open cleaned up");
    check(decoder.open(argv[1], argv[2]), "dictionary load");
    check(decoder.open(argv[1], argv[2]), "same engine open idempotent");
    check(!decoder.open(argv[1], std::string(argv[2]) + ".other"), "different path open rejected");
    {
        bbime::Decoder intruder;
        check(!intruder.open(argv[1], argv[2]), "second engine owner rejected");
    }
    check(decoder.setCode("nihk", true) && has(decoder, "\xe4\xbd\xa0\xe5\xa5\xbd"),
          "failed second owner leaves original usable");
    decoder.flush();
    const std::string beforeNoLearn = fileBytes(argv[2]);
    check(!decoder.choose(0, false).text.empty(), "selection without learning");
    decoder.flush();
    check(fileBytes(argv[2]) == beforeNoLearn, "no-learning selection preserves dictionary bytes");
    check(decoder.mappedSyllables() > 380, "complete syllable map");
    ime_pinyin::SpellingTrie &trie = ime_pinyin::SpellingTrie::get_instance();
    size_t covered = 0;
    for (size_t i = 0; i < decoder.mappedSyllables(); ++i) {
        std::string spelling(trie.get_spelling_str(
            static_cast<ime_pinyin::uint16>(ime_pinyin::kFullSplIdStart + i)));
        for (size_t j = 0; j < spelling.size(); ++j)
            spelling[j] = static_cast<char>(std::tolower(spelling[j]));
        const std::string code = bbime::Decoder::naturalCode(spelling);
        if (code.size() != 2) {
            std::fprintf(stderr, "Unmapped dictionary spelling: %s\n", spelling.c_str());
            continue;
        }
        check(decoder.setCode(code, true), "all mapped syllables accepted");
        bool found = decoder.pinyin() == spelling;
        for (size_t c = 0; c < decoder.candidates().size(); ++c)
            if (decoder.candidates()[c].pinyin == spelling) found = true;
        check(found, spelling.c_str());
        ++covered;
    }
    check(covered >= 400, "dictionary syllable coverage");
    static const char *spelling[] = {
        "ni","hao","zhong","guo","shi","chi","zhi","shuang","ying","jue",
        "xiong","liu","gui","nve","a","o","e","ai","ang","eng","ou","er"
    };
    static const char *codes[] = {
        "ni","hk","vs","go","ui","ii","vi","ud","yy","jt",
        "xs","lq","gv","nt","aa","oo","ee","ai","ah","eg","ou","er"
    };
    for (size_t i = 0; i < sizeof(codes) / sizeof(codes[0]); ++i)
        check(bbime::Decoder::naturalCode(spelling[i]) == codes[i], spelling[i]);
    check(decoder.setCode("nihk", true), "natural ni hao accepted");
    check(has(decoder, "\xe4\xbd\xa0\xe5\xa5\xbd"), "natural ni hao");
    check(decoder.pinyin() == "ni'hao", "natural segmentation");
    bbime::Candidate first = decoder.choose(0);
    check(first.consumed == 4, "sentence candidate consumes four codes");
    check(decoder.setCode("nihk", true), "prefix preparation");
    size_t prefix = decoder.candidates().size();
    for (size_t i = 0; i < decoder.candidates().size(); ++i)
        if (decoder.candidates()[i].consumed == 2) { prefix = i; break; }
    check(prefix < decoder.candidates().size(), "prefix candidate exists");
    bbime::Candidate chosenPrefix = decoder.choose(prefix);
    check(chosenPrefix.consumed == 2, "prefix consumes one syllable");
    check(decoder.setCode("hk", true) &&
          has(decoder, "\xe5\xa5\xbd"), "suffix survives prefix choice");
    check(decoder.setCode("vsgo", true) && has(decoder, "\xe4\xb8\xad\xe5\x9b\xbd"),
          "natural zhong guo");
    check(decoder.setCode("nihao", false) && has(decoder, "\xe4\xbd\xa0\xe5\xa5\xbd"),
          "full pinyin comparison");
    check(decoder.setCode("al", true) && decoder.pinyin() == "ai", "zero initial alias");
    check(decoder.setCode("jv", true) && decoder.pinyin() == "ju", "u umlaut alias");
    check(decoder.setCode("nih", true) && !decoder.candidates().empty(), "half syllable");
    check(decoder.setCode("ni", true) && !decoder.candidates().empty(), "backspace recompute");
    check(decoder.setCode("zz", true) && decoder.pinyin() == "zei", "valid zei code");
    check(decoder.setCode("ww", true) && decoder.candidates().empty(), "no stale candidates");
    check(!decoder.setCode(std::string(33, 'n'), true), "bounded code");
    check(!decoder.setCode(std::string(40, 'n'), false), "engine row bound");
    check(!decoder.setCode("ni!", true), "invalid characters rejected");
    const std::string validCode = decoder.code();
    const std::string validPinyin = decoder.pinyin();
    const size_t validCount = decoder.candidates().size();
    check(!decoder.setCode("NI!", true) && decoder.code() == validCode &&
          decoder.pinyin() == validPinyin && decoder.candidates().size() == validCount,
          "invalid input preserves decoder state");
    check(decoder.setCode("ni'hao", false) &&
          has(decoder, "\xe4\xbd\xa0\xe5\xa5\xbd"), "full pinyin delimiters");
    check(decoder.setCode("", true) && decoder.candidates().empty(), "cancel");
    const std::string poolBoundary = "nmecy'grgfxchsznrtnglcmmlz'jifycbiazx";
    for (size_t length = 0; length <= poolBoundary.size(); ++length) {
        check(decoder.setCode(poolBoundary.substr(0, length), false),
              "dictionary pool exhaustion returns safely");
        check(decoder.candidates().size() <= 40, "pool boundary candidates bounded");
        check(decoder.setCode("nihk", true) && has(decoder, "\xe4\xbd\xa0\xe5\xa5\xbd"),
              "normal input recovers after resource boundary");
    }
    for (size_t i = 0; i < poolBoundary.size(); ++i) {
        std::string nearby = poolBoundary;
        nearby[i] = 'a';
        check(decoder.setCode(nearby, false), "nearby resource boundary accepted safely");
    }
    const std::string markBoundary = "rdkqispdihbglzmccxtapfxprdcdx'hyjgyzxot";
    for (size_t length = 0; length <= markBoundary.size(); ++length) {
        check(decoder.setCode(markBoundary.substr(0, length), false),
              "milestone and parsing mark exhaustion returns safely");
        check(decoder.setCode("nihk", true) && has(decoder, "\xe4\xbd\xa0\xe5\xa5\xbd"),
              "normal input recovers after parsing mark boundary");
    }
    for (size_t i = 0; i < markBoundary.size(); ++i) {
        std::string nearby = markBoundary;
        nearby[i] = 'a';
        check(decoder.setCode(nearby, false), "nearby parsing mark boundary accepted safely");
    }
    std::srand(910);
    for (int iteration = 0; iteration < 10000; ++iteration) {
        const bool natural = iteration % 2 == 0;
        std::string code;
        const size_t length = std::rand() % (natural ? 33 : 40);
        for (size_t i = 0; i < length; ++i) {
            const int letter = std::rand() % (natural ? 26 : 27);
            code += letter == 26 ? '\'' : static_cast<char>('a' + letter);
        }
        if (std::getenv("BBIME_TRACE_FUZZ"))
            std::fprintf(stderr, "FUZZ iteration=%d mode=%s code=%s\n",
                iteration, natural ? "natural" : "full", code.c_str());
        if (!decoder.setCode(code, natural)) continue;
        check(decoder.candidates().size() <= 40, "randomized candidate limit");
        for (size_t i = 0; i < decoder.candidates().size(); ++i) {
            const bbime::Candidate &candidate = decoder.candidates()[i];
            check(!candidate.text.empty() && candidate.consumed > 0 &&
                  candidate.consumed <= code.size() && candidate.pinyin.size() <= 39,
                  "randomized candidate bounds");
        }
        if (!decoder.candidates().empty() && iteration % 100 == 0)
            check(!decoder.choose(0, false).text.empty(), "randomized no-learning choice");
    }
    double samples[100];
    for (int i = 0; i < 100; ++i) {
        std::clock_t start = std::clock();
        check(decoder.setCode("nihkvsgo", true) && !decoder.candidates().empty(), "stress");
        samples[i] = 1000.0 * (std::clock() - start) / CLOCKS_PER_SEC;
    }
    std::sort(samples, samples + 100);
    decoder.close();
    decoder.close();
    {
        bbime::Decoder successor;
        check(successor.open(argv[1], argv[2]) && successor.setCode("nihk", true) &&
              has(successor, "\xe4\xbd\xa0\xe5\xa5\xbd"), "owner release and reopen");
    }
    std::printf("PASS: module profile validation, engine ownership, no-learning, UTF8, "
                "text position units, natural code, dictionary, phrases, partial syllable, deletion, "
                "invalid input, pool exhaustion and 10000 randomized queries, full pinyin and bounds. "
                "CPU ms p50=%.3f p95=%.3f max=%.3f\n",
                samples[49], samples[94], samples[99]);
    return 0;
}

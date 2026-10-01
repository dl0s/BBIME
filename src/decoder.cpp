#include "decoder.h"
#include <pinyinime.h>
#include <spellingtrie.h>
#include <algorithm>
#include <cctype>
#include <cstring>

using namespace ime_pinyin;
namespace bbime {
static Decoder *engineOwner = 0;
Decoder::Decoder() : opened_(false), syllables_(0) {}
Decoder::~Decoder() { close(); }

void Decoder::close() {
    if (opened_ && engineOwner == this) {
        im_close_decoder();
        engineOwner = 0;
    }
    opened_ = false;
    syllables_ = 0;
    code_.clear();
    pinyin_.clear();
    systemPath_.clear();
    userPath_.clear();
    pairs_.clear();
    items_.clear();
}
void Decoder::flush() { if (opened_ && engineOwner == this) im_flush_cache(); }

std::string Decoder::utf8(const unsigned short *text) {
    std::string out;
    for (size_t i = 0; text[i]; ++i) {
        unsigned c = text[i];
        if (c >= 0xd800 && c <= 0xdbff) {
            const unsigned low = text[i + 1];
            if (low >= 0xdc00 && low <= 0xdfff) {
                c = 0x10000 + ((c - 0xd800) << 10) + low - 0xdc00;
                ++i;
            } else c = 0xfffd;
        } else if (c >= 0xdc00 && c <= 0xdfff) c = 0xfffd;
        if (c < 128) out += static_cast<char>(c);
        else if (c < 2048) {
            out += static_cast<char>(0xc0 | (c >> 6));
            out += static_cast<char>(0x80 | (c & 63));
        } else if (c < 0x10000) {
            out += static_cast<char>(0xe0 | (c >> 12));
            out += static_cast<char>(0x80 | ((c >> 6) & 63));
            out += static_cast<char>(0x80 | (c & 63));
        } else {
            out += static_cast<char>(0xf0 | (c >> 18));
            out += static_cast<char>(0x80 | ((c >> 12) & 63));
            out += static_cast<char>(0x80 | ((c >> 6) & 63));
            out += static_cast<char>(0x80 | (c & 63));
        }
    }
    return out;
}

std::string Decoder::naturalCode(const std::string &s) {
    if (s.empty()) return "";
    std::string initial, final;
    if (s[0] == 'a' || s[0] == 'e' || s[0] == 'o') {
        if (s.size() == 1) return s + s;
        if (s.size() == 2) return s;
        initial = s.substr(0, 1);
        final = s;
    } else if (s.substr(0, 2) == "zh" || s.substr(0, 2) == "ch" ||
               s.substr(0, 2) == "sh") {
        initial = s[0] == 'z' ? "v" : (s[0] == 'c' ? "i" : "u");
        final = s.substr(2);
    } else {
        initial = s.substr(0, 1);
        final = s.substr(1);
    }
    static const char *finals[] = {
        "iu","ia","ua","uan","van","ue","ve","ing","uai","uo","un","vn",
        "ong","iong","iang","uang","en","eng","ang","ian","an","iao","ao",
        "ai","ei","ie","ui","ou","in"
    };
    static const char keys[] = "qwwrrttyyoppssddfghmjcklzxvbn";
    for (size_t i = 0; i < sizeof(finals) / sizeof(finals[0]); ++i)
        if (final == finals[i]) return initial + keys[i];
    return final.size() == 1 ? initial + final : "";
}

void Decoder::buildMap() {
    pairs_.clear();
    SpellingTrie &trie = SpellingTrie::get_instance();
    syllables_ = trie.get_spelling_num();
    for (size_t i = 0; i < syllables_; ++i) {
        std::string spelling(trie.get_spelling_str(
            static_cast<uint16>(kFullSplIdStart + i)));
        for (size_t j = 0; j < spelling.size(); ++j)
            spelling[j] = static_cast<char>(std::tolower(spelling[j]));
        std::string pair = naturalCode(spelling);
        if (pair.size() != 2) continue;
        pairs_[pair].push_back(spelling);
        // Natural code also accepts a doubled zero initial before the final.
        if (spelling.size() == 2 && (spelling[0] == 'a' ||
            spelling[0] == 'e' || spelling[0] == 'o') && spelling[1] != 'r') {
            static const char *vowels = "ioun";
            if (std::strchr(vowels, spelling[1])) {
                std::string alias = naturalCode("b" + spelling);
                if (alias.size() == 2) {
                    alias[0] = spelling[0];
                    pairs_[alias].push_back(spelling);
                }
            }
        }
        if (spelling.size() == 2 && spelling[1] == 'u' &&
            std::strchr("jqxy", spelling[0])) {
            pairs_[spelling.substr(0, 1) + "v"].push_back(spelling);
        }
    }
}

bool Decoder::open(const std::string &system, const std::string &user) {
    if (opened_) return system == systemPath_ && user == userPath_;
    if (engineOwner || system.empty() || user.empty() || system == user) return false;
    opened_ = im_open_decoder(system.c_str(), user.c_str());
    if (opened_) {
        engineOwner = this;
        systemPath_ = system;
        userPath_ = user;
        im_set_max_lens(39, 16);
        buildMap();
    } else im_close_decoder();
    return opened_;
}

std::vector<std::string> Decoder::expand(const std::string &code) const {
    std::vector<std::string> paths(1, "");
    for (size_t pos = 0; pos + 1 < code.size(); pos += 2) {
        std::map<std::string, std::vector<std::string> >::const_iterator found =
            pairs_.find(code.substr(pos, 2));
        if (found == pairs_.end()) return std::vector<std::string>();
        std::vector<std::string> next;
        for (size_t p = 0; p < paths.size(); ++p)
            for (size_t s = 0; s < found->second.size() && next.size() < 8; ++s)
                next.push_back(paths[p] + (pos ? "'" : "") + found->second[s]);
        paths.swap(next);
    }
    if (code.size() % 2) {
        char ch = code[code.size() - 1];
        std::string initial(1, ch);
        if (ch == 'v') initial = "zh";
        if (ch == 'i') initial = "ch";
        if (ch == 'u') initial = "sh";
        for (size_t p = 0; p < paths.size(); ++p)
            paths[p] += (code.size() > 1 ? "'" : "") + initial;
    }
    return paths;
}

bool Decoder::setCode(const std::string &code, bool natural) {
    if (!opened_ || code.size() > (natural ? 32u : 39u)) return false;
    for (size_t i = 0; i < code.size(); ++i)
        if (!(code[i] >= 'a' && code[i] <= 'z') &&
            !(!natural && code[i] == '\'')) return false;
    std::vector<std::string> paths = natural ? expand(code) :
        std::vector<std::string>(1, code);
    bool fits = paths.empty();
    for (size_t p = 0; p < paths.size(); ++p)
        if (paths[p].size() <= 39) fits = true;
    if (!fits) return false;
    code_ = code;
    pinyin_.clear();
    items_.clear();
    im_reset_search();
    if (code.empty()) return true;
    std::vector<std::vector<Candidate> > groups;
    for (size_t p = 0; p < paths.size(); ++p) {
        if (paths[p].size() > 39) continue;
        im_reset_search();
        size_t count = im_search(paths[p].c_str(), paths[p].size());
        size_t decoded = 0;
        im_get_sps_str(&decoded);
        if (decoded != paths[p].size()) continue;
        if (pinyin_.empty()) pinyin_ = paths[p];
        const uint16 *starts = 0;
        size_t segments = im_get_spl_start_pos(starts);
        std::vector<Candidate> group;
        for (size_t c = 0; c < count && c < 40; ++c) {
            char16 text[64] = {0};
            if (!im_get_candidate(c, text, 64) || !text[0]) continue;
            size_t chars = 0;
            while (text[chars]) ++chars;
            Candidate item;
            item.text = utf8(text);
            item.pinyin = paths[p];
            item.engineId = c;
            item.consumed = natural ? std::min(code.size(), chars * 2) :
                (chars <= segments && starts ? starts[chars] : code.size());
            while (item.consumed < code.size() && code[item.consumed] == '\'')
                ++item.consumed;
            if (item.consumed) group.push_back(item);
        }
        groups.push_back(group);
    }
    for (size_t rank = 0; rank < 40; ++rank)
        for (size_t p = 0; p < groups.size(); ++p) {
            if (rank >= groups[p].size()) continue;
            bool duplicate = false;
            for (size_t i = 0; i < items_.size(); ++i)
                if (items_[i].text == groups[p][rank].text &&
                    items_[i].consumed == groups[p][rank].consumed) duplicate = true;
            if (!duplicate && items_.size() < 40) items_.push_back(groups[p][rank]);
        }
    if (pinyin_.empty() && !paths.empty()) pinyin_ = paths[0];
    return true;
}

Candidate Decoder::choose(size_t index, bool learn) {
    if (!opened_ || index >= items_.size()) return Candidate();
    Candidate result = items_[index];
    if (!learn) return result;
    im_reset_search();
    im_search(result.pinyin.c_str(), result.pinyin.size());
    im_choose(result.engineId);
    return result;
}
}

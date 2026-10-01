#ifndef BBIME_DECODER_H
#define BBIME_DECODER_H

#include <stddef.h>
#include <map>
#include <string>
#include <vector>

namespace bbime {
struct Candidate {
    Candidate() : engineId(0), consumed(0) {}
    std::string text;
    std::string pinyin;
    size_t engineId;
    size_t consumed;
};

class Decoder {
public:
    Decoder();
    ~Decoder();
    // libgooglepinyin is process-global: one owner, called on the host's input thread.
    bool open(const std::string &systemDictionary, const std::string &userDictionary);
    void close();
    void flush();
    bool setCode(const std::string &code, bool natural);
    Candidate choose(size_t index, bool learn = true);
    const std::vector<Candidate> &candidates() const { return items_; }
    const std::string &pinyin() const { return pinyin_; }
    const std::string &code() const { return code_; }
    size_t mappedSyllables() const { return syllables_; }
    static std::string naturalCode(const std::string &pinyin);
    static std::string utf8(const unsigned short *text);
private:
    Decoder(const Decoder &);
    Decoder &operator=(const Decoder &);
    void buildMap();
    std::vector<std::string> expand(const std::string &code) const;
    bool opened_;
    size_t syllables_;
    std::string code_, pinyin_, systemPath_, userPath_;
    std::map<std::string, std::vector<std::string> > pairs_;
    std::vector<Candidate> items_;
};
}
#endif

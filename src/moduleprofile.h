#ifndef BBIME_MODULEPROFILE_H
#define BBIME_MODULEPROFILE_H

#include <map>
#include <string>

namespace bbime {
struct ModuleProfile {
    typedef std::map<std::string, std::string> Values;
    ModuleProfile() : startMode("natural"), chineseMode("natural"),
        shiftCandidates(true), shiftCursor(true), chinesePunctuation(true) {}

    std::string startMode, chineseMode;
    bool shiftCandidates, shiftCursor, chinesePunctuation;

    Values values() const {
        Values out;
        out["profile/version"] = "1";
        out["profile/engine"] = "libgooglepinyin-0.1.2";
        out["input/startMode"] = startMode;
        out["input/chineseMode"] = chineseMode;
        out["input/shiftCandidates"] = shiftCandidates ? "true" : "false";
        out["input/shiftCursor"] = shiftCursor ? "true" : "false";
        out["input/chinesePunctuation"] = chinesePunctuation ? "true" : "false";
        return out;
    }

    static bool parse(const Values &values, ModuleProfile &result) {
        const Values required = ModuleProfile().values();
        if (values.size() != required.size()) return false;
        for (Values::const_iterator it = required.begin(); it != required.end(); ++it)
            if (values.find(it->first) == values.end()) return false;
        ModuleProfile next;
        if (values.find("profile/version")->second != "1" ||
            values.find("profile/engine")->second != "libgooglepinyin-0.1.2")
            return false;
        next.startMode = values.find("input/startMode")->second;
        next.chineseMode = values.find("input/chineseMode")->second;
        if (next.startMode != "natural" && next.startMode != "full" &&
            next.startMode != "english") return false;
        if (next.chineseMode != "natural" && next.chineseMode != "full") return false;
        if (next.startMode != "english" && next.chineseMode != next.startMode) return false;
        if (!boolean(values.find("input/shiftCandidates")->second, next.shiftCandidates) ||
            !boolean(values.find("input/shiftCursor")->second, next.shiftCursor) ||
            !boolean(values.find("input/chinesePunctuation")->second, next.chinesePunctuation))
            return false;
        result = next;
        return true;
    }
    static bool boolean(const std::string &value, bool &result) {
        if (value != "true" && value != "false") return false;
        result = value == "true";
        return true;
    }
};
}
#endif

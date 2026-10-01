#ifndef BBIME_MODULEPROFILE_H
#define BBIME_MODULEPROFILE_H

#include <map>
#include <string>

namespace bbime {
struct ModuleProfile {
    typedef std::map<std::string, std::string> Values;
    ModuleProfile() : startMode("natural"),
        shiftCandidates(true), shiftCursor(true), chinesePunctuation(true) {}

    std::string startMode;
    bool shiftCandidates, shiftCursor, chinesePunctuation;

    Values values() const {
        Values out;
        out["profile/version"] = "2";
        out["profile/engine"] = "libgooglepinyin-0.1.2";
        out["input/startMode"] = startMode;
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
        if (values.find("profile/version")->second != "2" ||
            values.find("profile/engine")->second != "libgooglepinyin-0.1.2")
            return false;
        next.startMode = values.find("input/startMode")->second;
        if (next.startMode != "natural" &&
            next.startMode != "english") return false;
        if (!boolean(values.find("input/shiftCandidates")->second, next.shiftCandidates) ||
            !boolean(values.find("input/shiftCursor")->second, next.shiftCursor) ||
            !boolean(values.find("input/chinesePunctuation")->second, next.chinesePunctuation))
            return false;
        result = next;
        return true;
    }
    // Only the private settings reader may migrate a known v1 snapshot.
    // Shared import continues to require the exact current schema.
    static bool migrateLegacy(const Values &values, ModuleProfile &result) {
        Values next = values;
        Values::iterator version = next.find("profile/version");
        Values::iterator start = next.find("input/startMode");
        Values::iterator chinese = next.find("input/chineseMode");
        if (version == next.end() || version->second != "1" ||
            start == next.end() || chinese == next.end()) return false;
        if (start->second != "natural" && start->second != "full" &&
            start->second != "english") return false;
        if (chinese->second != "natural" && chinese->second != "full") return false;
        if (start->second != "english" && start->second != chinese->second) return false;
        if (start->second == "full") start->second = "natural";
        version->second = "2";
        next.erase(chinese);
        return parse(next, result);
    }
    static bool boolean(const std::string &value, bool &result) {
        if (value != "true" && value != "false") return false;
        result = value == "true";
        return true;
    }
};
}
#endif

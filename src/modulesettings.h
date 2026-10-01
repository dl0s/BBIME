#ifndef BBIME_MODULESETTINGS_H
#define BBIME_MODULESETTINGS_H

#include "moduleprofile.h"
#include <QString>

namespace bbime {
// The host supplies private paths. Shared files contain preferences, never user data.
class ModuleSettings {
public:
    explicit ModuleSettings(const QString &privatePath);
    const ModuleProfile &profile() const { return m_profile; }
    bool learnSelections() const { return m_learnSelections; }
    QString error() const { return m_error; }
    bool load();
    bool save(const ModuleProfile &profile, bool learnSelections);
    bool publish(const QString &path);
    bool importProfile(const QString &path);
    static QString sharedPath(bool createDirectory);
    static bool selftest();
private:
    bool read(const QString &path, ModuleProfile &profile, bool *learnSelections);
    bool write(const QString &path, const ModuleProfile &profile, const bool *learnSelections);
    QString m_privatePath, m_error;
    ModuleProfile m_profile;
    bool m_learnSelections;
};
}
#endif

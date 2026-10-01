#include "modulesettings.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryFile>
#include <cstdio>
#include <unistd.h>

namespace bbime {
ModuleSettings::ModuleSettings(const QString &path) :
    m_privatePath(path), m_learnSelections(true) {}

bool ModuleSettings::read(const QString &path, ModuleProfile &profile, bool *learn) {
    m_error.clear();
    const QFileInfo info(path);
    if (!info.exists()) { m_error = "NOT_FOUND"; return false; }
    if (!info.isFile() || info.isSymLink() || !info.isReadable()) {
        m_error = "ACCESS"; return false;
    }
    if (info.size() <= 0 || info.size() > 8192) {
        m_error = "SIZE"; return false;
    }
    QFile source(path);
    if (!source.open(QIODevice::ReadOnly)) { m_error = "ACCESS"; return false; }
    const QByteArray bytes = source.read(8193);
    if (source.error() != QFile::NoError || bytes.isEmpty() || bytes.size() > 8192 ||
        !source.atEnd()) { m_error = "SIZE"; return false; }
    const QString privateDirectory = QFileInfo(m_privatePath).absolutePath();
    if (!QDir().mkpath(privateDirectory)) { m_error = "ACCESS"; return false; }
    QTemporaryFile snapshot(privateDirectory + "/.bbime-read-XXXXXX");
    if (!snapshot.open() || snapshot.write(bytes) != bytes.size() || !snapshot.flush()) {
        m_error = "READ"; return false;
    }
    snapshot.close();
    QSettings ini(snapshot.fileName(), QSettings::IniFormat);
    ini.setIniCodec("UTF-8");
    ini.setFallbacksEnabled(false);
    ini.sync();
    ModuleProfile::Values values;
    foreach (const QString &key, ini.allKeys())
        values[key.toStdString()] = ini.value(key).toString().toStdString();
    if (ini.status() != QSettings::NoError) {
        m_error = "FORMAT"; return false;
    }
    bool localLearn = true;
    if (learn) {
        ModuleProfile::Values::iterator it = values.find("local/learnSelections");
        if (it == values.end() || !ModuleProfile::boolean(it->second, localLearn)) {
            m_error = "LOCAL_POLICY"; return false;
        }
        values.erase(it);
    }
    ModuleProfile next;
    if (!ModuleProfile::parse(values, next) &&
        !(learn && ModuleProfile::migrateLegacy(values, next))) {
        m_error = "SCHEMA"; return false;
    }
    profile = next;
    if (learn) *learn = localLearn;
    return true;
}

bool ModuleSettings::write(const QString &path, const ModuleProfile &profile, const bool *learn) {
    m_error.clear();
    ModuleProfile checked;
    if (!ModuleProfile::parse(profile.values(), checked)) {
        m_error = "SCHEMA"; return false;
    }
    const QFileInfo destination(path);
    if (destination.isSymLink() || (destination.exists() && !destination.isFile()) ||
        !QDir().mkpath(destination.absolutePath())) {
        m_error = "ACCESS"; return false;
    }
    QTemporaryFile temporary(destination.absolutePath() + "/.bbime-settings-XXXXXX");
    if (!temporary.open()) { m_error = "ACCESS"; return false; }
    const QString stagedPath = temporary.fileName();
    temporary.close();
    {
        QSettings staged(stagedPath, QSettings::IniFormat);
        staged.setIniCodec("UTF-8");
        staged.setFallbacksEnabled(false);
        const ModuleProfile::Values values = profile.values();
        for (ModuleProfile::Values::const_iterator it = values.begin(); it != values.end(); ++it)
            staged.setValue(QString::fromStdString(it->first), QString::fromStdString(it->second));
        if (learn) staged.setValue("local/learnSelections", *learn ? "true" : "false");
        staged.sync();
        if (staged.status() != QSettings::NoError) { m_error = "WRITE"; return false; }
    }
    QFile staged(stagedPath);
    const QFile::Permissions permissions = QFile::ReadOwner | QFile::WriteOwner |
        (learn ? QFile::Permissions(0) : QFile::ReadGroup | QFile::ReadOther);
    if (!staged.setPermissions(permissions) || !staged.open(QIODevice::ReadWrite) ||
        !staged.flush() || ::fsync(staged.handle()) != 0) {
        m_error = "FLUSH"; return false;
    }
    staged.close();
    // Same-directory POSIX rename publishes the whole snapshot, not partial keys.
    if (std::rename(QFile::encodeName(stagedPath).constData(),
                    QFile::encodeName(destination.absoluteFilePath()).constData()) != 0) {
        m_error = "RENAME"; return false;
    }
    return true;
}

bool ModuleSettings::load() {
    ModuleProfile next;
    bool learn = true;
    if (!read(m_privatePath, next, &learn)) return false;
    m_profile = next;
    m_learnSelections = learn;
    return true;
}
bool ModuleSettings::save(const ModuleProfile &profile, bool learn) {
    if (!write(m_privatePath, profile, &learn)) return false;
    m_profile = profile;
    m_learnSelections = learn;
    return true;
}
bool ModuleSettings::publish(const QString &path) {
    return write(path, m_profile, 0);
}
bool ModuleSettings::importProfile(const QString &path) {
    ModuleProfile next;
    if (!read(path, next, 0)) return false;
    return save(next, m_learnSelections);
}

QString ModuleSettings::sharedPath(bool create) {
    const QDir documents(QDir::currentPath() + "/shared/documents");
    const QString base = documents.canonicalPath();
    if (base.isEmpty()) return QString();
    const QString directory = documents.absoluteFilePath("BBIME");
    if (create && !QDir().mkpath(directory)) return QString();
    const QString resolved = QDir(directory).canonicalPath();
    if (resolved != base + "/BBIME") return QString();
    return directory + "/module-profile.ini";
}

bool ModuleSettings::selftest() {
    QTemporaryFile local(QDir::currentPath() + "/data/.bbime-local-test-XXXXXX");
    QTemporaryFile shared(QDir::currentPath() + "/data/.bbime-shared-test-XXXXXX");
    if (!local.open() || !shared.open()) return false;
    const QString localPath = local.fileName(), sharedPath = shared.fileName();
    local.close();
    shared.close();
    ModuleSettings owner(localPath);
    ModuleProfile expected;
    expected.startMode = "english";
    expected.shiftCursor = false;
    bool pass = owner.save(expected, false) && owner.publish(sharedPath);
    ModuleSettings peer(localPath);
    pass = peer.load() && !peer.learnSelections() &&
        peer.profile().values() == expected.values() && pass;
    expected.shiftCandidates = false;
    pass = peer.save(expected, false) && peer.importProfile(sharedPath) &&
        !peer.learnSelections() && peer.profile().shiftCandidates && pass;
    {
        QSettings invalid(sharedPath, QSettings::IniFormat);
        invalid.setValue("profile/version", "999");
        invalid.sync();
    }
    const ModuleProfile before = peer.profile();
    pass = !peer.importProfile(sharedPath) &&
        peer.profile().values() == before.values() && !peer.learnSelections() && pass;
    pass = owner.publish(sharedPath) && pass;
    {
        QSettings invalid(sharedPath, QSettings::IniFormat);
        invalid.setValue("local/learnSelections", "true");
        invalid.sync();
    }
    pass = !peer.importProfile(sharedPath) && !peer.learnSelections() && pass;
    pass = owner.publish(sharedPath) && pass;
    {
        QSettings invalid(sharedPath, QSettings::IniFormat);
        invalid.setValue("input/shiftCursor", "maybe");
        invalid.sync();
    }
    pass = !peer.importProfile(sharedPath) && peer.profile().values() == before.values() && pass;
    {
        QFile oversized(sharedPath);
        pass = oversized.open(QIODevice::WriteOnly | QIODevice::Truncate) && pass;
        pass = oversized.write(QByteArray(8193, 'x')) == 8193 && pass;
    }
    pass = !peer.importProfile(sharedPath) && peer.error() == "SIZE" && pass;
    ModuleProfile invalidProfile = before;
    invalidProfile.startMode = "system";
    pass = !peer.save(invalidProfile, true) && !peer.learnSelections() &&
        peer.profile().values() == before.values() && pass;
    // Local v1 settings migrate, while shared files cannot transfer legacy state
    // or local learning consent. The next publication always emits schema v2.
    pass = owner.save(expected, false) && pass;
    {
        QSettings legacy(localPath, QSettings::IniFormat);
        legacy.setValue("profile/version", "1");
        legacy.setValue("input/startMode", "full");
        legacy.setValue("input/chineseMode", "full");
        legacy.sync();
    }
    pass = owner.load() && owner.profile().startMode == "natural" &&
        !owner.learnSelections() && owner.profile().values().count("input/chineseMode") == 0 && pass;
    pass = owner.publish(sharedPath) && peer.importProfile(sharedPath) &&
        peer.profile().startMode == "natural" && !peer.learnSelections() && pass;
    const ModuleProfile migrated = peer.profile();
    {
        QSettings legacy(sharedPath, QSettings::IniFormat);
        legacy.setValue("profile/version", "1");
        legacy.setValue("input/startMode", "full");
        legacy.setValue("input/chineseMode", "full");
        legacy.sync();
    }
    pass = !peer.importProfile(sharedPath) && peer.error() == "SCHEMA" &&
        peer.profile().values() == migrated.values() && !peer.learnSelections() && pass;
    {
        QSettings invalid(localPath, QSettings::IniFormat);
        invalid.setValue("unknown/key", "true");
        invalid.sync();
    }
    const ModuleProfile localBefore = owner.profile();
    pass = !owner.load() && owner.profile().values() == localBefore.values() &&
        !owner.learnSelections() && pass;
    return pass;
}
}

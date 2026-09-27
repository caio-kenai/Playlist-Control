#include "services/AppSettings.h"

namespace pc
{

AppSettings::AppSettings()
{
    juce::PropertiesFile::Options o;
    o.applicationName = "PlaylistControl";
    o.filenameSuffix = ".settings";
    o.folderName = "PlaylistControl";
    o.osxLibrarySubFolder = "Application Support";
    o.storageFormat = juce::PropertiesFile::storeAsXML;
    props_ = std::make_unique<juce::PropertiesFile> (o);
}

juce::File AppSettings::pgmFolder() const
{
    auto path = props_->getValue ("pgmFolder");
    return path.isNotEmpty() ? juce::File (path) : juce::File();
}

void AppSettings::setPgmFolder (const juce::File& folder)
{
    props_->setValue ("pgmFolder", folder.getFullPathName());
    props_->saveIfNeeded();
}

bool AppSettings::installationConfirmed() const
{
    return props_->getBoolValue ("installationConfirmed", false);
}

void AppSettings::setInstallationConfirmed (bool confirmed)
{
    props_->setValue ("installationConfirmed", confirmed);
    props_->saveIfNeeded();
}

bool AppSettings::readOnly() const
{
    return props_->getBoolValue ("readOnly", true);
}

void AppSettings::setReadOnly (bool readOnly)
{
    props_->setValue ("readOnly", readOnly);
    props_->saveIfNeeded();
}

int AppSettings::historyKeepDays() const
{
    return juce::jlimit (7, 3650, props_->getIntValue ("historyKeepDays", 180));
}

juce::File AppSettings::dataFolder()
{
    auto local = juce::File::getSpecialLocation (juce::File::windowsLocalAppData);
    return local.getChildFile ("PlaylistControl");
}

juce::File AppSettings::historyFolder()
{
    return dataFolder().getChildFile ("history");
}

juce::File AppSettings::logFolder()
{
    return dataFolder().getChildFile ("logs");
}

} // namespace pc

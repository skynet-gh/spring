/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "ConfigHandler.h"
#include "ConfigLocater.h"
#include "ConfigSource.h"
#include "System/Util.h"
#include "System/Log/ILog.h"
#include "System/UnorderedMap.hpp"
#include "System/Threading/SpringThreading.h"

#include <stdio.h>
#include <string.h>

#include <stdexcept>

/******************************************************************************/

typedef std::map<std::string, std::string> StringMap;

ConfigHandler* configHandler = NULL;


/******************************************************************************/

class ConfigHandlerImpl : public ConfigHandler
{
public:
	ConfigHandlerImpl(const std::vector<std::string>& locations, const bool safemode);
	~ConfigHandlerImpl();

	void SetString(const std::string& key, const std::string& value, bool useOverlay);
	std::string GetString(const std::string& key) const;
	bool IsSet(const std::string& key) const;
	bool IsReadOnly(const std::string& key) const;
	void Delete(const std::string& key);
	std::string GetConfigFile() const;
	const StringMap GetData() const;
	StringMap GetDataWithoutDefaults() const;
	void Update();
	void EnableWriting(bool write) { writingEnabled = write; }

protected:
	struct NamedConfigNotifyCallback {
		NamedConfigNotifyCallback(ConfigNotifyCallback c, void* h)
		: callback(c)
		, holder(h)
		{}
		ConfigNotifyCallback callback;
		void* holder;
	};

protected:
	void AddObserver(ConfigNotifyCallback observer, void* holder, const std::vector<std::string>& configs);
	void RemoveObserver(void* holder);

private:
	void RemoveDefaults();

	OverlayConfigSource* overlay;
	FileConfigSource* writableSource;
	std::vector<ReadOnlyConfigSource*> sources;

	// observer related
	spring::unsynced_map<std::string, std::vector<NamedConfigNotifyCallback>> configsToObservers;
	spring::unsynced_map<void *, std::vector<std::string>> holdersToConfigs;
	spring::mutex observerMutex;
	StringMap changedValues;
	bool writingEnabled;
};

/******************************************************************************/

#define for_each_source(it) \
	for (std::vector<ReadOnlyConfigSource*>::iterator it = sources.begin(); it != sources.end(); ++it)

#define for_each_source_const(it) \
	for (std::vector<ReadOnlyConfigSource*>::const_iterator it = sources.begin(); it != sources.end(); ++it)

/**
 * @brief Fills the list of sources based on locations.
 *
 * Sources at lower indices take precedence over sources at higher indices.
 * First there is the overlay, then one or more file sources, and the last
 * source(s) specify default values.
 */
ConfigHandlerImpl::ConfigHandlerImpl(const std::vector<std::string>& locations, const bool safemode)
{
	writingEnabled = true;
	overlay = new OverlayConfigSource();
	writableSource = new FileConfigSource(locations.front());

	size_t sources_num = 3;
	sources_num += (safemode) ? 1 : 0;
	sources_num += locations.size() - 1;
#ifdef DEDICATED
	sources_num++;
#endif
#ifdef HEADLESS
	sources_num++;
#endif
	sources.reserve(sources_num);

	sources.push_back(overlay);

	if (safemode) {
		sources.push_back(new SafemodeConfigSource());
	}

	sources.push_back(writableSource);

	std::vector<std::string>::const_iterator loc = locations.begin();
	++loc; // skip writableSource
	for (; loc != locations.end(); ++loc) {
		sources.push_back(new FileConfigSource(*loc));
	}
#ifdef DEDICATED
	sources.push_back(new DedicatedConfigSource());
#endif
#ifdef HEADLESS
	sources.push_back(new HeadlessConfigSource());
#endif
	sources.push_back(new DefaultConfigSource());

	assert(sources.size() <= sources_num);

	// Perform migrations that need to happen on every load.
	RemoveDefaults();
}

ConfigHandlerImpl::~ConfigHandlerImpl()
{
	//all observers have to be deregistered by RemoveObserver()
	assert(configsToObservers.empty());
	assert(holdersToConfigs.empty());

	for_each_source(it) {
		delete (*it);
	}
}

/**
 * @brief Remove config variables that are set to their default value.
 *
 * Algorithm is as follows:
 *
 * 1) Take all defaults from the DefaultConfigSource
 * 2) Go in reverse through the FileConfigSources
 * 3) For each one:
 *  3a) delete setting if equal to the one in `defaults'
 *  3b) add new value to `defaults' if different
 */
void ConfigHandlerImpl::RemoveDefaults()
{
	StringMap defaults = sources.back()->GetData();

	std::vector<ReadOnlyConfigSource*>::const_reverse_iterator rsource = sources.rbegin();
	for (; rsource != sources.rend(); ++rsource) {
		FileConfigSource* source = dynamic_cast<FileConfigSource*> (*rsource);
		if (source != NULL) {
			// Copy the map; we modify the original while iterating over the copy.
			StringMap file = source->GetData();
			for (StringMap::const_iterator it = file.begin(); it != file.end(); ++it) {
				// Does the key exist in `defaults'?
				StringMap::const_iterator pos = defaults.find(it->first);
				if (pos != defaults.end() && pos->second == it->second) {
					// Exists and value is equal => Delete.
					source->Delete(it->first);
				}
				else {
					// Doesn't exist or is not equal => Store new default.
					// (It will be the default for the next FileConfigSource.)
					defaults[it->first] = it->second;
				}
			}
		}
	}
}


StringMap ConfigHandlerImpl::GetDataWithoutDefaults() const
{
	StringMap cleanConfig;
	StringMap defaults = sources.back()->GetData();

	for (auto rsource = sources.crbegin(); rsource != sources.crend(); ++rsource) {
		const FileConfigSource* source = dynamic_cast<const FileConfigSource*> (*rsource);
		if (source == nullptr) continue;

		// Copy the map; we modify the original while iterating over the copy.
		StringMap file = source->GetData();
		for (auto it = file.cbegin(); it != file.cend(); ++it) {
			const auto pos = defaults.find(it->first);
			if (pos != defaults.end() && pos->second == it->second)
				continue;

			cleanConfig[it->first] = it->second;
		}
	}

	return cleanConfig;
}


void ConfigHandlerImpl::Delete(const std::string& key)
{
	for_each_source(it) {
		// The alternative to the dynamic cast is to merge ReadWriteConfigSource
		// with ReadOnlyConfigSource, but then DefaultConfigSource would have to
		// violate Liskov Substitution Principle... (by blocking modifications)
		ReadWriteConfigSource* rwcs = dynamic_cast<ReadWriteConfigSource*> (*it);
		if (rwcs != NULL) {
			rwcs->Delete(key);
		}
	}
}

bool ConfigHandlerImpl::IsSet(const std::string& key) const
{
	for_each_source_const(it) {
		if ((*it)->IsSet(key)) {
			return true;
		}
	}
	return false;
}

bool ConfigHandlerImpl::IsReadOnly(const std::string& key) const
{
	const ConfigVariableMetaData* meta = ConfigVariable::GetMetaData(key);
	if (meta==NULL) {
		return false;
	}
	return meta->GetReadOnly().Get();
}

std::string ConfigHandlerImpl::GetString(const std::string& key) const
{
	const ConfigVariableMetaData* meta = ConfigVariable::GetMetaData(key);

	for_each_source_const(it) {
		if ((*it)->IsSet(key)) {
			std::string value = (*it)->GetString(key);
			if (meta != NULL) {
				value = meta->Clamp(value);
			}
			return value;
		}
	}
	throw std::runtime_error("ConfigHandler: Error: Key does not exist: " + key +
			"\nPlease add the key to the list of allowed configuration values.");
}

/**
 * @brief Sets a config string
 *
 * This does:
 * 1) Lock file.
 * 2) Read file (in case another program modified something)
 * 3) Set data[key] = value.
 * 4) Write file (so we keep the settings in the event of a crash or error)
 * 5) Unlock file.
 *
 * We do not want conflicts when multiple instances are running
 * at the same time (which would cause data loss).
 * This would happen if e.g. unitsync and spring would access
 * the config file at the same time, if we would not lock.
 */
void ConfigHandlerImpl::SetString(const std::string& key, const std::string& value, bool useOverlay)
{
	// if we set something to be persisted,
	// we do want to override the overlay value
	if (!useOverlay) {
		overlay->Delete(key);
	}

	// Don't do anything if value didn't change.
	if (IsSet(key) && GetString(key) == value) {
		return;
	}

	if (useOverlay) {
		overlay->SetString(key, value);
	}
	else if (writingEnabled) {
		std::vector<ReadOnlyConfigSource*>::const_iterator it = sources.begin();
		bool deleted = false;

		++it; // skip overlay
		++it; // skip writableSource

		for (; it != sources.end(); ++it) {
			if ((*it)->IsSet(key)) {
				if ((*it)->GetString(key) == value) {
					// key is being set to the default value,
					// delete the key instead of setting it.
					writableSource->Delete(key);
					deleted = true;
				}
				break;
			}
		}

		if (!deleted) {
			// set key to the specified non-default value
			writableSource->SetString(key, value);
		}
	}

	std::lock_guard<spring::mutex> lck(observerMutex);
	changedValues[key] = value;
}

void ConfigHandlerImpl::Update()
{
	std::lock_guard<spring::mutex> lck(observerMutex);
	for (StringMap::const_iterator ut = changedValues.begin(); ut != changedValues.end(); ++ut) {
		const std::string& key = ut->first;
		const std::string& value = ut->second;
		if (configsToObservers.find(key) != configsToObservers.end()) {
			for (const NamedConfigNotifyCallback& ncnc: configsToObservers[key]) {
				(ncnc.callback)(key, value);
			}
		}
	}
	changedValues.clear();
}

std::string ConfigHandlerImpl::GetConfigFile() const {
	return writableSource->GetFilename();
}

const StringMap ConfigHandlerImpl::GetData() const {
	StringMap data;
	for_each_source_const(it) {
		const StringMap& sourceData = (*it)->GetData();
		// insert doesn't overwrite, so this preserves correct overrides
		data.insert(sourceData.begin(), sourceData.end());
	}
	return data;
}


void ConfigHandlerImpl::AddObserver(ConfigNotifyCallback observer, void* holder, const std::vector<std::string>& configs) {
	std::lock_guard<spring::mutex> lck(observerMutex);
	for (const std::string& config: configs) {
		configsToObservers[config].emplace_back(observer, holder);
		holdersToConfigs[holder].push_back(config);
	}
}

void ConfigHandlerImpl::RemoveObserver(void* holder) {
	std::lock_guard<spring::mutex> lck(observerMutex);
	for (const std::string& config: holdersToConfigs[holder]) {
		spring::VectorEraseIf(configsToObservers[config], [&](NamedConfigNotifyCallback& ncnc) {
			return (ncnc.holder == holder);
		});

		if (configsToObservers[config].empty())
			configsToObservers.erase(config);
	}

	holdersToConfigs.erase(holder);
}


/******************************************************************************/

void ConfigHandler::Instantiate(const std::string configSource, const bool safemode)
{
	Deallocate();

	std::vector<std::string> locations;
	if (!configSource.empty()) {
		locations.push_back(configSource);
	} else {
		ConfigLocater::GetDefaultLocations(locations);
	}
	assert(!locations.empty());

	// log here so unitsync shows configuration source(s), too
	std::vector<std::string>::const_iterator loc = locations.begin();
	LOG("Using configuration source: \"%s\"", loc->c_str());
	for (++loc; loc != locations.end(); ++loc) {
		LOG("Using additional configuration source: \"%s\"", loc->c_str());
	}

	configHandler = new ConfigHandlerImpl(locations, safemode);

	//assert(configHandler->GetString("test") == "x y z");
}

void ConfigHandler::Deallocate()
{
	spring::SafeDelete(configHandler);
}

/******************************************************************************/

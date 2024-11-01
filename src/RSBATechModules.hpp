#pragma once
#include "plugin.hpp"
#include "digital/ScaledMapParam.hpp"

namespace RSBATechModules {

static const int MAX_CHANNELS = 300;
static const int MAX_NPRN_ID = 299; // 0 to MAX_NPRN_ID

static const char LOAD_MIDIMAP_FILTERS[] = "VCV Rack module preset (.vcvm):vcvm, JSON (.json):json";
static const char SAVE_JSON_FILTERS[] = "JSON (.json):json";

#define MIDIOPTION_VELZERO_BIT 0

enum class NPRNMODE {
	DIRECT = 0,
	PICKUP1 = 1,
	PICKUP2 = 2,
	TOGGLE = 3,
	TOGGLE_VALUE = 4
};

static const std::set<std::pair<std::string, std::string>> AUTOMAP_EXCLUDED_MODULES {
	std::pair<std::string, std::string>("RSBATechModules", "OrestesOne"),
	std::pair<std::string, std::string>("MindMeldModular", "PatchMaster")
};

static const std::string DEFAULT_LIBRARY_FILENAME = "midimap-library.json";
static const std::string FACTORY_LIBRARY_FILENAME = "factory-midimap-library.json";

struct MemParam {
	int paramId = -1;
    int nprn = -1;
    NPRNMODE nprnMode;
	std::string label = "";
	int midiOptions = 0;
	float slew = 0.f;
	float min = 0.f;
	float max = 1.f;
	int64_t moduleId = -1; // Only used for storing rack mapping parameters
};

struct MemModule {
	std::string pluginName;
	std::string moduleName;
	bool autoMapped;
	std::list<MemParam*> paramMap;
	~MemModule() {
		for (auto it : paramMap) delete it;
	}
	void reset() {
		for (auto it : paramMap) delete it;
		paramMap.clear();	
	}

};

struct RackMappedModuleListItem {
    public:
        RackMappedModuleListItem(
            const std::string &moduleKey,
            const std::string &moduleDisplayName,
            const float moduleY,
            const float moduleX) : m_moduleKey(moduleKey), m_moduleDisplayName(moduleDisplayName), m_y(moduleY), m_x(moduleX) {}
        const std::string &getModuleKey() const { return m_moduleKey; }
        const std::string &getModuleDisplayName() const { return m_moduleDisplayName; }
        float getY() const { return m_y; }
        float getX() const { return m_x; }
    private:
        std::string m_moduleKey; // Uniquely identifies the module type (plugin + model)
        std::string m_moduleDisplayName; // Module display name
        float m_y; // Module instance rack y position
        float m_x; // Module instance rack x position
};

enum MIDIMODE {
	MIDIMODE_DEFAULT = 0,
	MIDIMODE_LOCATE = 1
};

struct RackParam : ScaledMapParam<int> {
	enum class CLOCKMODE {
		OFF = 0,
		ARM = 1,
		ARM_DEFERRED_FEEDBACK = 2
	};

	CLOCKMODE clockMode = CLOCKMODE::OFF;
	int clockSource = 0;

	int setValueDeffered;
	int getValueLast;
	bool resetToDefault;

	void reset(bool resetSettings = true) override {
		if (resetSettings) {
			clockMode = CLOCKMODE::OFF;
			clockSource = 0;
		}
		resetToDefault = false;
		ScaledMapParam<int>::reset(resetSettings);
	}

	void setValue(int i) override {
		switch (clockMode) {
			case CLOCKMODE::OFF:
				ScaledMapParam<int>::setValue(i);
				break;
			case CLOCKMODE::ARM:
			case CLOCKMODE::ARM_DEFERRED_FEEDBACK:
				setValueDeffered = i;
				break;
		}
	}

	int getValue() override {
		switch (clockMode) {
			case CLOCKMODE::OFF:
				return ScaledMapParam<int>::getValue();
			case CLOCKMODE::ARM:
				return setValueDeffered;
			case CLOCKMODE::ARM_DEFERRED_FEEDBACK:
				return getValueLast;
		}
		return 0;
	}

	void tick(int clock) {
		if (clockMode != CLOCKMODE::OFF && clockSource == clock) {
			ScaledMapParam<int>::setValue(setValueDeffered);
		}
		if (clockMode == CLOCKMODE::ARM_DEFERRED_FEEDBACK) {
			getValueLast = ScaledMapParam<int>::getValue();
		}
	}

	bool isNear(int value, int jump = -1) {
		if (value == -1) return false;
		int p = getValue();
		int delta3p = (limitMaxT - limitMinT + 1) * 3 / 100;
		bool r = p - delta3p <= value && value <= p + delta3p;

		if (jump >= 0) {
			int delta7p = (limitMaxT - limitMinT + 1) * 7 / 100;
			r = r && p - delta7p <= jump && jump <= p + delta7p;
		}

		return r;
	}

    void setValueToDefault() {
	    Param* param = paramQuantity->getParam();
        if (param) {
            resetToDefault = true;
        }
	}

	void process(float sampleTime = -1.f, bool force = false) override {
        if (resetToDefault) {
            paramQuantity->reset();
            resetToDefault = false;
            return;
        }
        ScaledMapParam<int>::process(sampleTime, force);
    }
};



}
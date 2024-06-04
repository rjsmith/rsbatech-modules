#pragma once
#include "plugin.hpp"

namespace Orestes {
namespace OrestesOne {

static const int MAX_CHANNELS = 300;
static const int MAX_NPRN_ID = 299; // 0 to MAX_NPRN_ID

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

struct MemParam {
	int paramId = -1;
    int nprn = -1;
    NPRNMODE nprnMode;
	std::string label;
	int midiOptions = 0;
	float slew = 0.f;
	float min = 0.f;
	float max = 1.f;
};

struct MemModule {
	std::string pluginName;
	std::string moduleName;
	bool autoMapped;
	std::list<MemParam*> paramMap;
	~MemModule() {
		for (auto it : paramMap) delete it;
	}
};

} // namespace OrestesOne
} // namespace Orestes
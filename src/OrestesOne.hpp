#pragma once
#include "plugin.hpp"

namespace Orestes {
namespace OrestesOne {

static const int MAX_CHANNELS = 640;

#define MIDIOPTION_VELZERO_BIT 0

enum class NPRNMODE {
	DIRECT = 0,
	PICKUP1 = 1,
	PICKUP2 = 2,
	TOGGLE = 3,
	TOGGLE_VALUE = 4
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
	std::list<MemParam*> paramMap;
	~MemModule() {
		for (auto it : paramMap) delete it;
	}
};

} // namespace OrestesOne
} // namespace Orestes
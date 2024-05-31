#include "plugin.hpp"
#include "OrestesOne.hpp"
#include "MapModuleBase.hpp"
#include "digital/ScaledMapParam.hpp"
#include "components/MenuLabelEx.hpp"
#include "components/SubMenuSlider.hpp"
#include "components/MidiWidget.hpp"
#include "ui/ParamWidgetContextExtender.hpp"
#include "ui/OverlayMessageWidget.hpp"
#include <osdialog.h>
#include <vector>
#include <iomanip>
#include <sstream>
#include <string>
#include <array>

namespace Orestes {
namespace OrestesOne {

static const char LOAD_MIDIMAP_FILTERS[] = "VCV Rack module preset (.vcvm):vcvm, JSON (.json):json";
static const char SAVE_JSON_FILTERS[] = "JSON (.json):json";

struct OrestesOneOutput : midi::Output {
	std::array<int, 16384> lastNPRNValues{};

	OrestesOneOutput() {
		reset();
	}

	void reset() {
		std::fill_n(lastNPRNValues.begin(), 16384, -1);
	}

    void setNPRNValue(int value, int nprn, int valueNprnIn, bool force = false) {
		if ((value == lastNPRNValues[nprn] || value == valueNprnIn) && !force)
			return;
		lastNPRNValues[nprn] = value;
		// NPRN
		sendCCMsg(99, nprn / 128);
		sendCCMsg(98, nprn % 128);
		sendCCMsg(6, value / 128);
		sendCCMsg(38, value % 128);
		sendCCMsg(101, 127);
		sendCCMsg(100, 127);
		// INFO("Sending NPRN %d value %d, force %s", nprn, value, force ? "true" : "false");
	}

    void sendCCMsg(int cc, int value) {
        midi::Message m;
        m.setStatus(0xb);
        m.setNote(cc);
        m.setValue(value);
        sendMessage(m);
    }

};

struct E1MappedModuleListItem {
    public:
        E1MappedModuleListItem(
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

struct E1MidiOutput : OrestesOneOutput {

    midi::Message m;

    E1MidiOutput() {
		reset();
		m.bytes.reserve(512);
	}

	void reset() {
		OrestesOneOutput::reset();
    	m.bytes.clear();
	}

    void sendE1ControlUpdate(int id, const char* name, const char* displayValue) {
        // See https://jansson.readthedocs.io/en/2.12/apiref.html
        // SysEx
        int e1ControllerId = id + 2;

        m.bytes.clear();
        m.bytes.resize(0);
        // SysEx header byte
        m.bytes.push_back(0xF0);
        // SysEx header byte
        // Electra One MIDI manufacturer Id
        m.bytes.push_back(0x00);
        m.bytes.push_back(0x21);
        m.bytes.push_back(0x45);
        // Update runtime command
        m.bytes.push_back(0x14);
        // Control
        m.bytes.push_back(0x07);
        // controlId LSB
        m.bytes.push_back(e1ControllerId & 0x7F);
        // controlId MSB
        m.bytes.push_back(e1ControllerId >> 7);

        // Build control-update-json-data
        // {"name":name,"value":{"id":"value","text":displayValue}}
        json_t* valueJ = json_object();
        json_object_set_new(valueJ, "text", json_string(displayValue));
        json_t* rootJ = json_object();
        json_object_set_new(rootJ, "name", json_string(name));
        json_object_set_new(rootJ, "value", valueJ);
        char* json = json_dumps(rootJ, JSON_COMPACT | JSON_ENSURE_ASCII);

        for( char* it = json; *it; ++it )
          m.bytes.push_back((uint8_t)*it);

        // SysEx closing byte
        m.bytes.push_back(0xf7);
        // INFO("Sending control update e1 ctrl %id, value %s", id, displayValue);
        sendMessage(m);

   }

   /**
    * Update a E1 preset group label.
    * Executes a lua command on the E1 of form:
    *
    * updateGroupLabel(1, "New label")
    */
   void changeE1Module(const char* moduleDisplayName, float moduleY, float moduleX) {

        std::stringstream ss;
        ss << "changeE1Module(\"" << moduleDisplayName << "\", " << string::f("%g", moduleY) << ", " << string::f("%g", moduleX) << ")";
        sendE1ExecuteLua(ss.str().c_str());

   }

   void endChangeE1Module() {
         std::stringstream ss;
         ss << "endChangeE1Module()";
         sendE1ExecuteLua(ss.str().c_str());
   }

   /**
    * Sends a series of lua commands to E1 to transmit the list of mapped modules in the current Rack patch.
    * Pass a begin() and end() of a list or vector of MappedModule objects.
    */
   template <class Iterator>
   void sendModuleList(Iterator begin, Iterator end) {

        // 1. Send a startMappedModuleList lua command
        startMappedModuleList();

        // 2. Loop over iterator, send a mappedModuleInfo lua command for each
        for (Iterator it = begin; it != end; ++it) {
            mappedModuleInfo(*it);
        }

        // 3. Finish with a endMappedModuleList lua command
        endMappedModuleList();
   }

    void startMappedModuleList() {
        std::stringstream ss;
        ss << "startMML()";
        sendE1ExecuteLua(ss.str().c_str());
    }
    void mappedModuleInfo(E1MappedModuleListItem& m) {
        std::stringstream ss;
        ss << "mappedMI(\"" << m.getModuleKey() << "\", \"" << m.getModuleDisplayName() <<  "\", " << string::f("%g", m.getY()) << ", " << string::f("%g", m.getX()) << ")";
        sendE1ExecuteLua(ss.str().c_str());
    }
    void endMappedModuleList() {
         std::stringstream ss;
         ss << "endMML()";
         sendE1ExecuteLua(ss.str().c_str());
    }

   /**
    * Execute a Lua command on the Electra One
    * @see https://docs.electra.one/developers/midiimplementation.html#execute-lua-command
    */
   void sendE1ExecuteLua(const char* luaCommand) {
        // INFO("Execute Lua %s", luaCommand);

        m.bytes.clear();
        // SysEx header byte
        m.bytes.push_back(0xF0);
        // SysEx header byte
        // Electra One MIDI manufacturer Id
        m.bytes.push_back(0x00);
        m.bytes.push_back(0x21);
        m.bytes.push_back(0x45);
        // Execute command
        m.bytes.push_back(0x8);
        // Lua command
        m.bytes.push_back(0xD);
        for( char* it = (char*)luaCommand; *it; ++it )
                  m.bytes.push_back((uint8_t)*it);
        // SysEx closing byte
        m.bytes.push_back(0xf7);

        // INFO("Sending bytes %s", hexStr(m.bytes.data(), m.getSize()).data());
        sendMessage(m);


   }

    std::string hexStr(const uint8_t *data, int len)
    {
        std::stringstream ss;
        ss << std::hex;

        for( int i(0) ; i < len; ++i )
             ss << std::setw(2) << std::setfill('0') << (int)data[i];

        return ss.str();
    }

};

enum MIDIMODE {
	MIDIMODE_DEFAULT = 0,
	MIDIMODE_LOCATE = 1
};


struct OrestesOneParam : ScaledMapParam<int> {
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


struct OrestesOneModule : Module {
	/** [Stored to Json] */
	midi::InputQueue midiInput;
	/** [Stored to Json] */
	E1MidiOutput midiOutput;

	/** [Stored to Json] */
	midi::InputQueue midiCtrlInput;
	/** [Stored to Json] */
	E1MidiOutput midiCtrlOutput;

	/** [Stored to JSON] */
	int panelTheme = 0;

	enum ParamIds {
		PARAM_APPLY,
		PARAM_PREV,
		PARAM_NEXT,
		NUM_PARAMS
	};
	enum InputIds {
		NUM_INPUTS
	};
	enum OutputIds {
		NUM_OUTPUTS
	};
	enum LightIds {
		LIGHT_APPLY,
		NUM_LIGHTS
	};


	struct MidiNPRNAdapter {
        OrestesOneModule* module;
        int id;
        int current = -1;
        uint32_t lastTs = 0;

        /** [Stored to Json] */
        int nprn;
        /** [Stored to Json] */
        NPRNMODE nprnMode;

        bool process() {
            int previous = current;

            if (module->valuesNprnTs[nprn] > lastTs) {
                current = module->valuesNprn[nprn];
                lastTs = module->ts;
            }

            return current >= 0 && current != previous;
        }

        int getValue() {
            return current;
        }

        void setValue(int value, bool sendOnly) {
            if (nprn == -1) return;
            module->midiOutput.setNPRNValue(value, nprn, module->valuesNprn[nprn], current == -1);
            if (!sendOnly) current = value;
        }

        void reset() {
            nprn = -1;
            current = -1;
        }

        void resetValue() {
            current = -1;
        }

        int getNprn() {
            return nprn;
        }

        void setNprn(int nprn) {
            this->nprn = nprn;
            current = -1;
        }

        bool get14bit() {
            return true;
        }

        void set14bit(bool value) {
            module->midiParam[id].setLimits(0, 128 * 128 - 1, -1);
        }
    };

	/** Number of maps */
	int mapLen = 0;
    MidiNPRNAdapter nprns[MAX_CHANNELS];
	/** [Stored to JSON] */
	int midiOptions[MAX_CHANNELS];
	/** [Stored to JSON] */
	bool midiIgnoreDevices;
	/** [Stored to JSON] */
	bool clearMapsOnLoad;

    // Flags to indicate command received from E1 to be processed in widget tick() thread
    bool e1ProcessNext;
    bool e1ProcessPrev;
    bool e1ProcessSelect;
    math::Vec e1SelectedModulePos;
    bool e1ProcessResendMIDIFeedback;

    // E1 Process flags
    int sendE1EndMessage = 0;
    bool e1ProcessListMappedModules;
    // Re-usable list of mapped modules
    std::vector< E1MappedModuleListItem > e1MappedModuleList;
    size_t INITIAL_MAPPED_MODULE_LIST_SIZE = 100;
    bool e1ProcessResetParameter;
    int e1ProcessResetParameterNPRN;

	/** [Stored to Json] The mapped param handle of each channel */
	ParamHandleIndicator paramHandles[MAX_CHANNELS];

    /** NPRN Parsing */
    bool isPendingNPRN = true; // TRUE if processing a NPRN sequence, start CC 99, end CC 100
    int lastNPRNcc; // Tracks last received NPRN message
    std::array<uint8_t, 4> nprnMsg{}; // Stores raw received nprn CC values (ignoring the NULL CC commands)

	/** Channel ID of the learning session */
	int learningId;
	/** Wether multiple slots or just one slot should be learned */
	bool learnSingleSlot = false;
	/** Whether the NPRN has been set during the learning session */
    bool learnedNprn;
    int learnedNprnLast = -1;
	/** Whether the param has been set during the learning session */
	bool learnedParam;

	/** [Stored to Json] */
	bool textScrolling = true;
	/** [Stored to Json] */
	std::string textLabel[MAX_CHANNELS];
	/** [Stored to Json] */
	bool locked;

	NVGcolor mappingIndicatorColor = nvgRGB(0xff, 0xff, 0x40);
	/** [Stored to Json] */
	bool mappingIndicatorHidden = false;

	uint32_t ts = 0;

	/** The value of each NPRN number */
    int valuesNprn[16384];
    uint32_t valuesNprnTs[16284];
	
	MIDIMODE midiMode = MIDIMODE::MIDIMODE_DEFAULT;

	/** Track last values */
	int lastValueIn[MAX_CHANNELS];
	int lastValueInIndicate[MAX_CHANNELS];
	int lastValueOut[MAX_CHANNELS];

	dsp::RingBuffer<int, 8> overlayQueue;
	/** [Stored to Json] */
	bool overlayEnabled;

	/** [Stored to Json] */
	OrestesOneParam midiParam[MAX_CHANNELS];
	/** [Stored to Json] */
	bool midiResendPeriodically;
	dsp::ClockDivider midiResendDivider;

	dsp::ClockDivider processDivider;
	/** [Stored to Json] */
	int processDivision;
	dsp::ClockDivider indicatorDivider;

	// MEM-
	// Pointer of the MEM's attribute
	int64_t expMemModuleId = -1;
	/** [Stored to JSON] */
	std::map<std::pair<std::string, std::string>, MemModule*> midiMap;

	OrestesOneModule() {
		panelTheme = pluginSettings.panelThemeDefault;

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam<BufferedTriggerParamQuantity>(PARAM_PREV, 0.f, 1.f, 0.f, "Scan for previous module mapping");
		configParam<BufferedTriggerParamQuantity>(PARAM_NEXT, 0.f, 1.f, 0.f, "Scan for next module mapping");
		configParam<BufferedTriggerParamQuantity>(PARAM_APPLY, 0.f, 1.f, 0.f, "Apply mapping");

		for (int id = 0; id < MAX_CHANNELS; id++) {
			paramHandles[id].color = mappingIndicatorColor;
			APP->engine->addParamHandle(&paramHandles[id]);
			midiParam[id].setLimits(0, 16383, -1);
			nprns[id].module = this;
			nprns[id].id = id;
		}
		indicatorDivider.setDivision(2048);
		midiResendDivider.setDivision(APP->engine->getSampleRate() / 2);
		onReset();
		e1MappedModuleList.reserve(INITIAL_MAPPED_MODULE_LIST_SIZE);
	}

	~OrestesOneModule() {
		for (int id = 0; id < MAX_CHANNELS; id++) {
			APP->engine->removeParamHandle(&paramHandles[id]);
		}
	}

	void onReset() override {
		learningId = -1;
		learnedNprn = false;
		learnedParam = false;
		// Use NoLock because we're already in an Engine write-lock if Engine::resetModule().
		// We also might be in the MIDIMap() constructor, which could cause problems, but when constructing, all ParamHandles will point to no Modules anyway.
		clearMaps_NoLock();
		mapLen = 1;
		for (int i = 0; i < 128*128; i++) {
		    valuesNprn[i] = -1;
		    valuesNprnTs[i] = 0;
		}
		for (int i = 0; i < MAX_CHANNELS; i++) {
			lastValueIn[i] = -1;
			lastValueOut[i] = -1;
			nprns[i].nprnMode = NPRNMODE::DIRECT;
			textLabel[i] = "";
			midiOptions[i] = 0;
			midiParam[i].reset();
		}
		locked = false;
		midiInput.reset();
		midiOutput.reset();
		midiOutput.midi::Output::reset();
		midiCtrlInput.reset();
		midiCtrlOutput.reset();
		midiCtrlOutput.midi::Output::reset();
		midiIgnoreDevices = false;
		midiResendPeriodically = false;
		midiResendDivider.reset();
		processDivision = 512;
		processDivider.setDivision(processDivision);
		processDivider.reset();
		overlayEnabled = true;
		clearMapsOnLoad = false;
		
		e1ProcessNext = false;
		e1ProcessPrev = false;
		e1ProcessSelect = false;
		e1ProcessListMappedModules = false;
		e1ProcessResetParameter = false;

		resetMap();
	}

	void resetMap() {
		for (auto it : midiMap) {
			delete it.second;
		}
		midiMap.clear();
	}

	void onSampleRateChange() override {
		midiResendDivider.setDivision(APP->engine->getSampleRate() / 2);
	}

    void sendE1Feedback(int id) {

       if (id >= mapLen) return;
       // Update E1 control with mapped parameter name and value
       if (paramHandles[id].moduleId < 0) return;

       ModuleWidget* mw = APP->scene->rack->getModule(paramHandles[id].moduleId);
       if (!mw) return;

       Module* m = mw->getModule();
       if (!m) return;
       //idx of parameter in target module
       int paramId = paramHandles[id].paramId;
       if (paramId >= (int)m->params.size()) return;

       ParamQuantity* paramQuantity = m->paramQuantities[paramId];
       std::stringstream ss;
       ss << paramQuantity->getDisplayValueString() << " " << paramQuantity->getUnit();
       midiCtrlOutput.sendE1ControlUpdate(nprns[id].getNprn(), paramQuantity->getLabel().c_str(), ss.str().c_str());
    }


	void process(const ProcessArgs &args) override {
		ts++;

		// Aquire new MIDI messages from the queue
		midi::Message msg;
		bool midiReceived = false;
		while (midiInput.tryPop(&msg, args.frame)) {
			bool r = midiProcessMessage(msg);
			midiReceived = midiReceived || r;
		}

		// Handle indicators - blinking
		if (indicatorDivider.process()) {
			float t = indicatorDivider.getDivision() * args.sampleTime;
			for (int i = 0; i < mapLen; i++) {
				paramHandles[i].color = mappingIndicatorHidden ? color::BLACK_TRANSPARENT : mappingIndicatorColor;
				if (paramHandles[i].moduleId >= 0) {
					paramHandles[i].process(t, learningId == i);
				}
			}
		}

		if (e1ProcessResendMIDIFeedback || (midiResendPeriodically && midiResendDivider.process())) {
			midiResendFeedback();
		}

		// // Expanders
		// bool expMemFound = false;
		// bool expCtxFound = false;
		// bool expClkFound = false;
		// Module* exp = rightExpander.module;
		// for (int i = 0; i < 3; i++) {
		// 	if (!exp) break;
		// 	if (exp->model == modelOrestesOneMem && !expMemFound) {
		// 		expMemStorage = reinterpret_cast<std::map<std::pair<std::string, std::string>, MemModule*>*>(exp->leftExpander.consumerMessage);
		// 		expMem = exp;
		// 		expMemFound = true;
		// 		exp = exp->rightExpander.module;
		// 		continue;
		// 	}
		// 	if (exp->model == modelOrestesOneCtx && !expCtxFound) {
		// 		expCtx = exp;
		// 		expCtxFound = true;
		// 		exp = exp->rightExpander.module;
		// 		continue;
		// 	}
		// 	if (exp->model == modelOrestesOneClk && !expClkFound) {
		// 		expClk = exp;
		// 		expClkFound = true;
		// 		exp = exp->rightExpander.module;
		// 		continue;
		// 	}
		// 	break;
		// }
		// if (!expMemFound) {
		// 	expMemStorage = NULL;
		// 	expMem = NULL;
		// }
		// if (!expCtxFound) {
		// 	expCtx = NULL;
		// }
		// if (!expClkFound) {
		// 	if (expClk) {
		// 		for (int i = 0; i < MAX_CHANNELS; i++) {
		// 			midiParam[i].clockMode = OrestesOneParam::CLOCKMODE::OFF;
		// 			midiParam[i].clockSource = 0;
		// 		}
		// 	}
		// 	expClk = NULL;
		// }
		// else {
		// 	expClkProcess();
		// }

        // Only step channels when some midi event has been received. Additionally
        // step channels for parameter changes made manually at a lower frequency . Notice
        // that midi allows about 1000 messages per second, so checking for changes more often
        // won't lead to higher precision on midi output.
        if (processDivider.process() || midiReceived) {
            processMappings(args.sampleTime);
            processE1Commands();
        }

	}

	void processMappings(float sampleTime) {
		float st = sampleTime * float(processDivision);

		for (int id = 0; id < mapLen; id++) {
			int nprn = nprns[id].getNprn();
			if (nprn < 0)
				continue;

			// Get Module
			Module* module = paramHandles[id].module;
			if (!module)
				continue;

			// Get ParamQuantity
			int paramId = paramHandles[id].paramId;
			ParamQuantity* paramQuantity = module->paramQuantities[paramId];
			if (!paramQuantity)
				continue;

			if (!paramQuantity->isBounded())
				continue;

			switch (midiMode) {
				case MIDIMODE::MIDIMODE_DEFAULT: {
					midiParam[id].paramQuantity = paramQuantity;
					int t = -1;

 
                    if (!e1ProcessResetParameter && nprn >= 0 && nprns[id].process()) {
					    // Check if NPRN value has been set and changed
						switch (nprns[id].nprnMode) {
							case NPRNMODE::DIRECT:
								if (lastValueIn[id] != nprns[id].getValue()) {
									lastValueIn[id] = nprns[id].getValue();
									t = nprns[id].getValue();
								}
								break;
							case NPRNMODE::PICKUP1:
								if (lastValueIn[id] != nprns[id].getValue()) {
									if (midiParam[id].isNear(lastValueIn[id])) {
										midiParam[id].resetFilter();
										t = nprns[id].getValue();
									}
									lastValueIn[id] = nprns[id].getValue();
								}
								break;
							case NPRNMODE::PICKUP2:
								if (lastValueIn[id] != nprns[id].getValue()) {
									if (midiParam[id].isNear(lastValueIn[id], nprns[id].getValue())) {
										midiParam[id].resetFilter();
										t = nprns[id].getValue();
									}
									lastValueIn[id] = nprns[id].getValue();
								}
								break;
							case NPRNMODE::TOGGLE:
								if (nprns[id].getValue() > 0 && (lastValueIn[id] == -1 || lastValueIn[id] >= 0)) {
									t = midiParam[id].getLimitMax();
									lastValueIn[id] = -2;
								}
								else if (nprns[id].getValue() == 0 && lastValueIn[id] == -2) {
									t = midiParam[id].getLimitMax();
									lastValueIn[id] = -3;
								}
								else if (nprns[id].getValue() > 0 && lastValueIn[id] == -3) {
									t = midiParam[id].getLimitMin();
									lastValueIn[id] = -4;
								}
								else if (nprns[id].getValue() == 0 && lastValueIn[id] == -4) {
									t = midiParam[id].getLimitMin();
									lastValueIn[id] = -1;
								}
								break;
							case NPRNMODE::TOGGLE_VALUE:
								if (nprns[id].getValue() > 0 && (lastValueIn[id] == -1 || lastValueIn[id] >= 0)) {
									t = nprns[id].getValue();
									lastValueIn[id] = -2;
								}
								else if (nprns[id].getValue() == 0 && lastValueIn[id] == -2) {
									t = midiParam[id].getValue();
									lastValueIn[id] = -3;
								}
								else if (nprns[id].getValue() > 0 && lastValueIn[id] == -3) {
									t = midiParam[id].getLimitMin();
									lastValueIn[id] = -4;
								}
								else if (nprns[id].getValue() == 0 && lastValueIn[id] == -4) {
									t = midiParam[id].getLimitMin();
									lastValueIn[id] = -1;
								}
								break;
						}
					}

			        int v;

					// Set a new value for the mapped parameter
					if (e1ProcessResetParameter && nprn == e1ProcessResetParameterNPRN) {
                        midiParam[id].setValueToDefault();
                        e1ProcessResetParameterNPRN = -1;
                        lastValueOut[id] = -1;
                        nprns[id].resetValue(); // Forces NPRN adapter to emit NPRM message out

                    } else if (t >= 0) {
						midiParam[id].setValue(t);
						if (overlayEnabled && overlayQueue.capacity() > 0) overlayQueue.push(id);
					}

					// Apply value on the mapped parameter (respecting slew and scale)
					midiParam[id].process(st);

					// Retrieve the current value of the parameter (ignoring slew and scale)
					v = midiParam[id].getValue();

					// Midi feedback
					if (lastValueOut[id] != v) {

						if (!e1ProcessResetParameter && nprn >= 0 && nprns[id].nprnMode == NPRNMODE::DIRECT)
                        							lastValueIn[id] = v;
				        // Send manually altered parameter change out to MIDI
						nprns[id].setValue(v, lastValueIn[id] < 0);
						lastValueOut[id] = v;
						sendE1Feedback(id);
                        e1ProcessResetParameter = false;
                        // If we are broadcasting parameter updates when switching modules,
                        // record that we have now sent this parameter
                        if (sendE1EndMessage > 0) sendE1EndMessage--;
					}
				} break;

				case MIDIMODE::MIDIMODE_LOCATE: {
					bool indicate = false;
					if ((nprn >= 0 && nprns[id].getValue() >= 0) && lastValueInIndicate[id] != nprns[id].getValue()) {
						lastValueInIndicate[id] = nprns[id].getValue();
						indicate = true;
					}
					if (indicate) {
						ModuleWidget* mw = APP->scene->rack->getModule(paramQuantity->module->id);
						paramHandles[id].indicate(mw);
					}
				} break;
			}
		}

		// Send end of mapping message, when switching between saved module mappings
		// NB: Module parameters may get processed in multiple process() calls, so we need to wait until all the module parameters
		// have been send to E1 before sending the final endChangeE1Module command to the E1.
        if (sendE1EndMessage == 1) {
          // Send end module mapping message to E1
          endChangeE1Module();
          sendE1EndMessage = 0;
        }
	}

	bool midiProcessMessage(midi::Message msg) {
		switch (msg.getStatus()) {
			// cc
			case 0xb: {
			    if (isPendingNPRN) {
			        return midiNPRN(msg);
			    } else {
				    return midiCc(msg);
				}
			}
			// sysex
			case 0xf: {
			  return parseE1SysEx(msg);
			}
			default: {
				return false;
			}
		}
	}

    /**
     * Additional process() handlers for commands received from E1
     */
    void processE1Commands() {

        if (e1ProcessListMappedModules) {
            e1ProcessListMappedModules = false;
            sendE1MappedModulesList();
            return;
        }

        if (e1ProcessResendMIDIFeedback) {
            e1ProcessResendMIDIFeedback = false;
        }
    }

    /**
     * Build list of mapped Rack modules and transmit to E1
     */
    void sendE1MappedModulesList() {

        e1MappedModuleList.clear();

        // Get list of all modules in current Rack, sorted by module y then x position
        std::list<Widget*> modules = APP->scene->rack->getModuleContainer()->children;
        auto sort = [&](Widget* w1, Widget* w2) {
            auto t1 = std::make_tuple(w1->box.pos.y, w1->box.pos.x);
            auto t2 = std::make_tuple(w2->box.pos.y, w2->box.pos.x);
            return t1 < t2;
        };
        modules.sort(sort);
        std::list<Widget*>::iterator it = modules.begin();

        e1MappedModuleList.clear();
        // Scan over all rack modules, determine if each is parameter-mapped
        for (; it != modules.end(); it++) {
            ModuleWidget* mw = dynamic_cast<ModuleWidget*>(*it);
            Module* m = mw->module;
            if (expMemTest(m)) {
                // Add module to mapped module list
                // If there more than one instance of  mapped module in the rack, it will appear in the list multiple time
                auto key = string::f("%s %s", m->model->plugin->slug.c_str(), m->model->slug.c_str());
                E1MappedModuleListItem item = {
                    key,
                    m->model->name,
                    mw->box.pos.y,
                    mw->box.pos.x
                };
                e1MappedModuleList.push_back(item);
            }
        }

        midiCtrlOutput.sendModuleList(e1MappedModuleList.begin(), e1MappedModuleList.end());

    }

    /**
     * Parses VCVRack E1 Sysex sent from the Electra One
     * Message Format:
     *
     * Byte(s)
     * =======
     * [0 ]        0xF0 SysEx header byte
     * [1-3]       0x00 0x7F 0x7F Placeholder MIDI Manufacturer Id
     * [4]         0x01 Command
     * [...]
     * [end]       0xF7 SysEx end byte
     * ---
     * Command: Next
     * [5]         0x01 Next mapped module
     *
     * Command: Prev
     * [5]         0x02 Prev mapped module
     *
     * Command: Select
     * [5]         0x03 Select mapped module
     * [6-x]      Module rack y position as a length byte + ascii string byte array,
     * [x+1-y]     Module rack x position as a length byte + ascii string byte array
     *
     * Command: List mapped modules
     * [5]         0x04 List mapped modules
     *
     * Command: Reset mapped parameter to its default
     * [5]         0x05 Reset Parameter
     * [6]         NPRN id MSB (0-127)
     * [7]         NPRN id LSB (0-127)
     *
     */
    bool parseE1SysEx(midi::Message msg) {
        if (msg.getSize() < 7)
            return false;
        // Check this is one of our SysEx messages from our E1 preset
        if (msg.bytes.at(1) != 0x00 || msg.bytes.at(2) != 0x7f || msg.bytes.at(3) != 0x7f)
            return false;

        switch(msg.bytes.at(4)) {
            // Command
            case 0x01: {
                switch(msg.bytes.at(5)) {
                    // Next
                    case 0x01: {
                        // INFO("Received an E1 Next Command");
                        e1ProcessNext = true;
                        return true;
                    }
                    // Prev
                    case 0x02: {
                        // INFO("Received an E1 Prev Command");
                        e1ProcessNext = false;
                        e1ProcessPrev = true;
                        return true;
                    }
                    // Module Select
                    case 0x03: {
                        // INFO("Received an E1 Module Select Command");
                        e1ProcessSelect = true;
                        // Convert bytes 7 to float
                        std::vector<uint8_t>::const_iterator vit = msg.bytes.begin() + 6;
                        float moduleY = floatFromSysEx(vit);
                        vit++;
                        float moduleX = floatFromSysEx(vit);
                        e1SelectedModulePos = Vec(moduleX, moduleY);
                        return true;
                    }
                    // List Mapped Modules
                    case 0x04: {
                        // INFO("Received an E1 List Mapped Modules Command");
                        e1ProcessListMappedModules = true;
                        return true;
                    }
                    // Reset parameter to its default
                    case 0x05: {
                        e1ProcessResetParameter = true;
                        e1ProcessResetParameterNPRN = ((int) msg.bytes.at(6) << 7) + ((int) msg.bytes.at(7));

                        // INFO("Received an E1 Reset Parameter Command for NPRN %d", e1ProcessResetParameterNPRN);
                        return true;
                    }
                    // Re-send MIDI feedback
                    case 0x06: {
                        // INFO("Received an E1 Re-send MIDI Feedback Command");
                        e1ProcessResendMIDIFeedback = true;
                        return true;
                    }
                    default: {
                        return false;
                    }
                }
            }
            default: {
                return false;
            }
        }

    }

    float floatFromSysEx(std::vector<uint8_t>::const_iterator & vit) {
        // Decode string length byte
        uint8_t strLen = *vit;

        // Check <= remaining midi message length
        std::string s;
        // Read string len bytes, convert to string
        for (int i = 0; i < strLen; ++i) {
            vit++;
            uint8_t b = *vit;
            char c = (char) b;
            s += c;
        }
        // Convert string to float (std::stdof)
        return std::stof(s);
    }

    /**
     * Builds a 14 bit controllerId and 14Bit Value from a series of received CC messages following the NPRN standard
     *
     * See http://www.philrees.co.uk/nrpnq.htm
     * [1]. CC 99 NPRN Parameter Order high order 7 bits (MSB)
     * [2]. CC 98 NPRN Parameter Order low order 7 bitss (LSB)
     * [3]. CC 6 Parameter Value high order 7 bits (MSB)
     * [4]. CC 38 Parameter Value low order 7 bits (LSB)
     * [5]. CC 101 Null (127)
     * [6]. CC 100 Null (127)
     */
    bool midiNPRN(midi::Message msg) {
        uint8_t cc = msg.getNote();
        uint8_t value = msg.getValue();
        switch (cc) {
            case 99:
                if (isPendingNPRN) {
                    // Still processing a previous NPRN 6-message sequence, so reset and start again
                    std::fill_n(nprnMsg.begin(), 4, 0);
                }
                nprnMsg[0] = value;
                isPendingNPRN = true;
                break;
            case 98:
                if (isPendingNPRN) nprnMsg[1] = value;
                break;
            case 6:
                if (isPendingNPRN) nprnMsg[2] = value;
                break;
            case 38:
                if (isPendingNPRN) nprnMsg[3] = value;
                break;
            case 101:
                // Ignore
                break;
            case 100:
                if (isPendingNPRN) {
                    int nprn = (nprnMsg[0] << 7) + nprnMsg[1];
                    int value = (nprnMsg[2] << 7) + nprnMsg[3];
                    isPendingNPRN = false;

                    // Learn
                    if (learningId >= 0 && learnedNprnLast != nprn && valuesNprn[nprn] != value) {                    
                        nprns[learningId].setNprn(nprn);
                        nprns[learningId].nprnMode = NPRNMODE::DIRECT;
                        nprns[learningId].set14bit(true);
                        learnedNprn = true;
                        learnedNprnLast = nprn;
                        commitLearn();
                        updateMapLen();
                        refreshParamHandleText(learningId);
                    }
                    bool midiReceived = valuesNprn[nprn] != value;
                    valuesNprn[nprn] = value;
                    valuesNprnTs[nprn] = ts;
                    return midiReceived;
                }

        }
        return false;
    }

	bool midiCc(midi::Message msg) {
		uint8_t cc = msg.getNote();
		if (cc == 99 && !isPendingNPRN) {
		    // Start of a new NPRN message sequence
		    return midiNPRN(msg);
		}
		return false;
	}

	void midiResendFeedback() {
		for (int i = 0; i < MAX_CHANNELS; i++) {
			lastValueOut[i] = -1;
			nprns[i].resetValue();
		}
	}

	void changeE1Module(const char* moduleName, float moduleY, float moduleX) {
	    // INFO("changeE1Module to %s", moduleName);
	    midiCtrlOutput.changeE1Module(moduleName, moduleY, moduleX);
	}

	void endChangeE1Module() {
	    // INFO("endChangeE1Module");
	    midiCtrlOutput.endChangeE1Module();
    }


	void clearMap(int id, bool midiOnly = false) {
		learningId = -1;
		nprns[id].reset();
		midiOptions[id] = 0;
		midiParam[id].reset();
		if (!midiOnly) {
			textLabel[id] = "";
			APP->engine->updateParamHandle(&paramHandles[id], -1, 0, true);
			updateMapLen();
			refreshParamHandleText(id);
		}
	}

	void clearMaps_WithLock() {
		learningId = -1;
		for (int id = 0; id < MAX_CHANNELS; id++) {
			nprns[id].reset();
			textLabel[id] = "";
			midiOptions[id] = 0;
			midiParam[id].reset();
			APP->engine->updateParamHandle(&paramHandles[id], -1, 0, true);
			refreshParamHandleText(id);
		}
		mapLen = 1;
		expMemModuleId = -1;
	}

	void clearMaps_NoLock() {
		learningId = -1;
		for (int id = 0; id < MAX_CHANNELS; id++) {
			nprns[id].reset();
			textLabel[id] = "";
			midiOptions[id] = 0;
			midiParam[id].reset();
			APP->engine->updateParamHandle_NoLock(&paramHandles[id], -1, 0, true);
			refreshParamHandleText(id);
		}
		mapLen = 1;
		expMemModuleId = -1;
	}

	void updateMapLen() {
		// Find last nonempty map
		int id;
		for (id = MAX_CHANNELS - 1; id >= 0; id--) {
			if (nprns[id].getNprn() >= 0 || paramHandles[id].moduleId >= 0)
				break;
		}
		mapLen = id + 1;
		// Add an empty "Mapping..." slot
		if (mapLen < MAX_CHANNELS) {
			mapLen++;
		}
	}

	void commitLearn() {
		if (learningId < 0)
			return;
		if (!learnedNprn)
			return;
		if (!learnedParam && paramHandles[learningId].moduleId < 0)
			return;
		// Reset learned state
		learnedNprn = false;
		learnedParam = false;
		// Copy modes from the previous slot
		if (learningId > 0) {
			nprns[learningId].nprnMode = nprns[learningId - 1].nprnMode;
			nprns[learningId].set14bit(true);
			midiOptions[learningId] = midiOptions[learningId - 1];
			midiParam[learningId].setSlew(midiParam[learningId - 1].getSlew());
			midiParam[learningId].setMin(midiParam[learningId - 1].getMin());
			midiParam[learningId].setMax(midiParam[learningId - 1].getMax());
			midiParam[learningId].clockMode = midiParam[learningId - 1].clockMode;
			midiParam[learningId].clockSource = midiParam[learningId - 1].clockSource;
		}
		textLabel[learningId] = "";

		// Find next incomplete map
		while (!learnSingleSlot && ++learningId < MAX_CHANNELS) {
			if (nprns[learningId].getNprn() < 0 || paramHandles[learningId].moduleId < 0)
				return;
		}
		learningId = -1;
	}

	int enableLearn(int id, bool learnSingle = false) {
		if (id == -1) {
			// Find next incomplete map
			while (++id < MAX_CHANNELS) {
				if (nprns[id].getNprn() < 0 && paramHandles[id].moduleId < 0)
					break;
			}
			if (id == MAX_CHANNELS) {
				return -1;
			}
		}

		if (id == mapLen) {
			disableLearn();
			return -1;
		}
		if (learningId != id) {
			learningId = id;
			learnedNprn = false;
			learnedNprnLast = -1;
			learnedParam = false;
			learnSingleSlot = learnSingle;
		}
		return id;
	}

	void disableLearn() {
		learningId = -1;
	}

	void disableLearn(int id) {
		if (learningId == id) {
			learningId = -1;
		}
	}

	void learnParam(int id, int64_t moduleId, int paramId, bool resetMidiSettings = true) {
		APP->engine->updateParamHandle(&paramHandles[id], moduleId, paramId, true);
		midiParam[id].reset(resetMidiSettings);
		learnedParam = true;
		commitLearn();
		updateMapLen();
	}

	void moduleBind(Module* m, bool keepCcAndNote) {
		if (!m) return;
		if (!keepCcAndNote) {
			clearMaps_WithLock();
		}
		else {
			// Clean up some additional mappings on the end
			for (int i = int(m->params.size()); i < mapLen; i++) {
				APP->engine->updateParamHandle(&paramHandles[i], -1, -1, true);
			}
		}
		for (size_t i = 0; i < m->params.size() && i < MAX_CHANNELS; i++) {
			learnParam(int(i), m->id, int(i));
		}

		updateMapLen();
	}

	void moduleBindExpander(bool keepCcAndNote) {
		Module::Expander* exp = &leftExpander;
		if (exp->moduleId < 0) return;
		Module* m = exp->module;
		if (!m) return;
		moduleBind(m, keepCcAndNote);
	}

	void refreshParamHandleText(int id) {
		std::string text = "ORESTES-ONE";
		if (nprns[id].getNprn() >= 0) {
        	text += string::f(" nprn%05d", nprns[id].getNprn());
        }
		paramHandles[id].text = text;
	}

	void expMemSave(std::string pluginSlug, std::string moduleSlug) {
		MemModule* m = new MemModule;
		Module* module = NULL;
		for (size_t i = 0; i < MAX_CHANNELS; i++) {
			if (paramHandles[i].moduleId < 0) continue;
			if (paramHandles[i].module->model->plugin->slug != pluginSlug && paramHandles[i].module->model->slug == moduleSlug) continue;
			module = paramHandles[i].module;
			MemParam* p = new MemParam;
			p->paramId = paramHandles[i].paramId;
			p->nprn = nprns[i].getNprn();
			p->nprnMode = nprns[i].nprnMode;
			p->label = textLabel[i];
			p->midiOptions = midiOptions[i];
			p->slew = midiParam[i].getSlew();
			p->min = midiParam[i].getMin();
			p->max = midiParam[i].getMax();
			m->paramMap.push_back(p);
		}
		m->pluginName = module->model->plugin->name;
		m->moduleName = module->model->name;

		auto p = std::pair<std::string, std::string>(pluginSlug, moduleSlug);
		auto it = midiMap.find(p);
		if (it != midiMap.end()) {
			delete it->second;
		}

		(midiMap)[p] = m;
	}

	void expMemDelete(std::string pluginSlug, std::string moduleSlug) {
		json_t* currentStateJ = toJson();

		auto p = std::pair<std::string, std::string>(pluginSlug, moduleSlug);
		auto it = midiMap.find(p);
		delete it->second;
		midiMap.erase(p);

		// history::ModuleChange
		history::ModuleChange* h = new history::ModuleChange;
		h->name = "delete module mappings";
		h->moduleId = this->id;
		h->oldModuleJ = currentStateJ;
		h->newModuleJ = toJson();
		APP->history->push(h);
		
	}

	/* Delete all mapped modules belonging to same plugin, add to undo history */
	void expMemPluginDelete(std::string pluginSlug) {
		json_t* currentStateJ = toJson();

		// From: https://stackoverflow.com/a/4600592
		auto itr = midiMap.begin();
		while (itr != midiMap.end()) {
		    if (itr->first.first == pluginSlug) {
		    	delete itr->second;
		       	itr = midiMap.erase(itr);
		    } else {
		       ++itr;
		    }
		}

		// history::ModuleChange
		history::ModuleChange* h = new history::ModuleChange;
		h->name = "delete plugin mappings";
		h->moduleId = this->id;
		h->oldModuleJ = currentStateJ;
		h->newModuleJ = toJson();
		APP->history->push(h);
		
	}

	void expMemApply(Module* m, math::Vec pos = Vec(0,0)) {
		if (!m) return;
		auto p = std::pair<std::string, std::string>(m->model->plugin->slug, m->model->slug);
		auto it = midiMap.find(p);
		if (it == midiMap.end()) return;
		MemModule* map = it->second;

        // Update module name on E1 page
		changeE1Module(m->model->getFullName().c_str(), pos.y, pos.x);

		clearMaps_WithLock();
		midiOutput.reset();
		midiCtrlOutput.reset();

		expMemModuleId = m->id;
		int i = 0;
		sendE1EndMessage = 1;
		for (MemParam* it : map->paramMap) {
			learnParam(i, m->id, it->paramId);
			nprns[i].setNprn(it->nprn);
			nprns[i].nprnMode = it->nprnMode;
			nprns[i].set14bit(true);
			textLabel[i] = it->label;
			midiOptions[i] = it->midiOptions;
			midiParam[i].setSlew(it->slew);
			midiParam[i].setMin(it->min);
			midiParam[i].setMax(it->max);
			// Force next processMappings() call to process all mappings after module controls have been switched
			lastValueOut[i] = -1;

			// If this parameter is mapped to a MIDI controller, increment the sendE1EndMessage counter so
			// the process() code can figure out when it has sent all mapped parameters for the module to the E1
		    if (nprns[i].getNprn() >= 0) sendE1EndMessage++;

			i++;
		}

		updateMapLen();

	}

	void expMemExportPlugin(std::string pluginSlug) {

		osdialog_filters* filters = osdialog_filters_parse(SAVE_JSON_FILTERS);
		DEFER({
			osdialog_filters_free(filters);
		});

		std::string filename = pluginSlug + ".json";
		char* path = osdialog_file(OSDIALOG_SAVE, "", filename.c_str(), filters);
		if (!path) {
			// No path selected
			return;
		}
		DEFER({
			free(path);
		});

		exportPluginMidiMap_action(path, pluginSlug);
	}

	void exportPluginMidiMap_action(char* path, std::string pluginSlug) {

		INFO("Exporting midimaps for plugin %s to file %s", pluginSlug.c_str(), path);

		json_t* rootJ = json_object();
		DEFER({
			json_decref(rootJ);
		});

		json_object_set_new(rootJ, "plugin", json_string(this->model->plugin->slug.c_str()));
		json_object_set_new(rootJ, "model", json_string(this->model->slug.c_str()));
		json_t* dataJ = json_object();

		// Get map of all mapped modules for this plugin
		std::map<std::pair<std::string, std::string>, MemModule*> pluginMap;

		pluginMap.clear();
		for (auto it : midiMap) {
			if (it.first.first == pluginSlug)
				pluginMap[it.first] = it.second;
		}

		json_t* midiMapJ = midiMapToJsonArray(pluginMap);

		json_object_set_new(dataJ, "midiMap", midiMapJ);
		json_object_set_new(rootJ, "data", dataJ);

		// Write to json file
		FILE* file = fopen(path, "w");		
		if (!file) {
			WARN("Could not create file %s", path);
			return;
		}
		DEFER({
			fclose(file);
		});

		if (json_dumpf(rootJ, file, JSON_INDENT(4)) < 0) {
			std::string message = string::f("File could not be written to %s", path);
			osdialog_message(OSDIALOG_ERROR, OSDIALOG_OK, message.c_str());
			return;
		}

	}

	bool expMemTest(Module* m) {
		if (!m) return false;
		auto p = std::pair<std::string, std::string>(m->model->plugin->slug, m->model->slug);
		auto it = midiMap.find(p);
		if (it == midiMap.end()) return false;
		return true;
	}


	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		json_object_set_new(rootJ, "textScrolling", json_boolean(textScrolling));
		json_object_set_new(rootJ, "mappingIndicatorHidden", json_boolean(mappingIndicatorHidden));
		json_object_set_new(rootJ, "locked", json_boolean(locked));
		json_object_set_new(rootJ, "processDivision", json_integer(processDivision));
		json_object_set_new(rootJ, "overlayEnabled", json_boolean(overlayEnabled));
		json_object_set_new(rootJ, "clearMapsOnLoad", json_boolean(clearMapsOnLoad));

		json_t* mapsJ = json_array();
		for (int id = 0; id < mapLen; id++) {
			json_t* mapJ = json_object();
			json_object_set_new(mapJ, "nprn", json_integer(nprns[id].getNprn()));
            json_object_set_new(mapJ, "nprnMode", json_integer((int)nprns[id].nprnMode));
			json_object_set_new(mapJ, "moduleId", json_integer(paramHandles[id].moduleId));
			json_object_set_new(mapJ, "paramId", json_integer(paramHandles[id].paramId));
			json_object_set_new(mapJ, "label", json_string(textLabel[id].c_str()));
			json_object_set_new(mapJ, "midiOptions", json_integer(midiOptions[id]));
			json_object_set_new(mapJ, "slew", json_real(midiParam[id].getSlew()));
			json_object_set_new(mapJ, "min", json_real(midiParam[id].getMin()));
			json_object_set_new(mapJ, "max", json_real(midiParam[id].getMax()));
			json_object_set_new(mapJ, "clockMode", json_integer((int)midiParam[id].clockMode));
			json_object_set_new(mapJ, "clockSource", json_integer(midiParam[id].clockSource));
			json_array_append_new(mapsJ, mapJ);
		}
		json_object_set_new(rootJ, "maps", mapsJ);

		json_object_set_new(rootJ, "midiResendPeriodically", json_boolean(midiResendPeriodically));
		json_object_set_new(rootJ, "midiIgnoreDevices", json_boolean(midiIgnoreDevices));
		json_object_set_new(rootJ, "midiInput", midiInput.toJson());
		json_object_set_new(rootJ, "midiOutput", midiOutput.toJson());
		json_object_set_new(rootJ, "midiCtrlInput", midiCtrlInput.toJson());
		json_object_set_new(rootJ, "midiCtrlOutput", midiCtrlOutput.toJson());

		json_t* midiMapJ = midiMapToJsonArray(midiMap);
		json_object_set_new(rootJ, "midiMap", midiMapJ);

		return rootJ;
	}

	json_t* midiMapToJsonArray(const std::map<std::pair<std::string, std::string>, MemModule*>& aMidiMap) {
		json_t* midiMapJ = json_array();
		for (auto it : aMidiMap) {
			json_t* midiMapJJ = json_object();
			json_object_set_new(midiMapJJ, "pluginSlug", json_string(it.first.first.c_str()));
			json_object_set_new(midiMapJJ, "moduleSlug", json_string(it.first.second.c_str()));

			auto a = it.second;
			json_object_set_new(midiMapJJ, "pluginName", json_string(a->pluginName.c_str()));
			json_object_set_new(midiMapJJ, "moduleName", json_string(a->moduleName.c_str()));
			json_t* paramMapJ = json_array();
			for (auto p : a->paramMap) {
				json_t* paramMapJJ = json_object();
				json_object_set_new(paramMapJJ, "paramId", json_integer(p->paramId));
				json_object_set_new(paramMapJJ, "nprn", json_integer(p->nprn));
				json_object_set_new(paramMapJJ, "nprnMode", json_integer((int)p->nprnMode));
				json_object_set_new(paramMapJJ, "label", json_string(p->label.c_str()));
				json_object_set_new(paramMapJJ, "midiOptions", json_integer(p->midiOptions));
				json_object_set_new(paramMapJJ, "slew", json_real(p->slew));
				json_object_set_new(paramMapJJ, "min", json_real(p->min));
				json_object_set_new(paramMapJJ, "max", json_real(p->max));
				json_array_append_new(paramMapJ, paramMapJJ);
			}
			json_object_set_new(midiMapJJ, "paramMap", paramMapJ);

			json_array_append_new(midiMapJ, midiMapJJ);
		}
		return midiMapJ;
	}


	void dataFromJson(json_t* rootJ) override {
		json_t* panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ) panelTheme = json_integer_value(panelThemeJ);

		json_t* textScrollingJ = json_object_get(rootJ, "textScrolling");
		if (textScrollingJ) textScrolling = json_boolean_value(textScrollingJ);
		json_t* mappingIndicatorHiddenJ = json_object_get(rootJ, "mappingIndicatorHidden");
		if (mappingIndicatorHiddenJ) mappingIndicatorHidden = json_boolean_value(mappingIndicatorHiddenJ);
		json_t* lockedJ = json_object_get(rootJ, "locked");
		if (lockedJ) locked = json_boolean_value(lockedJ);
		json_t* processDivisionJ = json_object_get(rootJ, "processDivision");
		if (processDivisionJ) setProcessDivision(json_integer_value(processDivisionJ));
		json_t* overlayEnabledJ = json_object_get(rootJ, "overlayEnabled");
		if (overlayEnabledJ) overlayEnabled = json_boolean_value(overlayEnabledJ);
		json_t* clearMapsOnLoadJ = json_object_get(rootJ, "clearMapsOnLoad");
		if (clearMapsOnLoadJ) clearMapsOnLoad = json_boolean_value(clearMapsOnLoadJ);

		if (clearMapsOnLoad) {
			// Use NoLock because we're already in an Engine write-lock.
			clearMaps_NoLock();
		}

		json_t* mapsJ = json_object_get(rootJ, "maps");
		if (mapsJ) {
			json_t* mapJ;
			size_t mapIndex;
			json_array_foreach(mapsJ, mapIndex, mapJ) {
				if (mapIndex >= MAX_CHANNELS) {
					continue;
				}

				json_t* nprnJ = json_object_get(mapJ, "nprn");
				json_t* nprnModeJ = json_object_get(mapJ, "nprnMode");
				json_t* moduleIdJ = json_object_get(mapJ, "moduleId");
				json_t* paramIdJ = json_object_get(mapJ, "paramId");
				json_t* labelJ = json_object_get(mapJ, "label");
				json_t* midiOptionsJ = json_object_get(mapJ, "midiOptions");
				json_t* slewJ = json_object_get(mapJ, "slew");
				json_t* minJ = json_object_get(mapJ, "min");
				json_t* maxJ = json_object_get(mapJ, "max");
				json_t* clockModeJ = json_object_get(mapJ, "clockMode");
				json_t* clockSourceJ = json_object_get(mapJ, "clockSource");

				if (!nprnJ) {
					nprns[mapIndex].setNprn(-1);
					APP->engine->updateParamHandle_NoLock(&paramHandles[mapIndex], -1, 0, true);
					continue;
				}
				if (!(moduleIdJ || paramIdJ)) {
					APP->engine->updateParamHandle_NoLock(&paramHandles[mapIndex], -1, 0, true);
				}

				nprns[mapIndex].setNprn(nprnJ ? json_integer_value(nprnJ) : -1);
				nprns[mapIndex].nprnMode = (NPRNMODE)json_integer_value(nprnModeJ);
				nprns[mapIndex].set14bit(true);
				midiOptions[mapIndex] = json_integer_value(midiOptionsJ);
				int64_t moduleId = moduleIdJ ? json_integer_value(moduleIdJ) : -1;
				int paramId = paramIdJ ? json_integer_value(paramIdJ) : 0;
				if (moduleId >= 0) {
					//moduleId = idFix(moduleId);
					if (moduleId != paramHandles[mapIndex].moduleId || paramId != paramHandles[mapIndex].paramId) {
						APP->engine->updateParamHandle_NoLock(&paramHandles[mapIndex], moduleId, paramId, false);
						refreshParamHandleText(mapIndex);
					}
				}
				if (labelJ) textLabel[mapIndex] = json_string_value(labelJ);
				if (slewJ) midiParam[mapIndex].setSlew(json_real_value(slewJ));
				if (minJ) midiParam[mapIndex].setMin(json_real_value(minJ));
				if (maxJ) midiParam[mapIndex].setMax(json_real_value(maxJ));
				if (clockModeJ) midiParam[mapIndex].clockMode = (OrestesOneParam::CLOCKMODE)json_integer_value(clockModeJ);
				if (clockSourceJ) midiParam[mapIndex].clockSource = json_integer_value(clockSourceJ);
			}
		}

		updateMapLen();
		//idFixClearMap();
		
		json_t* midiResendPeriodicallyJ = json_object_get(rootJ, "midiResendPeriodically");
		if (midiResendPeriodicallyJ) midiResendPeriodically = json_boolean_value(midiResendPeriodicallyJ);

		if (!midiIgnoreDevices) {
			json_t* midiIgnoreDevicesJ = json_object_get(rootJ, "midiIgnoreDevices");
			if (midiIgnoreDevicesJ)	midiIgnoreDevices = json_boolean_value(midiIgnoreDevicesJ);
			json_t* midiInputJ = json_object_get(rootJ, "midiInput");
			if (midiInputJ) midiInput.fromJson(midiInputJ);
			json_t* midiOutputJ = json_object_get(rootJ, "midiOutput");
			if (midiOutputJ) midiOutput.fromJson(midiOutputJ);
			json_t* midiCtrlInputJ = json_object_get(rootJ, "midiCtrlInput");
			if (midiCtrlInputJ) midiCtrlInput.fromJson(midiCtrlInputJ);
			json_t* midiCtrlOutputJ = json_object_get(rootJ, "midiCtrlOutput");
			if (midiCtrlOutputJ) midiCtrlOutput.fromJson(midiCtrlOutputJ);
		}


		resetMap();
		json_t* midiMapJ = json_object_get(rootJ, "midiMap");
		size_t i;
		json_t* midiMapJJ;
		json_array_foreach(midiMapJ, i, midiMapJJ) {
			std::string pluginSlug = json_string_value(json_object_get(midiMapJJ, "pluginSlug"));
			std::string moduleSlug = json_string_value(json_object_get(midiMapJJ, "moduleSlug"));

			MemModule* a = new MemModule;
			a->pluginName = json_string_value(json_object_get(midiMapJJ, "pluginName"));
			a->moduleName = json_string_value(json_object_get(midiMapJJ, "moduleName"));
			json_t* paramMapJ = json_object_get(midiMapJJ, "paramMap");
			size_t j;
			json_t* paramMapJJ;
			json_array_foreach(paramMapJ, j, paramMapJJ) {
				MemParam* p = new MemParam;
				p->paramId = json_integer_value(json_object_get(paramMapJJ, "paramId"));
				p->nprn = json_integer_value(json_object_get(paramMapJJ, "nprn"));
				p->nprnMode = (NPRNMODE)json_integer_value(json_object_get(paramMapJJ, "nprnMode"));
				p->label = json_string_value(json_object_get(paramMapJJ, "label"));
				p->midiOptions = json_integer_value(json_object_get(paramMapJJ, "midiOptions"));
				json_t* slewJ = json_object_get(paramMapJJ, "slew");
				if (slewJ) p->slew = json_real_value(slewJ);
				json_t* minJ = json_object_get(paramMapJJ, "min");
				if (minJ) p->min = json_real_value(minJ);
				json_t* maxJ = json_object_get(paramMapJJ, "max");
				if (maxJ) p->max = json_real_value(maxJ);
				a->paramMap.push_back(p);
			}
			midiMap[std::pair<std::string, std::string>(pluginSlug, moduleSlug)] = a;
		}
	}

	void setProcessDivision(int d) {
		processDivision = d;
		processDivider.setDivision(d);
		processDivider.reset();
	}

	void setMode(MIDIMODE midiMode) {
		if (this->midiMode == midiMode)
			return;
		this->midiMode = midiMode;
		switch (midiMode) {
			case MIDIMODE::MIDIMODE_LOCATE:
				for (int i = 0; i < MAX_CHANNELS; i++)
					lastValueInIndicate[i] = std::max(0, lastValueIn[i]);
				break;
			default:
				break;
		}
	}
};


struct SlewSlider : ui::Slider {
	struct SlewQuantity : Quantity {
		const float SLEW_MIN = 0.f;
		const float SLEW_MAX = 5.f;
		OrestesOneParam* p;
		void setValue(float value) override {
			value = clamp(value, SLEW_MIN, SLEW_MAX);
			p->setSlew(value);
		}
		float getValue() override {
			return p->getSlew();
		}
		float getDefaultValue() override {
			return 0.f;
		}
		std::string getLabel() override {
			return "Slew-limiting";
		}
		int getDisplayPrecision() override {
			return 2;
		}
		float getMaxValue() override {
			return SLEW_MAX;
		}
		float getMinValue() override {
			return SLEW_MIN;
		}
	}; // struct SlewQuantity

	SlewSlider(OrestesOneParam* p) {
		box.size.x = 220.0f;
		quantity = construct<SlewQuantity>(&SlewQuantity::p, p);
	}
	~SlewSlider() {
		delete quantity;
	}
}; // struct SlewSlider

struct ScalingInputLabel : MenuLabelEx {
	OrestesOneParam* p;
	void step() override {
		float min = std::min(p->getMin(), p->getMax());
		float max = std::max(p->getMin(), p->getMax());

		float g1 = rescale(0.f, min, max, p->limitMin, p->limitMax);
		g1 = clamp(g1, p->limitMin, p->limitMax);
		int g1a = std::round(g1);
		float g2 = rescale(1.f, min, max, p->limitMin, p->limitMax);
		g2 = clamp(g2, p->limitMin, p->limitMax);
		int g2a = std::round(g2);

		rightText = string::f("[%i, %i]", g1a, g2a);
	}
}; // struct ScalingInputLabel

struct ScalingOutputLabel : MenuLabelEx {
	OrestesOneParam* p;
	void step() override {
		float min = p->getMin();
		float max = p->getMax();

		float f1 = rescale(p->limitMin, p->limitMin, p->limitMax, min, max);
		f1 = clamp(f1, 0.f, 1.f) * 100.f;
		float f2 = rescale(p->limitMax, p->limitMin, p->limitMax, min, max);
		f2 = clamp(f2, 0.f, 1.f) * 100.f;

		rightText = string::f("[%.1f%%, %.1f%%]", f1, f2);
	}
}; // struct ScalingOutputLabel

struct MinSlider : SubMenuSlider {
	struct MinQuantity : Quantity {
		OrestesOneParam* p;
		void setValue(float value) override {
			value = clamp(value, -1.f, 2.f);
			p->setMin(value);
		}
		float getValue() override {
			return p->getMin();
		}
		float getDefaultValue() override {
			return 0.f;
		}
		float getMinValue() override {
			return -1.f;
		}
		float getMaxValue() override {
			return 2.f;
		}
		float getDisplayValue() override {
			return getValue() * 100;
		}
		void setDisplayValue(float displayValue) override {
			setValue(displayValue / 100);
		}
		std::string getLabel() override {
			return "Low";
		}
		std::string getUnit() override {
			return "%";
		}
		int getDisplayPrecision() override {
			return 3;
		}
	}; // struct MinQuantity

	MinSlider(OrestesOneParam* p) {
		box.size.x = 220.0f;
		quantity = construct<MinQuantity>(&MinQuantity::p, p);
	}
	~MinSlider() {
		delete quantity;
	}
}; // struct MinSlider

struct MaxSlider : SubMenuSlider {
	struct MaxQuantity : Quantity {
		OrestesOneParam* p;
		void setValue(float value) override {
			value = clamp(value, -1.f, 2.f);
			p->setMax(value);
		}
		float getValue() override {
			return p->getMax();
		}
		float getDefaultValue() override {
			return 1.f;
		}
		float getMinValue() override {
			return -1.f;
		}
		float getMaxValue() override {
			return 2.f;
		}
		float getDisplayValue() override {
			return getValue() * 100;
		}
		void setDisplayValue(float displayValue) override {
			setValue(displayValue / 100);
		}
		std::string getLabel() override {
			return "High";
		}
		std::string getUnit() override {
			return "%";
		}
		int getDisplayPrecision() override {
			return 3;
		}
	}; // struct MaxQuantity

	MaxSlider(OrestesOneParam* p) {
		box.size.x = 220.0f;
		quantity = construct<MaxQuantity>(&MaxQuantity::p, p);
	}
	~MaxSlider() {
		delete quantity;
	}
}; // struct MaxSlider


struct OrestesOneChoice : MapModuleChoice<MAX_CHANNELS, OrestesOneModule> {
	OrestesOneChoice() {
		textOffset = Vec(6.f, 14.7f);
		color = nvgRGB(0xf0, 0xf0, 0xf0);
	}

	std::string getSlotPrefix() override {

		if (module->nprns[id].getNprn() >= 0) {
             return string::f("nprn%05d ", module->nprns[id].getNprn());
        }
		else if (module->paramHandles[id].moduleId >= 0) {
			return ".... ";
		}
		else {
			return "";
		}
	}

	std::string getSlotLabel() override {
		return module->textLabel[id];
	}

	void appendContextMenu(Menu* menu) override {
		struct UnmapMidiItem : MenuItem {
			OrestesOneModule* module;
			int id;
			void onAction(const event::Action& e) override {
				module->clearMap(id, true);
			}
		}; // struct UnmapMidiItem

		struct NprnModeMenuItem : MenuItem {
			OrestesOneModule* module;
			int id;

			NprnModeMenuItem() {
				rightText = RIGHT_ARROW;
			}

			struct NprnModeItem : MenuItem {
				OrestesOneModule* module;
				int id;
				NPRNMODE nprnMode;

				void onAction(const event::Action& e) override {
					module->nprns[id].nprnMode = nprnMode;
				}
				void step() override {
					rightText = module->nprns[id].nprnMode == nprnMode ? "✔" : "";
					MenuItem::step();
				}
			};

			Menu* createChildMenu() override {
				Menu* menu = new Menu;
				menu->addChild(construct<NprnModeItem>(&MenuItem::text, "Direct", &NprnModeItem::module, module, &NprnModeItem::id, id, &NprnModeItem::nprnMode, NPRNMODE::DIRECT));
				menu->addChild(construct<NprnModeItem>(&MenuItem::text, "Pickup (snap)", &NprnModeItem::module, module, &NprnModeItem::id, id, &NprnModeItem::nprnMode, NPRNMODE::PICKUP1));
				reinterpret_cast<MenuItem*>(menu->children.back())->disabled = module->midiParam[id].clockMode != OrestesOneParam::CLOCKMODE::OFF;
				menu->addChild(construct<NprnModeItem>(&MenuItem::text, "Pickup (jump)", &NprnModeItem::module, module, &NprnModeItem::id, id, &NprnModeItem::nprnMode, NPRNMODE::PICKUP2));
				reinterpret_cast<MenuItem*>(menu->children.back())->disabled = module->midiParam[id].clockMode != OrestesOneParam::CLOCKMODE::OFF;
				menu->addChild(construct<NprnModeItem>(&MenuItem::text, "Toggle", &NprnModeItem::module, module, &NprnModeItem::id, id, &NprnModeItem::nprnMode, NPRNMODE::TOGGLE));
				menu->addChild(construct<NprnModeItem>(&MenuItem::text, "Toggle + Value", &NprnModeItem::module, module, &NprnModeItem::id, id, &NprnModeItem::nprnMode, NPRNMODE::TOGGLE_VALUE));
				return menu;
			}
		}; // struct NprnModeMenuItem

		if (module->nprns[id].getNprn() >= 0) {
			menu->addChild(construct<UnmapMidiItem>(&MenuItem::text, "Clear MIDI assignment", &UnmapMidiItem::module, module, &UnmapMidiItem::id, id));
		}
		if (module->nprns[id].getNprn() >= 0) {
			menu->addChild(new MenuSeparator());
			menu->addChild(construct<NprnModeMenuItem>(&MenuItem::text, "Input mode for NPRN", &NprnModeMenuItem::module, module, &NprnModeMenuItem::id, id));
		}

		struct PresetMenuItem : MenuItem {
			OrestesOneModule* module;
			int id;
			PresetMenuItem() {
				rightText = RIGHT_ARROW;
			}

			Menu* createChildMenu() override {
				struct PresetItem : MenuItem {
					OrestesOneParam* p;
					float min, max;
					void onAction(const event::Action& e) override {
						p->setMin(min);
						p->setMax(max);
					}
				};

				Menu* menu = new Menu;
				menu->addChild(construct<PresetItem>(&MenuItem::text, "Default", &PresetItem::p, &module->midiParam[id], &PresetItem::min, 0.f, &PresetItem::max, 1.f));
				menu->addChild(construct<PresetItem>(&MenuItem::text, "Inverted", &PresetItem::p, &module->midiParam[id], &PresetItem::min, 1.f, &PresetItem::max, 0.f));
				menu->addChild(construct<PresetItem>(&MenuItem::text, "Lower 50%", &PresetItem::p, &module->midiParam[id], &PresetItem::min, 0.f, &PresetItem::max, 0.5f));
				menu->addChild(construct<PresetItem>(&MenuItem::text, "Upper 50%", &PresetItem::p, &module->midiParam[id], &PresetItem::min, 0.5f, &PresetItem::max, 1.f));
				return menu;
			}
		}; // struct PresetMenuItem

		struct LabelMenuItem : MenuItem {
			OrestesOneModule* module;
			int id;

			LabelMenuItem() {
				rightText = RIGHT_ARROW;
			}

			struct LabelField : ui::TextField {
				OrestesOneModule* module;
				int id;
				void onSelectKey(const event::SelectKey& e) override {
					if (e.action == GLFW_PRESS && e.key == GLFW_KEY_ENTER) {
						module->textLabel[id] = text;

						ui::MenuOverlay* overlay = getAncestorOfType<ui::MenuOverlay>();
						overlay->requestDelete();
						e.consume(this);
					}

					if (!e.getTarget()) {
						ui::TextField::onSelectKey(e);
					}
				}
			};

			struct ResetItem : ui::MenuItem {
				OrestesOneModule* module;
				int id;
				void onAction(const event::Action& e) override {
					module->textLabel[id] = "";
				}
			};

			Menu* createChildMenu() override {
				Menu* menu = new Menu;

				LabelField* labelField = new LabelField;
				labelField->placeholder = "Label";
				labelField->text = module->textLabel[id];
				labelField->box.size.x = 180;
				labelField->module = module;
				labelField->id = id;
				menu->addChild(labelField);

				ResetItem* resetItem = new ResetItem;
				resetItem->text = "Reset";
				resetItem->module = module;
				resetItem->id = id;
				menu->addChild(resetItem);

				return menu;
			}
		}; // struct LabelMenuItem

		menu->addChild(new SlewSlider(&module->midiParam[id]));
		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Scaling"));
		std::string l = string::f("Input %s", module->nprns[id].getNprn() >= 0 ? "MIDI NPRN" : "");
		menu->addChild(construct<ScalingInputLabel>(&MenuLabel::text, l, &ScalingInputLabel::p, &module->midiParam[id]));
		menu->addChild(construct<ScalingOutputLabel>(&MenuLabel::text, "Parameter range", &ScalingOutputLabel::p, &module->midiParam[id]));
		menu->addChild(new MinSlider(&module->midiParam[id]));
		menu->addChild(new MaxSlider(&module->midiParam[id]));
		menu->addChild(construct<PresetMenuItem>(&MenuItem::text, "Presets", &PresetMenuItem::module, module, &PresetMenuItem::id, id));
		menu->addChild(new MenuSeparator());
		menu->addChild(construct<LabelMenuItem>(&MenuItem::text, "Custom label", &LabelMenuItem::module, module, &LabelMenuItem::id, id));

	}
};


struct OrestesOneDisplay : MapModuleDisplay<MAX_CHANNELS, OrestesOneModule, OrestesOneChoice>, OverlayMessageProvider {
	void step() override {
		if (module) {
			int mapLen = module->mapLen;
			for (int id = 0; id < MAX_CHANNELS; id++) {
				choices[id]->visible = (id < mapLen);
				separators[id]->visible = (id < mapLen);
			}
		}
		MapModuleDisplay<MAX_CHANNELS, OrestesOneModule, OrestesOneChoice>::step();
	}

	int nextOverlayMessageId() override {
		if (module->overlayQueue.empty())
			return -1;
		return module->overlayQueue.shift();
	}

	void getOverlayMessage(int id, Message& m) override {
		ParamQuantity* paramQuantity = choices[id]->getParamQuantity();
		if (!paramQuantity) return;

		std::string label = choices[id]->getSlotLabel();
		if (label == "") label = paramQuantity->name;

		m.title = paramQuantity->getDisplayValueString() + paramQuantity->getUnit();
		m.subtitle[0] = paramQuantity->module->model->name;
		m.subtitle[1] = label;
	}
};

struct MemDisplay : OrestesLedDisplay {
	OrestesOneModule* module;
	void step() override {
		OrestesLedDisplay::step();
		if (!module) return;
		text = string::f("%i", (int)module->midiMap.size());
	}
};

struct OrestesOneWidget : ThemedModuleWidget<OrestesOneModule>, ParamWidgetContextExtender {
	OrestesOneModule* module;
	OrestesOneDisplay* mapWidget;

	dsp::SchmittTrigger expMemPrevTrigger;
	dsp::SchmittTrigger expMemNextTrigger;
	dsp::SchmittTrigger expMemParamTrigger;

	enum class LEARN_MODE {
		OFF = 0,
		BIND_CLEAR = 1,
		BIND_KEEP = 2,
		MEM = 3
	};

	LEARN_MODE learnMode = LEARN_MODE::OFF;

	OrestesOneWidget(OrestesOneModule* module)
		: ThemedModuleWidget<OrestesOneModule>(module, "OrestesOne") {
		setModule(module);
		this->module = module;
		box.size.x = 300;

		addChild(createWidget<OrestesBlackScrew>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<OrestesBlackScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<OrestesBlackScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<OrestesBlackScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		MidiWidget<>* midiInputWidget = createWidget<MidiWidget<>>(Vec(10.0f, 36.4f));
		midiInputWidget->box.size = Vec(130.0f, 67.0f);
		midiInputWidget->setMidiPort(module ? &module->midiInput : NULL);
		addChild(midiInputWidget);

		MidiWidget<>* midiCtrlInputWidget = createWidget<MidiWidget<>>(Vec(160.0f, 36.4f));
		midiCtrlInputWidget->box.size = Vec(130.0f, 67.0f);
		midiCtrlInputWidget->setMidiPort(module ? &module->midiCtrlInput : NULL);
		addChild(midiCtrlInputWidget);

		MidiWidget<>* midiOutputWidget = createWidget<MidiWidget<>>(Vec(10.0f, 107.4f));
		midiOutputWidget->box.size = Vec(130.0f, 67.0f);
		midiOutputWidget->setMidiPort(module ? &module->midiOutput : NULL);
		addChild(midiOutputWidget);

		MidiWidget<>* midiCtrlOutputWidget = createWidget<MidiWidget<>>(Vec(160.0f, 107.4f));
		midiCtrlOutputWidget->box.size = Vec(130.0f, 67.0f);
		midiCtrlOutputWidget->setMidiPort(module ? &module->midiCtrlOutput : NULL);
		addChild(midiCtrlOutputWidget);

		mapWidget = createWidget<OrestesOneDisplay>(Vec(10.0f, 178.5f));
		mapWidget->box.size = Vec(280.0f, 164.7f);
		mapWidget->setModule(module);
		addChild(mapWidget);

		addChild(createParamCentered<TL1105>(Vec(40.0f, 360.0f), module, OrestesOneModule::PARAM_PREV));
		addChild(createParamCentered<TL1105>(Vec(80.0f, 360.0f), module, OrestesOneModule::PARAM_NEXT));
		addChild(createLightCentered<TinyLight<WhiteLight>>(Vec(190.f, 360.f), module, OrestesOneModule::LIGHT_APPLY));
		addChild(createParamCentered<TL1105>(Vec(210.0f, 360.f), module, OrestesOneModule::PARAM_APPLY));
		MemDisplay* memDisplay = createWidgetCentered<MemDisplay>(Vec(260.0f, 360.f));
		memDisplay->module = module;
		addChild(memDisplay);

		if (module) {
			OverlayMessageWidget::registerProvider(mapWidget);
		}
	}

	~OrestesOneWidget() {
		if (learnMode != LEARN_MODE::OFF) {
			glfwSetCursor(APP->window->win, NULL);
		}

		if (module) {
			OverlayMessageWidget::unregisterProvider(mapWidget);
		}
	}


	void step() override {
		ThemedModuleWidget<OrestesOneModule>::step();
		if (module) {
			// MEM
			if (module->e1ProcessPrev || expMemPrevTrigger.process(module->params[OrestesOneModule::PARAM_PREV].getValue())) {
			    module->e1ProcessPrev = false;
				expMemPrevModule();
			}
			if (module->e1ProcessNext || expMemNextTrigger.process(module->params[OrestesOneModule::PARAM_NEXT].getValue())) {
			    module->e1ProcessNext = false;
				expMemNextModule();
			}
			if (module->e1ProcessSelect) {
			    module->e1ProcessSelect = false;
			    expMemSelectModule(module->e1SelectedModulePos);
			    module->e1SelectedModulePos = Vec(0,0);
			}
			if (expMemParamTrigger.process(module->params[OrestesOneModule::PARAM_APPLY].getValue())) {
				enableLearn(LEARN_MODE::MEM);
			}
			module->lights[0].setBrightness(learnMode == LEARN_MODE::MEM);
		
		}

		ParamWidgetContextExtender::step();

	}

	void expMemPrevModule() {
		std::list<Widget*> modules = APP->scene->rack->getModuleContainer()->children;
		auto sort = [&](Widget* w1, Widget* w2) {
			auto t1 = std::make_tuple(w1->box.pos.y, w1->box.pos.x);
			auto t2 = std::make_tuple(w2->box.pos.y, w2->box.pos.x);
			return t1 > t2;
		};
		modules.sort(sort);
		expMemScanModules(modules);
	}

	void expMemNextModule() {
		std::list<Widget*> modules = APP->scene->rack->getModuleContainer()->children;
		auto sort = [&](Widget* w1, Widget* w2) {
			auto t1 = std::make_tuple(w1->box.pos.y, w1->box.pos.x);
			auto t2 = std::make_tuple(w2->box.pos.y, w2->box.pos.x);
			return t1 < t2;
		};
		modules.sort(sort);
		expMemScanModules(modules);
	}

	void expMemSelectModule(math::Vec modulePos) {
		// Build mapped module list to scan through to
		std::list<Widget*> modules = APP->scene->rack->getModuleContainer()->children;
		auto sort = [&](Widget* w1, Widget* w2) {
			auto t1 = std::make_tuple(w1->box.pos.y, w1->box.pos.x);
			auto t2 = std::make_tuple(w2->box.pos.y, w2->box.pos.x);
			return t1 < t2;
		};
		modules.sort(sort);
		std::list<Widget*>::iterator it = modules.begin();

		// Scan over all rack modules, find module at given rack position
		for (; it != modules.end(); it++) {
			ModuleWidget* mw = dynamic_cast<ModuleWidget*>(*it);
			if (modulePos.equals(mw->box.pos)) {
				Module* m = mw->module;
			    if (module->expMemTest(m)) {
			  	    // If module at matched position is mapped in extMem, we can safely apply its current mappings
				    module->expMemApply(m, mw->box.pos);
				}
			    return;
			}
		}
	}

	void expMemScanModules(std::list<Widget*>& modules) {
		f:
		std::list<Widget*>::iterator it = modules.begin();
		// Scan for current module in the list
		if (module->expMemModuleId != -1) {
			for (; it != modules.end(); it++) {
				ModuleWidget* mw = dynamic_cast<ModuleWidget*>(*it);
				Module* m = mw->module;
				if (m->id == module->expMemModuleId) {
					it++;
					break;
				}
			}
			// Module not found
			if (it == modules.end()) {
				it = modules.begin();
			}
		}
		// Scan for next module with stored mapping
		for (; it != modules.end(); it++) {
			ModuleWidget* mw = dynamic_cast<ModuleWidget*>(*it);
			Module* m = mw->module;
			if (module->expMemTest(m)) {
				module->expMemApply(m, mw->box.pos);
				return;
			}
		}
		// No module found yet -> retry from the beginning
		if (module->expMemModuleId != -1) {
			module->expMemModuleId = -1;
			goto f;
		}
	}

	void loadMidiMapPreset_dialog() {
		osdialog_filters* filters = osdialog_filters_parse(LOAD_MIDIMAP_FILTERS);
		DEFER({
			osdialog_filters_free(filters);
		});

		char* path = osdialog_file(OSDIALOG_OPEN, "", NULL, filters);
		if (!path) {
			// No path selected
			return;
		}
		DEFER({
			free(path);
		});

		loadMidiMapPreset_action(path);
	}

	void loadMidiMapPreset_action(std::string filename) {
		INFO("Merging mappings from file %s", filename.c_str());

		FILE* file = fopen(filename.c_str(), "r");
		if (!file) {
			WARN("Could not load file %s", filename.c_str());
			return;
		}
		DEFER({
			fclose(file);
		});

		json_error_t error;
		json_t* moduleJ = json_loadf(file, 0, &error);
		if (!moduleJ) {
			std::string message = string::f("File is not a valid JSON file. Parsing error at %s %d:%d %s", error.source, error.line, error.column, error.text);
			osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, message.c_str());
			return;
		}
		DEFER({
			json_decref(moduleJ);
		});

		json_t* currentStateJ = toJson();
		if (!mergeMidiMapPreset_convert(moduleJ, currentStateJ))
			return;

		// history::ModuleChange
		history::ModuleChange* h = new history::ModuleChange;
		h->name = "merge mappings";
		h->moduleId = module->id;
		h->oldModuleJ = toJson();

		module->fromJson(currentStateJ);

		h->newModuleJ = toJson();
		APP->history->push(h);
	}

	/**
	 * Merge module midiMap entries from the importedPresetJ into the current midiMap of this OrestesOne module.
	 * Entries from the imported presetJ will overwrite matching entries in the existing midiMap.
	 * 
	 * Mutates the currentStateJ with the amended midiMap list = current midiMap + merged midiMap from importedPresetJ
	 */
	bool mergeMidiMapPreset_convert(json_t* importedPresetJ, json_t* currentStateJ) {
		std::string pluginSlug = json_string_value(json_object_get(importedPresetJ, "plugin"));
		std::string modelSlug = json_string_value(json_object_get(importedPresetJ, "model"));

		// Only handle presets or midimap JSON files for OrestesOne
		if (!(pluginSlug == module->model->plugin->slug && modelSlug == module->model->slug))
			return false;

		// Get the midiMap in the imported preset Json
		json_t* dataJ = json_object_get(importedPresetJ, "data");
		json_t* midiMapJ = json_object_get(dataJ, "midiMap");

		json_t* currentStateDataJ = json_object_get(currentStateJ, "data");
		json_t* currentStateMidiMapJ = json_object_get(currentStateDataJ, "midiMap");

		// Loop over the midiMap from the imported preset JSON
		size_t i;
		json_t* midiMapJJ;
		json_array_foreach(midiMapJ, i, midiMapJJ) {
			std::string importedPluginSlug = json_string_value(json_object_get(midiMapJJ, "pluginSlug"));
			std::string importedModuleSlug = json_string_value(json_object_get(midiMapJJ, "moduleSlug"));

			// Find this mapped module in the current Orestes module state Json
			size_t ii;
			json_t* currentStateMidiMapJJ;
			bool foundCurrent = false;
			json_array_foreach(currentStateMidiMapJ, ii, currentStateMidiMapJJ) {
				std::string currentPluginSlug = json_string_value(json_object_get(currentStateMidiMapJJ, "pluginSlug"));
				std::string currentModuleSlug = json_string_value(json_object_get(currentStateMidiMapJJ, "moduleSlug"));

				if (currentPluginSlug == importedPluginSlug && currentModuleSlug == importedModuleSlug) {
						foundCurrent = true;
						// Replace current midi map with the imported one
						json_array_set(currentStateMidiMapJ, ii, midiMapJJ);
				}

			}
			if (foundCurrent == false) {
				// Did not find the imported module in the current midi map, so append it now
				json_t* clonedMidiMapJJ = json_deep_copy(midiMapJJ);
				json_array_append(currentStateMidiMapJ, clonedMidiMapJJ);
				json_decref(clonedMidiMapJJ);
			}

		};

		// currentStateJ* now has the updated merged midimap
		// INFO("Merged %zu mappings" , i);
		return true;
	}

	void extendParamWidgetContextMenu(ParamWidget* pw, Menu* menu) override {
		if (!module) return;
		if (module->learningId >= 0) return;
		ParamQuantity* pq = pw->getParamQuantity();
		if (!pq) return;
		
		struct OrestesOneBeginItem : MenuLabel {
			OrestesOneBeginItem() {
				text = "ORESTES-ONE";
			}
		};

		struct OrestesOneEndItem : MenuEntry {
			OrestesOneEndItem() {
				box.size = Vec();
			}
		};

		struct MapMenuItem : MenuItem {
			OrestesOneModule* module;
			ParamQuantity* pq;
			int currentId = -1;

			MapMenuItem() {
				rightText = RIGHT_ARROW;
			}

			Menu* createChildMenu() override {
				struct MapItem : MenuItem {
					OrestesOneModule* module;
					int currentId;
					void onAction(const event::Action& e) override {
						module->enableLearn(currentId, true);
					}
				};

				struct MapEmptyItem : MenuItem {
					OrestesOneModule* module;
					ParamQuantity* pq;
					void onAction(const event::Action& e) override {
						int id = module->enableLearn(-1, true);
						if (id >= 0) module->learnParam(id, pq->module->id, pq->paramId);
					}
				};

				struct RemapItem : MenuItem {
					OrestesOneModule* module;
					ParamQuantity* pq;
					int id;
					int currentId;
					void onAction(const event::Action& e) override {
						module->learnParam(id, pq->module->id, pq->paramId, false);
					}
					void step() override {
						rightText = CHECKMARK(id == currentId);
						MenuItem::step();
					}
				};

				Menu* menu = new Menu;
				if (currentId < 0) {
					menu->addChild(construct<MapEmptyItem>(&MenuItem::text, "Learn MIDI", &MapEmptyItem::module, module, &MapEmptyItem::pq, pq));
				}
				else {
					menu->addChild(construct<MapItem>(&MenuItem::text, "Learn MIDI", &MapItem::module, module, &MapItem::currentId, currentId));
				}

				if (module->mapLen > 0) {
					menu->addChild(new MenuSeparator);
					for (int i = 0; i < module->mapLen; i++) {
						if (module->nprns[i].getNprn() >= 0) {
							std::string text;
							if (module->textLabel[i] != "") {
								text = module->textLabel[i];
							}
							else if (module->nprns[i].getNprn() >= 0) {
                            	text = string::f("MIDI NPRN %05d", module->nprns[i].getNprn());
                            }
							menu->addChild(construct<RemapItem>(&MenuItem::text, text, &RemapItem::module, module, &RemapItem::pq, pq, &RemapItem::id, i, &RemapItem::currentId, currentId));
						}
					}
				}
				return menu;
			} 
		};

		std::list<Widget*>::iterator beg = menu->children.begin();
		std::list<Widget*>::iterator end = menu->children.end();
		std::list<Widget*>::iterator itCvBegin = end;
		std::list<Widget*>::iterator itCvEnd = end;
		
		for (auto it = beg; it != end; it++) {
			if (itCvBegin == end) {
				OrestesOneBeginItem* ml = dynamic_cast<OrestesOneBeginItem*>(*it);
				if (ml) { itCvBegin = it; continue; }
			}
			else {
				OrestesOneEndItem* ml = dynamic_cast<OrestesOneEndItem*>(*it);
				if (ml) { itCvEnd = it; break; }
			}
		}

		for (int id = 0; id < module->mapLen; id++) {
			if (module->paramHandles[id].moduleId == pq->module->id && module->paramHandles[id].paramId == pq->paramId) {
				std::string midiCatId = "";
				std::list<Widget*> w;
				w.push_back(construct<MapMenuItem>(&MenuItem::text, string::f("Re-map %s", midiCatId.c_str()), &MapMenuItem::module, module, &MapMenuItem::pq, pq, &MapMenuItem::currentId, id));
				w.push_back(new SlewSlider(&module->midiParam[id]));
				w.push_back(construct<MenuLabel>(&MenuLabel::text, "Scaling"));
				std::string l = string::f("Input %s", module->nprns[id].getNprn() >= 0 ? "MIDI NPRN" : "");
				w.push_back(construct<ScalingInputLabel>(&MenuLabel::text, l, &ScalingInputLabel::p, &module->midiParam[id]));
				w.push_back(construct<ScalingOutputLabel>(&MenuLabel::text, "Parameter range", &ScalingOutputLabel::p, &module->midiParam[id]));
				w.push_back(new MinSlider(&module->midiParam[id]));
				w.push_back(new MaxSlider(&module->midiParam[id]));
				w.push_back(construct<CenterModuleItem>(&MenuItem::text, "Go to mapping module", &CenterModuleItem::mw, this));
				w.push_back(new OrestesOneEndItem);

				if (itCvBegin == end) {
					menu->addChild(new MenuSeparator);
					menu->addChild(construct<OrestesOneBeginItem>());
					for (Widget* wm : w) {
						menu->addChild(wm);
					}
				}
				else {
					for (auto i = w.rbegin(); i != w.rend(); ++i) {
						Widget* wm = *i;
						menu->addChild(wm);
						auto it = std::prev(menu->children.end());
						menu->children.splice(std::next(itCvBegin), menu->children, it);
					}
				}
				return;
			}
		}

	}

	void onDeselect(const event::Deselect& e) override {
		ModuleWidget::onDeselect(e);
		if (learnMode != LEARN_MODE::OFF) {
			DEFER({
				disableLearn();
			});

			// Learn module
			Widget* w = APP->event->getDraggedWidget();
			if (!w) return;
			ModuleWidget* mw = dynamic_cast<ModuleWidget*>(w);
			if (!mw) mw = w->getAncestorOfType<ModuleWidget>();
			if (!mw || mw == this) return;
			Module* m = mw->module;
			if (!m) return;

			OrestesOneModule* module = dynamic_cast<OrestesOneModule*>(this->module);
			switch (learnMode) {
				case LEARN_MODE::BIND_CLEAR:
					module->moduleBind(m, false); break;
				case LEARN_MODE::BIND_KEEP:
					module->moduleBind(m, true); break;
				case LEARN_MODE::MEM:
					module->expMemApply(m, mw->box.pos); break;
				case LEARN_MODE::OFF:
					break;
			}
		}
	}

	void onHoverKey(const event::HoverKey& e) override {
		if (e.action == GLFW_PRESS) {
			switch (e.key) {
				case GLFW_KEY_D: {
					if ((e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT) {
						enableLearn(LEARN_MODE::BIND_KEEP);
					}
					if ((e.mods & RACK_MOD_MASK) == (GLFW_MOD_SHIFT | RACK_MOD_CTRL)) {
						enableLearn(LEARN_MODE::BIND_CLEAR);
					}
					break;
				}
				case GLFW_KEY_E: {
					if ((e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT) {
						OrestesOneModule* module = dynamic_cast<OrestesOneModule*>(this->module);
						module->moduleBindExpander(true);
					}
					if ((e.mods & RACK_MOD_MASK) == (GLFW_MOD_SHIFT | RACK_MOD_CTRL)) {
						OrestesOneModule* module = dynamic_cast<OrestesOneModule*>(this->module);
						module->moduleBindExpander(false);
					}
					break;
				}
				case GLFW_KEY_V: {
					if ((e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT) {
						enableLearn(LEARN_MODE::MEM);
					}
					break;
				}
				case GLFW_KEY_ESCAPE: {
					OrestesOneModule* module = dynamic_cast<OrestesOneModule*>(this->module);
					disableLearn();
					module->disableLearn();
					e.consume(this);
					break;
				}
				case GLFW_KEY_SPACE: {
					if (module->learningId >= 0) {
						OrestesOneModule* module = dynamic_cast<OrestesOneModule*>(this->module);
						module->enableLearn(module->learningId + 1);
						if (module->learningId == -1) disableLearn();
						e.consume(this);
					}
					break;
				}
			}
		}
		ThemedModuleWidget<OrestesOneModule>::onHoverKey(e);
	}

	void enableLearn(LEARN_MODE mode) {
		learnMode = learnMode == LEARN_MODE::OFF ? mode : LEARN_MODE::OFF;
		APP->event->setSelectedWidget(this);
		GLFWcursor* cursor = NULL;
		if (learnMode != LEARN_MODE::OFF) {
			cursor = glfwCreateStandardCursor(GLFW_CROSSHAIR_CURSOR);
		}
		glfwSetCursor(APP->window->win, cursor);
	}

	void disableLearn() {
		learnMode = LEARN_MODE::OFF;
		glfwSetCursor(APP->window->win, NULL);
	}

	void appendContextMenu(Menu* menu) override {
		ThemedModuleWidget<OrestesOneModule>::appendContextMenu(menu);
		int sampleRate = int(APP->engine->getSampleRate());

		menu->addChild(new MenuSeparator());
		menu->addChild(createSubmenuItem("Preset load", "",
			[=](Menu* menu) {
				menu->addChild(createBoolPtrMenuItem("Ignore MIDI devices", "", &module->midiIgnoreDevices));
				menu->addChild(createBoolPtrMenuItem("Clear mapping slots", "", &module->clearMapsOnLoad));
			}
		));
		menu->addChild(Orestes::Rack::createMapSubmenuItem<int>("Precision", {
				{ 1, string::f("Samplerate (%i Hz)", sampleRate / 1) },
				{ 8, string::f("High (%i Hz)", sampleRate / 8) },
				{ 64, string::f("Moderate (%i Hz)", sampleRate / 64) },
				{ 256, string::f("Low (%i Hz)", sampleRate / 256) },
				{ 512, string::f("Rock bottom (%i Hz)", sampleRate / 512) }
			},
			[=]() {
				return module->processDivision;
			},
			[=](int division) {
				module->setProcessDivision(division);
			}
		));
		menu->addChild(Orestes::Rack::createMapSubmenuItem<MIDIMODE>("Mode", {
				{ MIDIMODE::MIDIMODE_DEFAULT, "Operating" },
				{ MIDIMODE::MIDIMODE_LOCATE, "Locate and indicate" }
			},
			[=]() {
				return module->midiMode;
			},
			[=](MIDIMODE midiMode) {
				module->setMode(midiMode);
			}
		));
		menu->addChild(createSubmenuItem("Re-send MIDI feedback", "",
			[=](Menu* menu) {
				menu->addChild(createMenuItem("Now", "", [=]() { module->midiResendFeedback(); }));
				menu->addChild(createBoolPtrMenuItem("Periodically", "", &module->midiResendPeriodically));
			}
		));
	
		menu->addChild(new MenuSeparator());
		menu->addChild(createSubmenuItem("User interface", "",
			[=](Menu* menu) {
				menu->addChild(createBoolPtrMenuItem("Text scrolling", "", &module->textScrolling));
				menu->addChild(createBoolPtrMenuItem("Hide mapping indicators", "", &module->mappingIndicatorHidden));
				menu->addChild(createBoolPtrMenuItem("Lock mapping slots", "", &module->locked));
			}
		));
		menu->addChild(createBoolPtrMenuItem("Status overlay", "", &module->overlayEnabled));
		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuItem("Clear mappings", "", [=]() { module->clearMaps_WithLock(); }));
		menu->addChild(createSubmenuItem("Map module (left)", "",
			[=](Menu* menu) {
				menu->addChild(createMenuItem("Clear first", RACK_MOD_CTRL_NAME "+" RACK_MOD_SHIFT_NAME "+E", [=]() { module->moduleBindExpander(false); }));
				menu->addChild(createMenuItem("Keep MIDI assignments", RACK_MOD_SHIFT_NAME "+E", [=]() { module->moduleBindExpander(true); }));
			}
		));
		menu->addChild(createSubmenuItem("Map module (select)", "",
			[=](Menu* menu) {
				menu->addChild(createMenuItem("Clear first", RACK_MOD_CTRL_NAME "+" RACK_MOD_SHIFT_NAME "+D", [=]() { enableLearn(LEARN_MODE::BIND_CLEAR); }));
				menu->addChild(createMenuItem("Keep MIDI assignments", RACK_MOD_SHIFT_NAME "+D", [=]() { enableLearn(LEARN_MODE::BIND_KEEP); }));
			}
		));

		appendContextMenuMem(menu);
	}

	void appendContextMenuMem(Menu* menu) {
		OrestesOneModule* module = dynamic_cast<OrestesOneModule*>(this->module);
		assert(module);

		struct MapMenuItem : MenuItem {
			OrestesOneModule* module;
			MapMenuItem() {
				rightText = RIGHT_ARROW;
			}

			Menu* createChildMenu() override {
				struct MidimapModuleItem : MenuItem {
					OrestesOneModule* module;
					std::string pluginSlug;
					std::string moduleSlug;
					MemModule* midimapModule;
					MidimapModuleItem() {
						rightText = RIGHT_ARROW;
					}
					Menu* createChildMenu() override {
						struct DeleteItem : MenuItem {
							OrestesOneModule* module;
							std::string pluginSlug;
							std::string moduleSlug;
							void onAction(const event::Action& e) override {
								module->expMemDelete(pluginSlug, moduleSlug);
							}
						}; // DeleteItem

						Menu* menu = new Menu;
						menu->addChild(construct<DeleteItem>(&MenuItem::text, "Delete", &DeleteItem::module, module, &DeleteItem::pluginSlug, pluginSlug, &DeleteItem::moduleSlug, moduleSlug));
						return menu;
					}
				}; // MidimapModuleItem

				struct MidimapPluginItem: MenuItem {
					OrestesOneModule* module;
					std::string pluginSlug;
					MidimapPluginItem() {
						rightText = RIGHT_ARROW;
					}
					// Create child menu listing all modules in this plugin
					Menu* createChildMenu() override {
						struct DeletePluginItem : MenuItem {
							OrestesOneModule* module;
							std::string pluginSlug;
							void onAction(const event::Action& e) override {
								module->expMemPluginDelete(pluginSlug);
							}
						}; // DeletePluginItem

						struct ExportPluginItem : MenuItem {
							OrestesOneModule* module;
							std::string pluginSlug;
							void onAction(const event::Action& e) override {
								module->expMemExportPlugin(pluginSlug);
							}
						}; // ExportPluginItem


						std::list<std::pair<std::string, MidimapModuleItem*>> l; 
						for (auto it : module->midiMap) {
							if (it.first.first == pluginSlug) {
								MemModule* a = it.second;
								MidimapModuleItem* midimapModuleItem = new MidimapModuleItem;
								midimapModuleItem->text = string::f("%s", a->moduleName.c_str());
								midimapModuleItem->module = module;
								midimapModuleItem->midimapModule = a;
								midimapModuleItem->pluginSlug = it.first.first;
								midimapModuleItem->moduleSlug = it.first.second;
								l.push_back(std::pair<std::string, MidimapModuleItem*>(midimapModuleItem->text, midimapModuleItem));
							}
						}

						l.sort();
						Menu* menu = new Menu;
						menu->addChild(construct<DeletePluginItem>(&MenuItem::text, "Delete all", &DeletePluginItem::module, module, &DeletePluginItem::pluginSlug, pluginSlug));
						menu->addChild(construct<ExportPluginItem>(&MenuItem::text, "Export all...", &ExportPluginItem::module, module, &ExportPluginItem::pluginSlug, pluginSlug));
						menu->addChild(new MenuSeparator());
						for (auto it : l) {
							menu->addChild(it.second);
						}
						return menu;

					}
				}; // MidimapPluginItem

				std::map<std::string, MidimapPluginItem*> l;
				l.clear();

				for (auto it : module->midiMap) {
					MemModule* a = it.second;
					if (l.find(it.first.first) == l.end()) {
						// Map does not already have an entry for this plugin, so add one now
						MidimapPluginItem* midimapPluginItem = new MidimapPluginItem;
						midimapPluginItem->text = string::f("%s", a->pluginName.c_str());
						midimapPluginItem->module = module;
						midimapPluginItem->pluginSlug = it.first.first;
						l[it.first.first] = midimapPluginItem;	
					}
				}
				Menu* menu = new Menu;
				for (auto it : l) {
					menu->addChild(it.second);
				}
				return menu;
			}
		}; // MapMenuItem

		struct SaveMenuItem : MenuItem {
			OrestesOneModule* module;
			SaveMenuItem() {
				rightText = RIGHT_ARROW;
			}

			Menu* createChildMenu() override {
				struct SaveItem : MenuItem {
					OrestesOneModule* module;
					std::string pluginSlug;
					std::string moduleSlug;
					void onAction(const event::Action& e) override {
						module->expMemSave(pluginSlug, moduleSlug);
					}
				}; // SaveItem

				typedef std::pair<std::string, std::string> ppair;
				std::list<std::pair<std::string, ppair>> list;
				std::set<ppair> s;
				for (size_t i = 0; i < MAX_CHANNELS; i++) {
					int64_t moduleId = module->paramHandles[i].moduleId;
					if (moduleId < 0) continue;
					Module* m = module->paramHandles[i].module;
					auto q = ppair(m->model->plugin->slug, m->model->slug);
					if (s.find(q) != s.end()) continue;
					s.insert(q);

					if (!m) continue;
					std::string l = string::f("%s %s", m->model->plugin->name.c_str(), m->model->name.c_str());
					auto p = std::pair<std::string, ppair>(l, q);
					list.push_back(p);
				}
				list.sort();

				Menu* menu = new Menu;
				for (auto it : list) {
					menu->addChild(construct<SaveItem>(&MenuItem::text, it.first, &SaveItem::module, module, &SaveItem::pluginSlug, it.second.first, &SaveItem::moduleSlug, it.second.second));
				}
				return menu;
			}
		}; // SaveMenuItem

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("MEM"));
		menu->addChild(construct<SaveMenuItem>(&MenuItem::text, "Store mapping", &SaveMenuItem::module, module));
		menu->addChild(createMenuItem("Apply mapping", RACK_MOD_SHIFT_NAME "+V", [=]() { enableLearn(LEARN_MODE::MEM); }));
		menu->addChild(construct<MapMenuItem>(&MenuItem::text, "Manage mappings", &MapMenuItem::module, module));
		menu->addChild(createMenuItem("Merge mappings from...", "", [=]() { loadMidiMapPreset_dialog(); }));
	}
};

} // namespace OrestesOne
} // namespace Orestes

Model* modelOrestesOne = createModel<Orestes::OrestesOne::OrestesOneModule, Orestes::OrestesOne::OrestesOneWidget>("OrestesOne");
#include "plugin.hpp"
#include "OrestesOne.hpp"
#include "MapModuleBase.hpp"
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

namespace RSBATechModules {
namespace OrestesOne {

struct OrestesOneOutput : midi::Output {

    void sendCCMsg(int cc, int value) {
        midi::Message m;
        m.setStatus(0xb);
        m.setNote(cc);
        m.setValue(value);
        sendMessage(m);
    }

};


struct E1MidiOutput : OrestesOneOutput {
	std::array<int, MAX_CHANNELS> lastNPRNValues{};
    midi::Message m;

    E1MidiOutput() {
		reset();
		m.bytes.reserve(512);
	}

	void reset() {
		std::fill_n(lastNPRNValues.begin(), MAX_CHANNELS, -1);
    	m.bytes.clear();
	}

    void sendE1ControlUpdate(int id, const char* name, const char* displayValue) {
        // See https://docs.electra.one/developers/midiimplementation.html#control-update

        // E1 VCVRack preset controls NPRM parameter numbers are offset by 2 from their preset controller Id
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
        json_object_set_new(valueJ, "visible", json_boolean(true));
        json_t* rootJ = json_object();
        json_object_set_new(rootJ, "name", json_string(name));
        json_object_set_new(rootJ, "value", valueJ);
        char* json = json_dumps(rootJ, JSON_COMPACT | JSON_ENSURE_ASCII);

        for( char* it = json; *it; ++it )
          m.bytes.push_back((uint8_t)*it);

        // SysEx closing byte
        m.bytes.push_back(0xf7);
        // DEBUG("Sending control update e1 ctrl %id, value %s", id, displayValue);
        sendMessage(m);

   }

   /**
    * Inform E1 that a change module action is starting
    */
   void changeE1Module(const char* moduleDisplayName, float moduleY, float moduleX, int maxNprnId) {

        std::stringstream ss;
        ss << "changeE1Module(\"" << moduleDisplayName << "\", " << string::f("%g, %g, %d", moduleY, moduleX, maxNprnId) << ")";
        sendE1ExecuteLua(ss.str().c_str());

   }

 	/**
 	 * Inform E1 that a change module sequence has completed
 	 */
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
    void mappedModuleInfo(RackMappedModuleListItem& m) {
        std::stringstream ss;
        ss << "mappedMI(\"" << m.getModuleKey() << "\", \"" << m.getModuleDisplayName() <<  "\", " << string::f("%g", m.getY()) << ", " << string::f("%g", m.getX()) << ")";
        sendE1ExecuteLua(ss.str().c_str());
    }
    void endMappedModuleList() {
         std::stringstream ss;
         ss << "endMML()";
         sendE1ExecuteLua(ss.str().c_str());
    }
    void sendOrestesOneVersion(std::string o1Version) {
    	std::stringstream ss;
    	ss << "o1Version(\"" << o1Version << "\")";
        sendE1ExecuteLua(ss.str().c_str());

    } 

    /**
     * Packs a 14 bit NPRN parameter update to E1 encoded in a single 9-byte output Sysex message
     * 
 	 * Byte(s)
     * =======
     * [0 ]        0xF0 SysEx header byte
     * [1-3]       0x00 0x7F 0x7F Placeholder MIDI Manufacturer Id
     * [4]         0x01 Control update
     * [5]         NPRN id MSB (0-127)
     * [6]         NPRN id LSB (0-127)
     * [7]         Value MSB (0-127)
     * [8]         Value LSB (0-127)
     * [9]       0xF7 SysEx end byte
     */
    void setPackedNPRNValue(int value, int nprn, int valueNprnIn, bool force = false) {
		if ((value == lastNPRNValues[nprn] || value == valueNprnIn) && !force)
			return;
		lastNPRNValues[nprn] = value;

  		m.bytes.clear();
        // SysEx header byte
        m.bytes.push_back(0xF0);
        // SysEx header byte
        // Custom MIDI manufacturer Id
        m.bytes.push_back(0x00);
        m.bytes.push_back(0x7F);
        m.bytes.push_back(0x7F);
        // Packed NPRN
        m.bytes.push_back(0x00);
        // controlId MSB
 		m.bytes.push_back(nprn >> 7);
        // controlId LSB
        m.bytes.push_back(nprn & 0x7F);
      	// value MSB
 		m.bytes.push_back(value >> 7);
        // value LSB
        m.bytes.push_back(value & 0x7F);
        // SysEx closing byte
 		m.bytes.push_back(0xf7);

 		// DEBUG("Sending bytes %s", hexStr(m.bytes.data(), m.getSize()).data());
        sendMessage(m);

    }
    

   /**
    * Execute a Lua command on the Electra One
    * @see https://docs.electra.one/developers/midiimplementation.html#execute-lua-command
    */
   void sendE1ExecuteLua(const char* luaCommand) {
        // DEBUG("Execute Lua %s", luaCommand);

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

        // DEBUG("Sending bytes %s", hexStr(m.bytes.data(), m.getSize()).data());
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
            module->midiOutput.setPackedNPRNValue(value, nprn, module->valuesNprn[nprn], current == -1);
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
    bool e1ProcessApply;
    bool e1ProcessApplyRackMapping;
    math::Vec e1SelectedModulePos;
    bool e1ProcessResendMIDIFeedback;
    bool e1VersionPoll;

    // E1 Process flags
    int sendE1EndMessage = 0;
    bool e1ProcessListMappedModules;
    // Re-usable list of mapped modules
    std::vector< RackMappedModuleListItem > e1MappedModuleList;
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

	/** The value of each NPRN parameter, assuming use range 0 .. MAX_NPRN_ID */
    int valuesNprn[MAX_NPRN_ID+1];
    uint32_t valuesNprnTs[MAX_NPRN_ID+1];
	
	MIDIMODE midiMode = MIDIMODE::MIDIMODE_DEFAULT;

	/** Track last channel values */
	int lastValueIn[MAX_CHANNELS];
	int lastValueInIndicate[MAX_CHANNELS];
	int lastValueOut[MAX_CHANNELS];

	dsp::RingBuffer<int, 8> overlayQueue;
	/** [Stored to Json] */
	bool overlayEnabled;

	/** [Stored to Json] */
	RackParam midiParam[MAX_CHANNELS];
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
	std::string midiMapLibraryFilename;
	/** [Stored to JSON] */
	bool autosaveMappingLibrary = true;

	/** Internal module midiMap. Not saved to module Json. */
	std::map<std::pair<std::string, std::string>, MemModule*> midiMap;

	/** [Stored to JSON] 
	 * Stores rack-level mapping e.g. for Patchmaster mappings
	 * Used to switch between this mapping and a module mapping in midiMap
	 */
	MemModule rackMapping = MemModule();

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
		for (int i = 0; i <= MAX_NPRN_ID; i++) {
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
		processDivision = 4098;
		processDivider.setDivision(processDivision);
		processDivider.reset();
		overlayEnabled = true;
		clearMapsOnLoad = false;
		
		e1ProcessNext = false;
		e1ProcessPrev = false;
		e1ProcessSelect = false;
		e1ProcessApply = false;
		e1ProcessApplyRackMapping = false;
		e1ProcessListMappedModules = false;
		e1ProcessResetParameter = false;
		e1VersionPoll = false;
		midiMapLibraryFilename.clear();
		autosaveMappingLibrary = true;
		resetMap();
		rackMapping.reset();
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

        // Only step channels when some midi event has been received. Additionally
        // step channels for parameter changes made manually at a lower frequency . Notice
        // that midi allows about 1000 messages per second, so checking for changes more often
        // won't lead to higher precision on midi output.
        bool stepParameterChange = processDivider.process();
        if (stepParameterChange || midiReceived) {
            processMappings(args.sampleTime, stepParameterChange, midiReceived);
            processE1Commands();
        }

	}

	void processMappings(float sampleTime, bool stepParameterChange, bool midiReceived) {
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
				        

						// Send enriched parameter feedback to E1
						// But only at the "processDivider" rate to control data rate sent to E1.
						// This means the displayed parameter values on E1 will lag the actual parameter value whilst
						// the parameter is being chnaged (either from E1 or from the VCVRack GUI).
						// Users can adjust the Oresets-One "Precision" to balance that lag with stability of E1 (reducing data traffic)
						if (stepParameterChange) {
							// Send manually altered parameter change out to MIDI
						    nprns[id].setValue(v, lastValueIn[id] < 0);
							lastValueOut[id] = v;

							// DEBUG("Sending MIDI feedback for %d, value %d", id, v);
							sendE1Feedback(id);
                        	e1ProcessResetParameter = false;
                       		 // If we are broadcasting parameter updates when switching modules,
                       		 // record that we have now sent this parameter
                        	if (sendE1EndMessage > 0) sendE1EndMessage--;
                        }
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

        if (e1VersionPoll) {
        	// Send the OrestesOne plugin version to E1
        	std::string o1PluginVersion = model->plugin->version;
        	midiCtrlOutput.sendOrestesOneVersion(o1PluginVersion);
        	e1VersionPoll = false;
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
                // If there more than one instance of  mapped module in the rack, it will appear in the list multiple times
                auto key = string::f("%s %s", m->model->plugin->slug.c_str(), m->model->slug.c_str());
                RackMappedModuleListItem item = {
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
     * [0 ]       	 0xF0 SysEx header byte
     * [1-3]       	0x00 0x7F 0x7F Placeholder MIDI Manufacturer Id
     * 
     * Packed NPRN
     * ===========
     * [4]			0x00 Packed NPRN
     * [5]         	NPRN id MSB (0-127)
     * [6]         	NPRN id LSB (0-127)
     * [7]			value id MSB (0-127)
     * [8]			value id LSB (0-127)
     * [9]       	0xF7 SysEx end byte
     * 
     * Commands
     * ========
     * [4]         	0x01 Command
     * [...]
     * [end]       	0xF7 SysEx end byte
     * ---
     * Command: Next
     * [5]         	0x01 Next mapped module
     *
     * Command: Prev
     * [5]         	0x02 Prev mapped module
     *
     * Command: Select
     * [5]         	0x03 Select mapped module
     * [6-x]      	Module rack y position as a length byte + ascii string byte array,
     * [x+1-y]     	Module rack x position as a length byte + ascii string byte array
     *
     * Command: List mapped modules
     * [5]         	0x04 List mapped modules
     *
     * Command: Reset mapped parameter to its default
     * [5]         	0x05 Reset Parameter
     * [6]         	NPRN id MSB (0-127)
     * [7]         	NPRN id LSB (0-127)
     * 
     * Command: Re-send MIDI feedback
     * [5]			0x06 Re-send MIDI
     *
     * Command: Apply module mapping
     * [5]			0x07 Re-send MIDI
     * 
     * Command: Apply rack mapping
     * [5]			0x08 Re-send MIDI
     * 
     * Command: Version Poll
     * [5]			0x09 Version Poll
     * 
     */
    bool parseE1SysEx(midi::Message msg) {
        if (msg.getSize() < 7)
            return false;
        // Check this is one of our SysEx messages from our E1 preset
        if (msg.bytes.at(1) != 0x00 || msg.bytes.at(2) != 0x7f || msg.bytes.at(3) != 0x7f)
            return false;

        switch(msg.bytes.at(4)) {
        	// Packed NPRN
        	case 0x00: {
				int nprn = (msg.bytes.at(5) << 7) + msg.bytes.at(6);
                int value = (msg.bytes.at(7) << 7) + msg.bytes.at(8);
                isPendingNPRN = false;

                // Guard to limit max recognised NPRN Parameter Id
                if (nprn > MAX_NPRN_ID) {
                	return false;
                }
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
            // Command
            case 0x01: {
                switch(msg.bytes.at(5)) {
                    // Next
                    case 0x01: {
                        // DEBUG("Received an E1 Next Command");
                        e1ProcessNext = true;
                        return true;
                    }
                    // Prev
                    case 0x02: {
                        // DEBUG("Received an E1 Prev Command");
                        e1ProcessNext = false;
                        e1ProcessPrev = true;
                        return true;
                    }
                    // Module Select
                    case 0x03: {
                        // DEBUG ("Received an E1 Module Select Command");
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
                        // DEBUG("Received an E1 List Mapped Modules Command");
                        e1ProcessListMappedModules = true;
                        return true;
                    }
                    // Reset parameter to its default
                    case 0x05: {
                        e1ProcessResetParameter = true;
                        e1ProcessResetParameterNPRN = ((int) msg.bytes.at(6) << 7) + ((int) msg.bytes.at(7));

                        // DEBUG("Received an E1 Reset Parameter Command for NPRN %d", e1ProcessResetParameterNPRN);
                        return true;
                    }
                    // Re-send MIDI feedback
                    case 0x06: {
                        // DEBUG("Received an E1 Re-send MIDI Feedback Command");
                        e1ProcessResendMIDIFeedback = true;
                        return true;
                    }
                    // Apply Module
	                case 0x07: {
	                	// DEBUG("Received an E1 Apply Module Command");
	                	e1ProcessApply = true;
	                	return true;	
	                }
	                // Apply Rack Mapping
	            	case 0x08: {
	            		// DEBUG("Received an E1 Apply Rack Mapping Command");
	            		e1ProcessApplyRackMapping = true;
	            		return true;
	            	}
	            	// Version Poll
	            	case 0x09: {
	            		// DEBUG("Received an E1 Version Poll Command");
	            		e1VersionPoll = true;
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

                    // Guard to limit max recognised NPRN Parameter Id
                    if (nprn > MAX_NPRN_ID) {
                    	return false;
                    }

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

	void changeE1Module(const char* moduleName, float moduleY, float moduleX, int maxNprnId) {
	    // DEBUG("changeE1Module to %s", moduleName);
	    midiCtrlOutput.changeE1Module(moduleName, moduleY, moduleX, maxNprnId);
	}

	void endChangeE1Module() {
	    // DEBUG("endChangeE1Module");
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

	void learnParamAutomap(int id, int64_t moduleId, int paramId) {
		APP->engine->updateParamHandle(&paramHandles[id], moduleId, paramId, true);
		midiParam[id].reset(true);
		learnedParam = true;
		if (id <= MAX_NPRN_ID) {
			learnedNprn = true;
			nprns[id].setNprn(id);
		}
		commitLearn();
		updateMapLen();
	}

	void moduleBind(Module* m, bool keepCcAndNote, bool autoMap = false) {
		if (!m) return;
		if (!keepCcAndNote || autoMap) {
			clearMaps_WithLock();
		}
		else {
			// Clean up some additional mappings on the end
			for (int i = int(m->params.size()); i < mapLen; i++) {
				APP->engine->updateParamHandle(&paramHandles[i], -1, -1, true);
			}
		}
		for (size_t i = 0; i < m->params.size() && i < MAX_CHANNELS; i++) {
			if (autoMap) {
				learnParamAutomap(int(i), m->id, int(i));
			} else {
				learnParam(int(i), m->id, int(i));
			}
		}

		if (autoMap) {
			// Also auto - save the module mapping in the midiMap
			std::string pluginSlug = m->model->plugin->slug;
            std::string moduleSlug = m->model->slug;

			expMemSave(pluginSlug, moduleSlug, true);

		}
		updateMapLen();
	}

    /**
     * Creates a new module mapping for every module in the current rack.
     * Optionally skips modules which already have a mapping definition loaded into Orestes-One midiMap
     */
    void autoMapAllModules(bool skipPremappedModules) {

    	// Get snapshot of current state
		json_t* currentStateJ = toJson();

    	// Iterate over every module in the rack
        std::list<Widget*> modules = APP->scene->rack->getModuleContainer()->children;
        modules.sort();
        std::list<Widget*>::iterator it = modules.begin();

        // Scan over all rack modules
        for (; it != modules.end(); it++) {
            ModuleWidget* mw = dynamic_cast<ModuleWidget*>(*it);
            Module* m = mw->module;

            std::string pluginSlug = m->model->plugin->slug;
            std::string moduleSlug = m->model->slug;

            // if module is bypassed, skip
            if (m->isBypassed()) {
            	// DEBUG("Skipping %s isBypassed", m->model->slug.c_str());
            	continue;
            }

	    	// If module is in automap black list, skip (e.g. Patchmaster)
	    	if (!isModuleAutomappable(m)) {
	    		// DEBUG("Skipping %s as excluded", m->model->slug.c_str());
	    		continue;
	    	}

	    	// If module is already mapped in the midiMap, and skipPremappedModules = true, skip
	    	if (skipPremappedModules && expMemTest(m)) {
	    		// DEBUG("Skipping %s as pre-mapped", m->model->slug.c_str());
	    		continue;
	    	}

	    	// Bind & save module parameters in midimap
	    	// DEBUG("Binding %s", m->model->slug.c_str());
	    	moduleBind(m, false, true);
        }

        // Update saved mapping library file
        expMemSaveLibrary();

    	// history::ModuleChange
		history::ModuleChange* h = new history::ModuleChange;
		h->name = "automap all modules";
		h->moduleId = this->id;
		h->oldModuleJ = currentStateJ;
		h->newModuleJ = toJson();
		APP->history->push(h);

    }

	void refreshParamHandleText(int id) {
		std::string text = "ORESTES-ONE";
		if (nprns[id].getNprn() >= 0) {
        	text += string::f(" nprn%03d", nprns[id].getNprn());
        }
		paramHandles[id].text = text;
	}

	void expMemSave(std::string pluginSlug, std::string moduleSlug, bool autoMapped) {
		MemModule* m = new MemModule;
		Module* module = NULL;
		bool hasParameters = false;
		for (size_t i = 0; i < MAX_CHANNELS; i++) {
			if (paramHandles[i].moduleId < 0) continue;
			if (paramHandles[i].module->model->plugin->slug != pluginSlug && paramHandles[i].module->model->slug == moduleSlug) continue;
			hasParameters = true;
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

		if (!hasParameters) return; // No mapped parameters, so do not add to map

		m->pluginName = module->model->plugin->name;
		m->moduleName = module->model->name;
		m->autoMapped = autoMapped; // Manually saving module map, so assume is no longer considered auto-mapped
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
		if (it != midiMap.end()) {
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

	/**
	 * Store current mapping parameters as restorable rack-level mapping
	 */
	void expMemSaveRackMapping() {
		rackMapping.reset();
		for (size_t i = 0; i < MAX_CHANNELS; i++) {
			if (nprns[i].getNprn() >= 0 || paramHandles[i].moduleId >= 0) {
                MemParam* p = new MemParam;
                p->paramId = paramHandles[i].paramId;
                p->nprn = nprns[i].getNprn();
                p->nprnMode = nprns[i].nprnMode;
                p->label = textLabel[i];
                p->midiOptions = midiOptions[i];
                p->slew = midiParam[i].getSlew();
                p->min = midiParam[i].getMin();
                p->max = midiParam[i].getMax();
                p->moduleId = paramHandles[i].moduleId;
                rackMapping.paramMap.push_back(p);    
            }
		}
	}


	void expMemApply(Module* m, math::Vec pos = Vec(0,0)) {
		if (!m) return;
		auto p = std::pair<std::string, std::string>(m->model->plugin->slug, m->model->slug);
		auto it = midiMap.find(p);
		if (it == midiMap.end()) return;
		MemModule* map = it->second;

        // Send message to E1 to prep for new mappings before new values sent
        int maxNprnId = 0;
        for (MemParam* it : map->paramMap) {
        	if (it->nprn > maxNprnId) {
        		maxNprnId = it->nprn;
        	}
        }
		changeE1Module(m->model->getFullName().c_str(), pos.y, pos.x, maxNprnId);

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

	void expMemApplyRackMapping() {

		if (rackMapping.paramMap.empty()) return;

		// Send message to E1 to prep for new rack mappings before new values sent
        int maxNprnId = 0;
        for (MemParam* it : rackMapping.paramMap) {
        	if (it->nprn > maxNprnId) {
        		maxNprnId = it->nprn;
        	}
        }
		changeE1Module("Rack Mapping", 0, 0, maxNprnId);
		clearMaps_WithLock();
		midiOutput.reset();
		midiCtrlOutput.reset();
		expMemModuleId = -1;

		int i = 0;
		sendE1EndMessage = 1;
		for (MemParam* it : rackMapping.paramMap) {
			learnParam(i,it->moduleId, it->paramId);
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

		// DEBUG("Exporting midimaps for plugin %s to file %s", pluginSlug.c_str(), path);

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
		return findModuleInMidiMap(m->model->plugin->slug, m->model->slug);
	}
	bool findModuleInMidiMap(std::string pluginSlug, std::string moduleSlug) {
		auto p = std::pair<std::string, std::string>(pluginSlug, moduleSlug);
		auto it = midiMap.find(p);
		if (it == midiMap.end()) return false;
		return true;
	}

	/**
	 * Re-writes the internal midiMap into the current mapping library file,
	 * overwriting existing file contents
	 */ 
	void expMemSaveLibrary(bool force = false) {

		if (midiMapLibraryFilename.empty()) return;
		if (!force && !autosaveMappingLibrary) return;

		saveMappingLibraryFile(midiMapLibraryFilename);

	}

	/**
	 * Determines if module is excluded from automapping
	 */
    bool isModuleAutomappable(Module* m) {
    	if (!m) return false;
		auto p = std::pair<std::string, std::string>(m->model->plugin->slug, m->model->slug);
		auto it = AUTOMAP_EXCLUDED_MODULES.find(p);
		return (it == AUTOMAP_EXCLUDED_MODULES.end());
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


		json_t* rackMappingJJ = json_object();		
		json_t* paramMapJ = json_array();
		for (auto p : rackMapping.paramMap) {
            if (p->paramId >= 0 && p->nprn >= 0 && p->nprn < MAX_NPRN_ID && p->moduleId > 0) {
                json_t* paramMapJJ = json_object();
                json_object_set_new(paramMapJJ, "paramId", json_integer(p->paramId));
                json_object_set_new(paramMapJJ, "nprn", json_integer(p->nprn));
                json_object_set_new(paramMapJJ, "nprnMode", json_integer((int)p->nprnMode));
                json_object_set_new(paramMapJJ, "label", json_string(p->label.c_str()));
                json_object_set_new(paramMapJJ, "midiOptions", json_integer(p->midiOptions));
                json_object_set_new(paramMapJJ, "slew", json_real(p->slew));
                json_object_set_new(paramMapJJ, "min", json_real(p->min));
                json_object_set_new(paramMapJJ, "max", json_real(p->max));
                json_object_set_new(paramMapJJ, "moduleId", json_integer(p->moduleId));
                json_array_append_new(paramMapJ, paramMapJJ);
            }
			
		}
		json_object_set_new(rackMappingJJ, "paramMap", paramMapJ);

		// json_t* rackMappingJ = rackMappingToJson(rackMapping);
		json_object_set_new(rootJ, "rackMapping", rackMappingJJ);

		// Only persist the location of the module mapping json file in module / preset state
		// Avoids saving (potentially large) full mapping library json in rack autosave patch.json
		json_object_set_new(rootJ, "midiMapLibraryFilename", json_string(midiMapLibraryFilename.c_str()));
		json_object_set_new(rootJ, "autosaveMidiMapLibrary", json_boolean(autosaveMappingLibrary));

		return rootJ;
	}

	json_t* midiMapToJsonArray(std::map<std::pair<std::string, std::string>, MemModule*>& aMidiMap) {
		json_t* midiMapJ = json_array();
		for (auto it : aMidiMap) {
			json_t* midiMapJJ = json_object();
			json_object_set_new(midiMapJJ, "ps", json_string(it.first.first.c_str())); // pluginSlug
			json_object_set_new(midiMapJJ, "ms", json_string(it.first.second.c_str())); // moduleSlug
			auto a = it.second;
			json_object_set_new(midiMapJJ, "am", json_boolean(a->autoMapped)); // autoMapped
			json_object_set_new(midiMapJJ, "pn", json_string(a->pluginName.c_str())); // pluginName
			json_object_set_new(midiMapJJ, "mn", json_string(a->moduleName.c_str())); // moduleName
			json_t* paramMapJ = json_array();
			for (auto p : a->paramMap) {
				json_t* paramMapJJ = json_object();
				json_object_set_new(paramMapJJ, "p", json_integer(p->paramId));
				json_object_set_new(paramMapJJ, "n", json_integer(p->nprn));
				json_object_set_new(paramMapJJ, "nm", json_integer((int)p->nprnMode));
				json_object_set_new(paramMapJJ, "l", json_string(p->label.c_str()));
				json_object_set_new(paramMapJJ, "o", json_integer(p->midiOptions));
				json_object_set_new(paramMapJJ, "s", json_real(p->slew));
				json_object_set_new(paramMapJJ, "m", json_real(p->min));
				json_object_set_new(paramMapJJ, "x", json_real(p->max));
				json_array_append_new(paramMapJ, paramMapJJ);
			}
			json_object_set_new(midiMapJJ, "pm", paramMapJ); // paramMap

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
				if (clockModeJ) midiParam[mapIndex].clockMode = (RackParam::CLOCKMODE)json_integer_value(clockModeJ);
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

		// Rack Mapping
		json_t* rackMappingJ = json_object_get(rootJ, "rackMapping");
		if (rackMappingJ) {

			rackMapping.reset();
			json_t* paramMapJ = json_object_get(rackMappingJ, "paramMap");
			size_t j;
			json_t* paramMapJJ;
			json_array_foreach(paramMapJ, j, paramMapJJ) {
				MemParam* p = new MemParam;
				p->paramId = json_integer_value(json_object_get(paramMapJJ, "paramId"));
                p->moduleId = json_integer_value(json_object_get(paramMapJJ, "moduleId"));
                p->nprn = json_integer_value(json_object_get(paramMapJJ, "nprn"));
                if (p->nprn >= 0 && p->nprn <= MAX_NPRN_ID && p->paramId >= 0 && p->moduleId > 0) {
                    p->nprnMode = (NPRNMODE)json_integer_value(json_object_get(paramMapJJ, "nprnMode"));
                    p->label = json_string_value(json_object_get(paramMapJJ, "label"));
                    p->midiOptions = json_integer_value(json_object_get(paramMapJJ, "midiOptions"));
                    json_t* slewJ = json_object_get(paramMapJJ, "slew");
                    if (slewJ) p->slew = json_real_value(slewJ);
                    json_t* minJ = json_object_get(paramMapJJ, "min");
                    if (minJ) p->min = json_real_value(minJ);
                    json_t* maxJ = json_object_get(paramMapJJ, "max");
                    if (maxJ) p->max = json_real_value(maxJ);
                    rackMapping.paramMap.push_back(p);
                }
				
			}

		}

		json_t* autosaveMappingLibraryJ = json_object_get(rootJ, "autosaveMidiMapLibrary");
		if (autosaveMappingLibraryJ) autosaveMappingLibrary = json_boolean_value(autosaveMappingLibraryJ);

		// Try to read and load the  module midimap library from the saved file location from the saved JSON
		bool midiMapLoaded = false;
		json_t* midiMapLibraryFilenameJ = json_object_get(rootJ, "midiMapLibraryFilename");
		if (midiMapLibraryFilenameJ) {

			midiMapLibraryFilename = json_string_value(midiMapLibraryFilenameJ);

			// Load library data from file into json
			if (!midiMapLibraryFilename.empty()) {
				if (readMappingLibraryFile(midiMapLibraryFilename)) midiMapLoaded = true;
			} else {
				// If there is a default library file in the user presets, load that anyway
				std::string userPresetPath = this->model->getUserPresetDirectory();
				midiMapLibraryFilename = system::join(userPresetPath, DEFAULT_LIBRARY_FILENAME);

				if (system::exists(midiMapLibraryFilename)) midiMapLoaded = readMappingLibraryFile(midiMapLibraryFilename);

			}
		}

		// No loaded midimap library file either from Orestes-One state or loaded preset
		// So try and load default factory mapping library
		if (!midiMapLoaded && loadMidiMapFromFactoryLibraryFile()) {
			// factory library loaded OK, now save copy of factory midimap to user preset folder if there is not already one in there
			std::string userPresetPath = this->model->getUserPresetDirectory();
			std::string defaultMidiMapLibraryFilename = system::join(userPresetPath, DEFAULT_LIBRARY_FILENAME);

			if (!system::exists(defaultMidiMapLibraryFilename)) {
                system::createDirectories(userPresetPath); // NB: no-op if model preset folder already exists
				midiMapLibraryFilename = defaultMidiMapLibraryFilename;
				saveMappingLibraryFile(defaultMidiMapLibraryFilename);
			}
		}
			
	}

	bool loadMidiMapFromFactoryLibraryFile() {
        // Load factory default library
        // It is stored in the RSBATechModules plugin presets folder
        std::string factoryLibraryFilename = asset::plugin(this->model->plugin, system::join("presets", FACTORY_LIBRARY_FILENAME));

		if (!system::exists(factoryLibraryFilename)) {
			WARN("Factory library file %s does not exist, skipping", factoryLibraryFilename.c_str());
			return false;
		}
		
		FILE* file = fopen(factoryLibraryFilename.c_str(), "r");
		if (!file) {
			WARN("Could not load factory library file %s, skipping", factoryLibraryFilename.c_str());
			return false;
		}
		DEFER({
			fclose(file);
		});

		json_error_t error;
		json_t* libraryJ = json_loadf(file, 0, &error);
		DEFER({
			json_decref(libraryJ);
		});

		if (!libraryJ) {
			WARN("Factory library file is not a valid JSON file. Parsing error at %s %d:%d %s, skipping", error.source, error.line, error.column, error.text);
			return false;
		}
	
		if (loadMidiMapFromLibrary(libraryJ)) {
            INFO("Loaded factory library file %s", factoryLibraryFilename.c_str());
			return true;
		} else {
			return false;
		}


	}


	bool readMappingLibraryFile(std::string filename) {

		// DEBUG ("Reading mapping library file at %s", filename.c_str());
		FILE* file = fopen(filename.c_str(), "r");
		if (!file) {
			WARN("Could not load mapping library file %s", filename.c_str());
			return false;
		}
		DEFER({
			fclose(file);
		});

		json_error_t error;
		json_t* libraryJ = json_loadf(file, 0, &error);
		DEFER ({
			json_decref(libraryJ);
		});

		if (!libraryJ) {
			WARN("File is not a valid JSON file. Parsing error at %s %d:%d %s", error.source, error.line, error.column, error.text);
			return false;
		}
		return loadMidiMapFromLibrary(libraryJ);
		
	}

	/**
	 * Replaces internal midiMap from selected mapping library file
	 * 
	 * Mutates the currentStateJ with the loaded midiMap list
	 */
	bool loadMidiMapFromLibrary(json_t* libraryJ) {

		std::string pluginSlug = json_string_value(json_object_get(libraryJ, "plugin"));

		std::string modelSlug = json_string_value(json_object_get(libraryJ, "model"));

		// Only handle midimap library JSON files created by OrestesOne
		if (!(pluginSlug == this->model->plugin->slug && modelSlug == this->model->slug))
			return false;

		// Get the midiMap in the imported library Json
		json_t* dataJ = json_object_get(libraryJ, "data");
		json_t* midiMapJ = json_object_get(dataJ, "midiMap");
		
		// Load libray file midiMap into internal midiMap state
		midiMapJSONArrayToMidiMap(midiMapJ);

		// DEBUG("Loaded midiMap with %d", (int)midiMap.size());

		return true;
	}

	/**
	 * Replaces internal midiMap with contents of a midiMap JSON array
	 */
	void midiMapJSONArrayToMidiMap(json_t* midiMapJ) {

		resetMap();
		size_t i;
		json_t* midiMapJJ;
		json_array_foreach(midiMapJ, i, midiMapJJ) {
			midiMapJSONToMidiMap(midiMapJJ);
		}
	}

	void midiMapJSONToMidiMap(json_t* midiMapJJ) {
		std::string pluginSlug = json_string_value(json_object_get(midiMapJJ, "ps")); // pluginSlug
		std::string moduleSlug = json_string_value(json_object_get(midiMapJJ, "ms")); // moduleSlug

		MemModule* a = new MemModule;
		a->pluginName = json_string_value(json_object_get(midiMapJJ, "pn")); // pluginName
		a->moduleName = json_string_value(json_object_get(midiMapJJ, "mn")); // moduleName
		json_t* autoMappedJ = json_object_get(midiMapJJ, "am"); // autoMapped
		if (autoMappedJ) {
			a->autoMapped = json_boolean_value(autoMappedJ);
		} else {
			a->autoMapped = false; // default
		}
		json_t* paramMapJ = json_object_get(midiMapJJ, "pm"); // paramMap
		size_t j;
		json_t* paramMapJJ;
		json_array_foreach(paramMapJ, j, paramMapJJ) {
			MemParam* p = new MemParam;
			p->paramId = json_integer_value(json_object_get(paramMapJJ, "p")); // paramId
			p->nprn = json_integer_value(json_object_get(paramMapJJ, "n")); // nprnId
			p->nprnMode = (NPRNMODE)json_integer_value(json_object_get(paramMapJJ, "nm")); // nprnMode
			p->label = json_string_value(json_object_get(paramMapJJ, "l")); // label
			p->midiOptions = json_integer_value(json_object_get(paramMapJJ, "o")); // midiOptions
			json_t* slewJ = json_object_get(paramMapJJ, "s"); // slew
			if (slewJ) p->slew = json_real_value(slewJ);
			json_t* minJ = json_object_get(paramMapJJ, "m"); // min
			if (minJ) p->min = json_real_value(minJ);
			json_t* maxJ = json_object_get(paramMapJJ, "x"); // max
			if (maxJ) p->max = json_real_value(maxJ);
			a->paramMap.push_back(p);

		}
		midiMap[std::pair<std::string, std::string>(pluginSlug, moduleSlug)] = a;
	}

	/**
	 * Co-ordinates saving internal midimap state as a mapping library json file
	 */
	bool saveMappingLibraryFile(std::string filename) {

		INFO ("Saving library to %s", filename.c_str());
		json_t* rootJ = json_object();
		DEFER({
			json_decref(rootJ);
		});

		json_object_set_new(rootJ, "plugin", json_string(this->model->plugin->slug.c_str()));
		json_object_set_new(rootJ, "model", json_string(this->model->slug.c_str()));
		json_t* dataJ = json_object();
		json_t* midiMapJ = midiMapToJsonArray(midiMap);

		json_object_set_new(dataJ, "midiMap", midiMapJ);
		json_object_set_new(rootJ, "data", dataJ);

		// Write to json file
		FILE* file = fopen(filename.c_str(), "w");
		if (!file) {
			WARN("Could not open mapping library file for writing %s", filename.c_str());
			return false;
		}
		DEFER({
			fclose(file);
		});

        // Save midimap library JSON in a relatively compact form.
        // Assume can be expanded in text editors if anyone needs to read and edit them directly.
		if (json_dumpf(rootJ, file, 0) < 0) {
			std::string message = string::f("File could not be written to %s", filename.c_str());
			osdialog_message(OSDIALOG_ERROR, OSDIALOG_OK, message.c_str());
			return false;
		}

		return true;
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




} // namespace OrestesOne
} // namespace Orestes

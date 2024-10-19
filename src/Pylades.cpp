#include "plugin.hpp"
#include "Pylades.hpp"
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

namespace RSBATechModules {
namespace Pylades {



struct PyladesModule : Module {

struct OscOutput {
	public:

    OscOutput(PyladesModule& module): moduleRef(module) {
		reset();
		b.reserve(1, 2);
	}

	void reset() {
		std::fill_n(lastNPRNValues.begin(), MAX_CHANNELS, -1);
    	b.clear();
	}

	/**
	 * Send fader value, and optionally rich display value and name
	 */
    void sendOscControlUpdate(int id, const char* name, const char* displayValue) {
        
        if (moduleRef.sending) {
	        TheModularMind::OscBundle feedbackBundle;
			TheModularMind::OscMessage infoMessage;

			infoMessage.setAddress("/fader/info");
			infoMessage.addIntArg(id); // controller id
			infoMessage.addStringArg(displayValue); // displayValue
		    infoMessage.addIntArg(1); // visible
		    infoMessage.addStringArg(name); // parameter display name
			feedbackBundle.addMessage(infoMessage);
		

			moduleRef.oscSender.sendBundle(feedbackBundle);	
        }
		

   }

   /**
    * Inform E1 that a change module action is starting
    */
   void changeE1Module(const char* moduleName, const char* moduleDisplayName, float moduleY, float moduleX, int maxNprnId) {

		if (moduleRef.sending) {
	   		TheModularMind::OscBundle feedbackBundle;
			TheModularMind::OscMessage infoMessage;

			infoMessage.setAddress("/module/changing");
			infoMessage.addStringArg(moduleName);
			infoMessage.addStringArg(moduleDisplayName);
			infoMessage.addFloatArg(moduleY);
			infoMessage.addFloatArg(moduleX);
			infoMessage.addIntArg(maxNprnId);
			feedbackBundle.addMessage(infoMessage);
		

			moduleRef.oscSender.sendBundle(feedbackBundle);

		}
   }

 	/**
 	 * Inform E1 that a change module sequence has completed
 	 */
   void endChangeE1Module() {

   		if (moduleRef.sending) {
	       	TheModularMind::OscBundle feedbackBundle;
			TheModularMind::OscMessage infoMessage;

			infoMessage.setAddress("/module/end");
			feedbackBundle.addMessage(infoMessage);
		
			moduleRef.oscSender.sendBundle(feedbackBundle);
		}

   }

   /**
    * Sends a series of lua commands to E1 to transmit the list of mapped modules in the current Rack patch.
    * Pass a begin() and end() of a list or vector of MappedModule objects.
    */
   template <class Iterator>
   void sendModuleList(Iterator begin, Iterator end, int numMappedModules) {

		if (moduleRef.sending) {
			
	        // 1. Send a startmml message
	        sendStartMappedModuleList();

	        TheModularMind::OscBundle modulesBundle;
	        modulesBundle.reserve(1,10);
	        int bundleMessageCount = 0;
	        // 2. Loop over iterator, Send a OscMessage for each mapped module
	        for (Iterator it = begin; it != end; ++it) {
	            mappedModuleInfo(*it, modulesBundle);
	            bundleMessageCount++;
	            if (bundleMessageCount == 10) {
	            	// flush bundle
	            	moduleRef.oscSender.sendBundle(modulesBundle);
	            	modulesBundle.clear();
	            	bundleMessageCount = 0;
	            } 
	        }
	        // Flush last module bundle
	        if (bundleMessageCount > 0) {
	        	moduleRef.oscSender.sendBundle(modulesBundle);
	        }

	        // 3. Finish with a endMappedModuleList OscMessage
	        sendEndMappedModuleList();

	    } else {
	    	WARN("Cannot send module list whilst not connected");
	    }
   }

    void sendStartMappedModuleList() {
    	TheModularMind::OscBundle moduleBundle;
    	TheModularMind::OscMessage moduleMessage;
		moduleMessage.setAddress("/module/startmml");
		moduleBundle.addMessage(moduleMessage);
		moduleRef.oscSender.sendBundle(moduleBundle);
    }
    void mappedModuleInfo(RackMappedModuleListItem& m, TheModularMind::OscBundle& mappedModulesBundle) {
		TheModularMind::OscMessage moduleMessage;
		moduleMessage.setAddress("/module/mappedmodule");
		moduleMessage.addStringArg(m.getModuleKey());
		moduleMessage.addStringArg(m.getModuleDisplayName());
		moduleMessage.addFloatArg(m.getY());
		moduleMessage.addFloatArg(m.getX());
		mappedModulesBundle.addMessage(moduleMessage);
    }
    void sendEndMappedModuleList() {
    	TheModularMind::OscBundle moduleBundle;
       	TheModularMind::OscMessage moduleMessage;
       	moduleMessage.setAddress("/module/endmml");
		moduleBundle.addMessage(moduleMessage);
		moduleRef.oscSender.sendBundle(moduleBundle);
    }
    
    void sendPyladesVersion(std::string o1Version) {

    	if (moduleRef.sending) {
	    	TheModularMind::OscBundle feedbackBundle;
			TheModularMind::OscMessage infoMessage;

			infoMessage.setAddress("/pylades/version");
			infoMessage.addStringArg(o1Version);
			feedbackBundle.addMessage(infoMessage);
		
			moduleRef.oscSender.sendBundle(feedbackBundle);
		}

    } 

    void setPackedNPRNValue(int value, int nprn, int valueNprnIn, bool force = false) {
		if ((value == lastNPRNValues[nprn] || value == valueNprnIn || !moduleRef.sending) && !force)
			return;
		lastNPRNValues[nprn] = value;

		TheModularMind::OscBundle valueBundle;
		TheModularMind::OscMessage valueMessage;
	
		valueMessage.setAddress("/fader");
		valueMessage.addIntArg(nprn);
		valueMessage.addIntArg(value);
		valueBundle.addMessage(valueMessage);

		moduleRef.oscSender.sendBundle(valueBundle);

    }

private:
	std::array<int, MAX_CHANNELS> lastNPRNValues{};
	// Assume can re-use a single OscBundle, maybe not thread safe ?
    TheModularMind::OscBundle b;
	PyladesModule& moduleRef;

};

	// TODO: OSC output structs

	TheModularMind::OscReceiver oscReceiver;
	TheModularMind::OscSender oscSender;
	std::string ip = "localhost";
	std::string rxPort = RXPORT_DEFAULT;
	std::string txPort = TXPORT_DEFAULT;



	/** [Stored to JSON] */
	int panelTheme = 0;

	enum ParamIds {
		PARAM_RECV, PARAM_SEND, 
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
		ENUMS(LIGHT_RECV, 3), 
		ENUMS(LIGHT_SEND, 3),
		LIGHT_APPLY,
		NUM_LIGHTS
	};

	struct OscNPRNAdapter {
        PyladesModule* module;
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
            module->oscOutput.setPackedNPRNValue(value, nprn, module->valuesNprn[nprn], current == -1);
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

    OscOutput oscOutput = OscOutput(*this); 

	/** Number of maps */
	int mapLen = 0;
    OscNPRNAdapter nprns[MAX_CHANNELS];
	/** [Stored to JSON] */
	int midiOptions[MAX_CHANNELS];
	/** [Stored to JSON] */
	bool midiIgnoreDevices;
	/** [Stored to JSON] */
	bool clearMapsOnLoad;

	/** [Stored to JSON] */
	bool receiving;
	/** [Stored to JSON] */
	bool sending;

    // Flags to indicate command received from E1 to be processed in widget tick() thread
    bool oscProcessNext;
    bool oscProcessPrev;
    bool oscProcessSelect;
    bool oscProcessApply;
    bool oscProcessApplyRackMapping;
    math::Vec oscSelectedModulePos;
    bool oscProcessResendMIDIFeedback;
    bool oscVersionPoll;
	bool oscReceived = false;
	bool oscSent = false;

    // E1 Process flags
    int sendE1EndMessage = 0;
    bool oscProcessListMappedModules;
    // Re-usable list of mapped modules
    std::vector< RackMappedModuleListItem > oscMappedModuleList;
    size_t INITIAL_MAPPED_MODULE_LIST_SIZE = 100;
    bool oscProcessResetParameter;
    int oscProcessResetParameterNPRN;

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
	dsp::ClockDivider lightDivider;

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


	PyladesModule() {
		panelTheme = pluginSettings.panelThemeDefault;

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(PARAM_RECV, 0.0f, 1.0f, 0.0f, "Enable Receiver");
		configParam(PARAM_SEND, 0.0f, 1.0f, 0.0f, "Enable Sender");
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
		lightDivider.setDivision(2048);
		midiResendDivider.setDivision(APP->engine->getSampleRate() / 2);
		onReset();
		oscMappedModuleList.reserve(INITIAL_MAPPED_MODULE_LIST_SIZE);
	}

	~PyladesModule() {
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
		oscOutput.reset();
		
		midiIgnoreDevices = false;
		midiResendPeriodically = false;
		midiResendDivider.reset();
		processDivision = 4098;
		processDivider.setDivision(processDivision);
		processDivider.reset();
		overlayEnabled = true;
		clearMapsOnLoad = false;
		
		oscProcessNext = false;
		oscProcessPrev = false;
		oscProcessSelect = false;
		oscProcessApply = false;
		oscProcessApplyRackMapping = false;
		oscProcessListMappedModules = false;
		oscProcessResetParameter = false;
		oscVersionPoll = false;
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

	bool isValidPort(std::string port) {
		bool isValid = false;
		try {
			if (port.length() > 0) {
				int portNumber = std::stoi(port);
				isValid = portNumber > 1023 && portNumber <= 65535;
			}
		} catch (const std::exception& ex) {
			isValid = false;
		}
		return isValid;
	}

	void receiverPower() {
		if (receiving) {
			if (!isValidPort(rxPort)) rxPort = RXPORT_DEFAULT;
			int port = std::stoi(rxPort);
			receiving = oscReceiver.start(port);
			if (receiving) INFO("Started OSC Receiver on port: %i", port);
		} else {
			oscReceiver.stop();
		}
	}

	void senderPower() {
		if (sending) {
			if (!isValidPort(txPort)) txPort = TXPORT_DEFAULT;
			int port = std::stoi(txPort);
			sending = oscSender.start(ip, port);
			if (sending) {
				INFO("Started OSC Sender on port: %i", port);
				midiResendFeedback();
			}
		} else {
			oscSender.stop();
		}
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
       oscOutput.sendOscControlUpdate(nprns[id].getNprn(), paramQuantity->getLabel().c_str(), ss.str().c_str());
    }

    void process(const ProcessArgs &args) override {
		ts++;

		// Aquire new OSC message from the Receiver
		TheModularMind::OscMessage rxMessage;
		oscReceived = false;
		while (oscReceiver.shift(&rxMessage)) {
			bool r = processOscMessage(rxMessage);
			oscReceived = oscReceived || r;
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

		if (oscProcessResendMIDIFeedback || (midiResendPeriodically && midiResendDivider.process())) {
			midiResendFeedback();
		}


		// This block from Oscelot https://github.com/The-Modular-Mind/oscelot
		// Process lights
		if (lightDivider.process() || oscReceived) {
			if (receiving) {
				if (oscReceived) {
					// Blue
					lights[LIGHT_RECV].setBrightness(0.0f);
					lights[LIGHT_RECV + 1].setBrightness(0.0f);
					lights[LIGHT_RECV + 2].setBrightness(1.0f);
				} else {
					// Green
					lights[LIGHT_RECV].setBrightness(0.0f);
					lights[LIGHT_RECV + 1].setBrightness(1.0f);
					lights[LIGHT_RECV + 2].setBrightness(0.0f);
				}
			} else {
				// Grey
				lights[LIGHT_RECV].setBrightness(0.028f);
				lights[LIGHT_RECV + 1].setBrightness(0.028f);
				lights[LIGHT_RECV + 2].setBrightness(0.028f);
			}

			if (sending) {
				if (oscSent) {
					// Blue
					lights[LIGHT_SEND].setBrightness(0.0f);
					lights[LIGHT_SEND + 1].setBrightness(0.0f);
					lights[LIGHT_SEND + 2].setBrightness(1.0f);
					oscSent = false;
				} else {
					// Green
					lights[LIGHT_SEND].setBrightness(0.0f);
					lights[LIGHT_SEND + 1].setBrightness(1.0f);
					lights[LIGHT_SEND + 2].setBrightness(0.0f);
				}
			} else {
				// Grey
				lights[LIGHT_SEND].setBrightness(0.028f);
				lights[LIGHT_SEND + 1].setBrightness(0.028f);
				lights[LIGHT_SEND + 2].setBrightness(0.028f);
			}
		}


        // Only step channels when some midi event has been received. Additionally
        // step channels for parameter changes made manually at a lower frequency . Notice
        // that midi allows about 1000 messages per second, so checking for changes more often
        // won't lead to higher precision on midi output.
        bool stepParameterChange = processDivider.process();
        if (stepParameterChange || oscReceived) {
            processMappings(args.sampleTime, stepParameterChange, oscReceived);
            processOscCommands();
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

 
                    if (!oscProcessResetParameter && nprn >= 0 && nprns[id].process()) {
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
					if (oscProcessResetParameter && nprn == oscProcessResetParameterNPRN) {
                        midiParam[id].setValueToDefault();
                        oscProcessResetParameterNPRN = -1;
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

						if (!oscProcessResetParameter && nprn >= 0 && nprns[id].nprnMode == NPRNMODE::DIRECT)
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
							oscSent = true;
							// DEBUG("Sending MIDI feedback for %d, value %d", id, v);
							sendE1Feedback(id);
                        	oscProcessResetParameter = false;
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

	/**
	 * Parses OSC commands sent from TouchOSC.
	 * 
	 * Processing for the non-fader commands are deferred to later in the process() function
	 * 
	 * OSC Messages and arguments
	 * 
	 * /fader (updated fader value from TouchOSC)
	 * ======
	 * [0] 		Controller Id (int)
	 * [1]		Value (int, 0-16535 14 bit integer)
	 * 
	 * /pylades/next (jump to next mapped module to right of current selected module in rack)
	 * =============
	 * []
	 * 
	 * /pylades/prev (jump to next mapped module to left of current selected module in rack)
	 * =============
	 * []
	 * 
	 * /pylades/select (jump to select mapped module)
	 * ===============
	 * [0]		Module Y (row) rack co-ordinate (float)
	 * [1]		Module X (column) rack co-ordinate (float)
	 * 
	 * /pylades/listmodules (return list of mapped modules in the rack)
	 * ====================
	 * []
	 * 
	 * /pylades/resetparam (resets mapped parameter to its default value)
	 * ===================
	 * [0] 		Controller Id (int)
	 * 
	 * /pylades/resend (re-sends info on current mapped parameters)
	 * ===============
	 * []
	 * 
	 */ 
	bool processOscMessage(TheModularMind::OscMessage msg) {

		std::string address = msg.getAddress();

		DEBUG("OSC message %s", address.c_str());

		if (address == OSCMSG_FADER) {
			int nprn = msg.getArgAsInt(0);
			int value = msg.getArgAsInt(1);
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
	        oscReceived = valuesNprn[nprn] != value;
	        valuesNprn[nprn] = value;
	        valuesNprnTs[nprn] = ts;
			return oscReceived;
		} else if (address == OSCMSG_NEXT_MODULE) {
			DEBUG("Received an OSC Next Command");
            oscProcessNext = true;
            return true;
		} else if (address == OSCMSG_PREV_MODULE) {
            DEBUG("Received an OSC Prev Command");
            oscProcessNext = false;
            oscProcessPrev = true;
            return true;
		} else if (address == OSCMSG_SELECT_MODULE) {
            DEBUG ("Received an OSC Module Select Command");
            oscProcessSelect = true;
            float moduleY = msg.getArgAsFloat(0);
            float moduleX = msg.getArgAsFloat(1);
            oscSelectedModulePos = Vec(moduleX, moduleY);
            return true;
		} else if (address == OSCMSG_LIST_MODULES) {
            DEBUG("Received an OSC List Mapped Modules Command");
            oscProcessListMappedModules = true;
            return true;
        } else if (address == OSCMSG_RESET_PARAM) {
        	DEBUG("Received an OSC Reset Parameter Command for id %d", oscProcessResetParameterNPRN);
 			oscProcessResetParameter = true;
            oscProcessResetParameterNPRN = msg.getArgAsInt(0);            
            return true;
        } else if (address == OSCMSG_RESEND) {
        	DEBUG("Received an OSC Re-send OSC Feedback Command");
            oscProcessResendMIDIFeedback = true;
            return true;
        } else if (address == OSCMSG_APPLY_MODULE) {
        	// Remotely switch on the "apply" mode, so next mouse click will apply mapped settings for selected module
        	DEBUG("Received an OSC Apply Module Command");
	        oscProcessApply = true;
	        return true;	
        } else if (address == OSCMSG_APPLY_RACK_MAPPING) {
        	DEBUG("Received an OSC Apply Rack Mapping Command");
	        oscProcessApplyRackMapping = true;
	        return true;
        } else if (address == OSCMSG_VERSION_POLL) {
        	DEBUG("Received an OSC Version Poll Command");
	        oscVersionPoll = true;
	        return true;
		} else {
			WARN("Discarding unknown OSC message. OSC message had address: %s and %i args", msg.getAddress().c_str(), (int)msg.getNumArgs());
			return false;
		};


	};

 	/**
     * Additional process() handlers for commands received from OSC
     */
    void processOscCommands() {

        if (oscProcessListMappedModules) {
            oscProcessListMappedModules = false;
            sendE1MappedModulesList();
            return;
        }

        if (oscProcessResendMIDIFeedback) {
            oscProcessResendMIDIFeedback = false;
        }

        if (oscVersionPoll) {
        	// Send the Pylades plugin version to TouchOSC
        	std::string o1PluginVersion = model->plugin->version;
        	oscOutput.sendPyladesVersion(o1PluginVersion);
        	oscVersionPoll = false;
        }

    }

   /**
     * Build list of mapped Rack modules and transmit to E1
     */
    void sendE1MappedModulesList() {

        oscMappedModuleList.clear();

        // Get list of all modules in current Rack, sorted by module y then x position
        std::list<Widget*> modules = APP->scene->rack->getModuleContainer()->children;
        auto sort = [&](Widget* w1, Widget* w2) {
            auto t1 = std::make_tuple(w1->box.pos.y, w1->box.pos.x);
            auto t2 = std::make_tuple(w2->box.pos.y, w2->box.pos.x);
            return t1 < t2;
        };
        modules.sort(sort);
        std::list<Widget*>::iterator it = modules.begin();

        oscMappedModuleList.clear();
        int count = 0;
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
                oscMappedModuleList.push_back(item);
                count++;
            }
        }
        DEBUG("Sending mapped module list with %d modules", count);
        oscOutput.sendModuleList(oscMappedModuleList.begin(), oscMappedModuleList.end(), count);

    }

	void midiResendFeedback() {
		for (int i = 0; i < MAX_CHANNELS; i++) {
			lastValueOut[i] = -1;
			nprns[i].resetValue();
		}
	}

	void changeE1Module(const char* moduleName, const char* moduleDisplayName, float moduleY, float moduleX, int maxNprnId) {
	    // DEBUG("changeE1Module to %s", moduleName);
	    oscOutput.changeE1Module(moduleName, moduleDisplayName, moduleY, moduleX, maxNprnId);
	}

	void endChangeE1Module() {
	    // DEBUG("endChangeE1Module");
	    oscOutput.endChangeE1Module();
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
     * Optionally skips modules which already have a mapping definition loaded into Pylades midiMap
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
		std::string text = "PYLADES";
		if (nprns[id].getNprn() >= 0) {
        	text += string::f(" FDR%03d", nprns[id].getNprn());
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
		changeE1Module(m->model->name.c_str(), m->model->getFullName().c_str(), pos.y, pos.x, maxNprnId);

		clearMaps_WithLock();
		oscOutput.reset();

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
		changeE1Module("RackMapping", "Rack Mapping", 0, 0, maxNprnId);
		clearMaps_WithLock();
		oscOutput.reset();
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
		
		// OSC connection
		json_object_set_new(rootJ, "receiving", json_boolean(receiving));
		json_object_set_new(rootJ, "sending", json_boolean(sending));
		json_object_set_new(rootJ, "ip", json_string(ip.c_str()));
		json_object_set_new(rootJ, "txPort", json_string(txPort.c_str()));
		json_object_set_new(rootJ, "rxPort", json_string(rxPort.c_str()));


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
			receiving = json_boolean_value(json_object_get(rootJ, "receiving"));
			sending = json_boolean_value(json_object_get(rootJ, "sending"));
			ip = json_string_value(json_object_get(rootJ, "ip"));
			txPort = json_string_value(json_object_get(rootJ, "txPort"));
			rxPort = json_string_value(json_object_get(rootJ, "rxPort"));
			receiverPower();
			senderPower();
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

		// No loaded midimap library file either from Pylades state or loaded preset
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

		// Only handle midimap library JSON files created by RSBATechModule
		if (!(pluginSlug == this->model->plugin->slug))
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

		INFO ("Saving mapping library to %s", filename.c_str());
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






}
}
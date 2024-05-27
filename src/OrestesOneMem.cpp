// #include "plugin.hpp"
// #include "OrestesOne.hpp"

// namespace Orestes {
// namespace OrestesOne {

// struct OrestesOneMemModule : Module {
// 	enum ParamIds {
// 		PARAM_APPLY,
// 		PARAM_PREV,
// 		PARAM_NEXT,
// 		NUM_PARAMS
// 	};
// 	enum InputIds {
// 		NUM_INPUTS
// 	};
// 	enum OutputIds {
// 		NUM_OUTPUTS
// 	};
// 	enum LightIds {
// 		LIGHT_APPLY,
// 		NUM_LIGHTS
// 	};

// 	/** [Stored to JSON] */
// 	int panelTheme = 0;
// 	/** [Stored to JSON] */
// 	std::map<std::pair<std::string, std::string>, MemModule*> midiMap;

// 	OrestesOneMemModule() {
// 		panelTheme = pluginSettings.panelThemeDefault;
// 		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
// 		configParam<BufferedTriggerParamQuantity>(PARAM_PREV, 0.f, 1.f, 0.f, "Scan for previous module mapping");
// 		configParam<BufferedTriggerParamQuantity>(PARAM_NEXT, 0.f, 1.f, 0.f, "Scan for next module mapping");
// 		configParam<BufferedTriggerParamQuantity>(PARAM_APPLY, 0.f, 1.f, 0.f, "Apply mapping");
// 		onReset();
// 	}

// 	void onReset() override {
// 		Module::onReset();
// 		resetMap();
// 	}

// 	void resetMap() {
// 		for (auto it : midiMap) {
// 			delete it.second;
// 		}
// 		midiMap.clear();
// 	}

// 	void process(const ProcessArgs& args) override {
// 		leftExpander.producerMessage = &midiMap;
// 		leftExpander.messageFlipRequested = true;
// 	}

// 	json_t* dataToJson() override {
// 		json_t *rootJ = json_object();
// 		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

// 		json_t* midiMapJ = json_array();
// 		for (auto it : midiMap) {
// 			json_t* midiMapJJ = json_object();
// 			json_object_set_new(midiMapJJ, "pluginSlug", json_string(it.first.first.c_str()));
// 			json_object_set_new(midiMapJJ, "moduleSlug", json_string(it.first.second.c_str()));

// 			auto a = it.second;
// 			json_object_set_new(midiMapJJ, "pluginName", json_string(a->pluginName.c_str()));
// 			json_object_set_new(midiMapJJ, "moduleName", json_string(a->moduleName.c_str()));
// 			json_t* paramMapJ = json_array();
// 			for (auto p : a->paramMap) {
// 				json_t* paramMapJJ = json_object();
// 				json_object_set_new(paramMapJJ, "paramId", json_integer(p->paramId));
// 				json_object_set_new(paramMapJJ, "nprn", json_integer(p->nprn));
// 				json_object_set_new(paramMapJJ, "nprnMode", json_integer((int)p->nprnMode));
// 				json_object_set_new(paramMapJJ, "label", json_string(p->label.c_str()));
// 				json_object_set_new(paramMapJJ, "midiOptions", json_integer(p->midiOptions));
// 				json_object_set_new(paramMapJJ, "slew", json_real(p->slew));
// 				json_object_set_new(paramMapJJ, "min", json_real(p->min));
// 				json_object_set_new(paramMapJJ, "max", json_real(p->max));
// 				json_array_append_new(paramMapJ, paramMapJJ);
// 			}
// 			json_object_set_new(midiMapJJ, "paramMap", paramMapJ);

// 			json_array_append_new(midiMapJ, midiMapJJ);
// 		}
// 		json_object_set_new(rootJ, "midiMap", midiMapJ);

// 		return rootJ;
// 	}

// 	void dataFromJson(json_t* rootJ) override {
// 		panelTheme = json_integer_value(json_object_get(rootJ, "panelTheme"));

// 		resetMap();
// 		json_t* midiMapJ = json_object_get(rootJ, "midiMap");
// 		size_t i;
// 		json_t* midiMapJJ;
// 		json_array_foreach(midiMapJ, i, midiMapJJ) {
// 			std::string pluginSlug = json_string_value(json_object_get(midiMapJJ, "pluginSlug"));
// 			std::string moduleSlug = json_string_value(json_object_get(midiMapJJ, "moduleSlug"));

// 			MemModule* a = new MemModule;
// 			a->pluginName = json_string_value(json_object_get(midiMapJJ, "pluginName"));
// 			a->moduleName = json_string_value(json_object_get(midiMapJJ, "moduleName"));
// 			json_t* paramMapJ = json_object_get(midiMapJJ, "paramMap");
// 			size_t j;
// 			json_t* paramMapJJ;
// 			json_array_foreach(paramMapJ, j, paramMapJJ) {
// 				MemParam* p = new MemParam;
// 				p->paramId = json_integer_value(json_object_get(paramMapJJ, "paramId"));
// 				p->nprn = json_integer_value(json_object_get(paramMapJJ, "nprn"));
// 				p->nprnMode = (NPRNMODE)json_integer_value(json_object_get(paramMapJJ, "nprnMode"));
// 				p->label = json_string_value(json_object_get(paramMapJJ, "label"));
// 				p->midiOptions = json_integer_value(json_object_get(paramMapJJ, "midiOptions"));
// 				json_t* slewJ = json_object_get(paramMapJJ, "slew");
// 				if (slewJ) p->slew = json_real_value(slewJ);
// 				json_t* minJ = json_object_get(paramMapJJ, "min");
// 				if (minJ) p->min = json_real_value(minJ);
// 				json_t* maxJ = json_object_get(paramMapJJ, "max");
// 				if (maxJ) p->max = json_real_value(maxJ);
// 				a->paramMap.push_back(p);
// 			}
// 			midiMap[std::pair<std::string, std::string>(pluginSlug, moduleSlug)] = a;
// 		}
// 	}
// };


// struct MemDisplay : OrestesLedDisplay {
// 	OrestesOneMemModule* module;
// 	void step() override {
// 		OrestesLedDisplay::step();
// 		if (!module) return;
// 		text = string::f("%i", (int)module->midiMap.size());
// 	}
// };

// struct OrestesOneMemWidget : ThemedModuleWidget<OrestesOneMemModule> {
// 	OrestesOneMemWidget(OrestesOneMemModule* module)
// 		: ThemedModuleWidget<OrestesOneMemModule>(module, "OrestesOneMem", "OrestesOne.md#mem-expander") {
// 		setModule(module);

// 		addChild(createWidget<OrestesBlackScrew>(Vec(box.size.x - RACK_GRID_WIDTH, 0)));
// 		addChild(createWidget<OrestesBlackScrew>(Vec(box.size.x - RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

// 		addChild(createParamCentered<TL1105>(Vec(15.0f, 227.1f), module, OrestesOneMemModule::PARAM_PREV));
// 		addChild(createParamCentered<TL1105>(Vec(15.0f, 258.5f), module, OrestesOneMemModule::PARAM_NEXT));
// 		addChild(createLightCentered<TinyLight<WhiteLight>>(Vec(15.f, 284.4f), module, OrestesOneMemModule::LIGHT_APPLY));
// 		addChild(createParamCentered<TL1105>(Vec(15.0f, 306.7f), module, OrestesOneMemModule::PARAM_APPLY));
// 		MemDisplay* memDisplay = createWidgetCentered<MemDisplay>(Vec(15.0f, 336.2f));
// 		memDisplay->module = module;
// 		addChild(memDisplay);
// 	}
// };

// } // namespace OrestesOne
// } // namespace Orestes

// Model* modelOrestesOneMem = createModel<Orestes::OrestesOne::OrestesOneMemModule, Orestes::OrestesOne::OrestesOneMemWidget>("OrestesOneEx");
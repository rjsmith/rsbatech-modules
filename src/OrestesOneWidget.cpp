#include "OrestesOneWidget.hpp"
#include "MapModuleBase.hpp"
#include "plugin.hpp"

namespace RSBATechModules {
namespace OrestesOne {

struct OrestesOneChoice : MapModuleChoice<MAX_CHANNELS, OrestesOneModule> {
	OrestesOneChoice() {
		textOffset = Vec(6.f, 14.7f);
		color = nvgRGB(0xf0, 0xf0, 0xf0);
	}

	std::string getSlotPrefix() override {

		if (module->nprns[id].getNprn() >= 0) {
             return string::f("nprn%03d ", module->nprns[id].getNprn());
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
				reinterpret_cast<MenuItem*>(menu->children.back())->disabled = module->midiParam[id].clockMode != RackParam::CLOCKMODE::OFF;
				menu->addChild(construct<NprnModeItem>(&MenuItem::text, "Pickup (jump)", &NprnModeItem::module, module, &NprnModeItem::id, id, &NprnModeItem::nprnMode, NPRNMODE::PICKUP2));
				reinterpret_cast<MenuItem*>(menu->children.back())->disabled = module->midiParam[id].clockMode != RackParam::CLOCKMODE::OFF;
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
					RackParam* p;
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
		MEM = 3,
		BIND_AUTOMAP = 4
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
			if (module->e1ProcessApply || expMemParamTrigger.process(module->params[OrestesOneModule::PARAM_APPLY].getValue())) {
				module->e1ProcessApply = false;
				enableLearn(LEARN_MODE::MEM);
			}
			if (module->e1ProcessApplyRackMapping) {
				module->e1ProcessApplyRackMapping = false;
				module->expMemApplyRackMapping();
			}
			module->lights[0].setBrightness(learnMode == LEARN_MODE::MEM);

			if (module->midiMapLibraryFilename.empty()) {
				// DEBUG("No known mapping library, so try and load default or factory library");
				// Load default mapping library else load and save plugin factory library
				bool midiMapLoaded = module->loadDefaultMappingLibraryFromPresetFolder();
				if (!midiMapLoaded) {
					module->createMappingLibraryFromFactory();
				}
			}

		
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
			    	// Optionally move viewport to selected module
				    if (module->scrollToModule) {
				    	RSBATechModules::Rack::ViewportCenter{mw};	
				    }

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
				// Optionally move viewport to selected module
				if (module->scrollToModule) {
				   RSBATechModules::Rack::ViewportCenter{mw};	
				}

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

	/**
	 * Sets the current mapping library file and reads a module mapping JSON file into the internal midiMap
	 */
	void expMemSelectLibrary() {

		osdialog_filters* filters = osdialog_filters_parse(LOAD_MIDIMAP_FILTERS);
		DEFER({
			osdialog_filters_free(filters);
		});

		std::string currentLibraryFilePath;
		if (!module->midiMapLibraryFilename.empty()) {
			currentLibraryFilePath = system::getDirectory(module->midiMapLibraryFilename);
		} else {
			currentLibraryFilePath = module->model->getUserPresetDirectory();
		}

		std::string currentLibraryFilename;
		if (!module->midiMapLibraryFilename.empty()) {
			currentLibraryFilename = system::getFilename(module->midiMapLibraryFilename);
		} else {
			currentLibraryFilename = DEFAULT_LIBRARY_FILENAME;
		}

		char* path = osdialog_file(OSDIALOG_OPEN, currentLibraryFilePath.c_str(), currentLibraryFilename.c_str(), filters);
		if (!path) {
			return;
		}
		DEFER({
			free(path);
		});


		loadMidiMapLibrary_action(path);
	}

	void expMemCreateLibrary() {

		osdialog_filters* filters = osdialog_filters_parse(LOAD_MIDIMAP_FILTERS);
		DEFER({
			osdialog_filters_free(filters);
		});

		std::string currentLibraryFilePath;
		if (!module->midiMapLibraryFilename.empty()) {
			currentLibraryFilePath = system::getDirectory(module->midiMapLibraryFilename);
		} else {
			currentLibraryFilePath = module->model->getUserPresetDirectory();
		}

		std::string currentLibraryFilename;
		if (!module->midiMapLibraryFilename.empty()) {
			currentLibraryFilename = system::getFilename(module->midiMapLibraryFilename);
		} else {
			currentLibraryFilename = DEFAULT_LIBRARY_FILENAME;
		}

		char* path = osdialog_file(OSDIALOG_SAVE, currentLibraryFilePath.c_str(), currentLibraryFilename.c_str(), filters);
		if (!path) {
			return;
		}
		DEFER({
			free(path);
		});

		// Update library filename
		module->midiMapLibraryFilename = path;
		module->expMemSaveLibrary(true);
	}

	void loadMidiMapLibrary_dialog() {
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

		loadMidiMapLibrary_action(path);
	}

	void loadMidiMapLibrary_action(std::string filename) {
		// DEBUG("Loading mapping library from file %s", filename.c_str());

		FILE* file = fopen(filename.c_str(), "r");
		if (!file) {
			WARN("Could not load file %s", filename.c_str());
			return;
		}
		DEFER({
			fclose(file);
		});

		json_error_t error;
		json_t* libraryJ = json_loadf(file, 0, &error);
		if (!libraryJ) {
			std::string message = string::f("File is not a valid JSON file. Parsing error at %s %d:%d %s", error.source, error.line, error.column, error.text);
			osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, message.c_str());
			return;
		}
		DEFER({
			json_decref(libraryJ);
		});

		json_t* currentStateJ = toJson();
		if (!module->loadMidiMapFromLibrary(libraryJ))
			return;

		// Update library filename
		module->midiMapLibraryFilename = filename;

		// history::ModuleChange
		history::ModuleChange* h = new history::ModuleChange;
		h->name = "select mapping library";
		h->moduleId = module->id;
		h->oldModuleJ = currentStateJ;
		h->newModuleJ = toJson();
		APP->history->push(h);
	}

	void loadMidiMapPreset_dialog(bool skipPremappedModules) {
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

		loadMidiMapPreset_action(path, skipPremappedModules);
	}

	void loadMidiMapPreset_action(std::string filename, bool skipPremappedModules) {
		INFO("Importing mappings from file %s", filename.c_str());

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
		if (mergeMidiMapPreset_convert(moduleJ, skipPremappedModules) == 0)
			return;

		// history::ModuleChange
		history::ModuleChange* h = new history::ModuleChange;
		h->name = "import mappings";
		h->moduleId = module->id;
		h->oldModuleJ = currentStateJ;
		h->newModuleJ = toJson();
		APP->history->push(h);

		module->expMemSaveLibrary();
	}

	void importFactoryMidiMapPreset_action(bool skipPremappedModules) {

        // Load factory default library
        // It is stored in the RSBATechModules plugin presets folder
        std::string factoryLibraryFilename = asset::plugin(this->model->plugin, system::join("presets", FACTORY_LIBRARY_FILENAME));


		if (!system::exists(factoryLibraryFilename)) {
			WARN("Factory library file %s does not exist, skipping", factoryLibraryFilename.c_str());
			return;
		}
		
		FILE* file = fopen(factoryLibraryFilename.c_str(), "r");
		if (!file) {
			WARN("Could not load factory library file %s, skipping", factoryLibraryFilename.c_str());
			return;
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
			return;
		}

		json_t* currentStateJ = toJson();
		if (mergeMidiMapPreset_convert(libraryJ, skipPremappedModules) == 0)
			return;

		// history::ModuleChange
		history::ModuleChange* h = new history::ModuleChange;
		h->name = "import mappings from factory library";
		h->moduleId = module->id;
		h->oldModuleJ = currentStateJ;
		h->newModuleJ = toJson();
		APP->history->push(h);

		module->expMemSaveLibrary();
	}

	/**
	 * Merge module midiMap entries from the importedPresetJ into the current midiMap of this OrestesOne module.
	 * Entries from the imported presetJ will overwrite matching entries in the existing midiMap.	 * 
	 */
	int mergeMidiMapPreset_convert(json_t* importedPresetJ, bool skipPremappedModules) {
		std::string pluginSlug = json_string_value(json_object_get(importedPresetJ, "plugin"));
		std::string modelSlug = json_string_value(json_object_get(importedPresetJ, "model"));

		// Only handle presets or midimap JSON files from OrestesOne
		if (!(pluginSlug == module->model->plugin->slug && modelSlug == module->model->slug))
			return 0;

		// Get the midiMap in the imported preset Json
		json_t* dataJ = json_object_get(importedPresetJ, "data");
		json_t* midiMapJ = json_object_get(dataJ, "midiMap");

		// Loop over the midiMap from the imported preset JSON
		size_t i;
		json_t* midiMapJJ;
		int importedModules = 0;
		json_array_foreach(midiMapJ, i, midiMapJJ) {
			std::string importedPluginSlug = json_string_value(json_object_get(midiMapJJ, "ps"));
			std::string importedModuleSlug = json_string_value(json_object_get(midiMapJJ, "ms"));

			// Find this mapped module in the current Orestes module midiMap
			auto p = std::pair<std::string, std::string>(importedPluginSlug, importedModuleSlug);
			auto it = module->midiMap.find(p);
			if (it != module->midiMap.end()) {
				if (skipPremappedModules) {
					continue;
				}
				delete it->second;
				module->midiMap.erase(p);
			}
			
			// Add new entry to midiMap
			module->midiMapJSONToMidiMap(midiMapJJ);
			importedModules++;
			
		};

		// currentStateJ* now has the updated merged midimap
		DEBUG("Imported mappings for %d modules", importedModules);
		return importedModules;
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
                            	text = string::f("MIDI NPRN %03d", module->nprns[i].getNprn());
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
					module->moduleBind(m, false, false); break;
				case LEARN_MODE::BIND_KEEP:
					module->moduleBind(m, true, false); break;
				case LEARN_MODE::BIND_AUTOMAP:
					module->moduleBind(m, false, true); 
					module->expMemSaveLibrary();
					break;	
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
					if ((e.mods & RACK_MOD_MASK) == (GLFW_MOD_SHIFT | GLFW_MOD_ALT)) {
						enableLearn(LEARN_MODE::BIND_AUTOMAP);
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
						int currentLearningId = module->learningId;
						if ((e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT) {
							module->clearMap(module->learningId, false);
						} 

						module->enableLearn(currentLearningId + 1);
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
		menu->addChild(RSBATechModules::Rack::createMapSubmenuItem<int>("Precision", {
				{ 2048, string::f("High (%i Hz)", sampleRate / 2048) },
				{ 4098, string::f("Medium (%i Hz)", sampleRate / 4098) },
				{ 8192, string::f("Low (%i Hz)", sampleRate / 8192) }
			},
			[=]() {
				return module->processDivision;
			},
			[=](int division) {
				module->setProcessDivision(division);
			}
		));
		menu->addChild(RSBATechModules::Rack::createMapSubmenuItem<MIDIMODE>("Mode", {
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
		menu->addChild(createBoolPtrMenuItem("Scroll to selected module", "", &module->scrollToModule));
		menu->addChild(createBoolPtrMenuItem("Status overlay", "", &module->overlayEnabled));
		menu->addChild(new MenuSeparator());
		menu->addChild(createSubmenuItem("Automap this rack", "",
			[=](Menu* menu) {
				menu->addChild(createMenuItem("Skip pre-mapped modules", "", [=]() { module->autoMapAllModules(true); }));
				menu->addChild(createMenuItem("Overwrite pre-mapped modules", "", [=]() { module->autoMapAllModules(false); }));
			}
		));
		menu->addChild(createSubmenuItem("Map module (select)", "",
			[=](Menu* menu) {
				menu->addChild(createMenuItem("Automap", RACK_MOD_ALT_NAME "+" RACK_MOD_SHIFT_NAME "+D", [=]() { enableLearn(LEARN_MODE::BIND_AUTOMAP); }));
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
								module->expMemSaveLibrary();
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
								module->expMemSaveLibrary();
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
								if (a->autoMapped) {
									midimapModuleItem->text = string::f("%s (A)", a->moduleName.c_str());
								} else {
									midimapModuleItem->text = string::f("%s", a->moduleName.c_str());
								}
								
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
						l[midimapPluginItem->text] = midimapPluginItem;	
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
						module->expMemSave(pluginSlug, moduleSlug, false);
						module->expMemSaveLibrary();
					}
				}; // SaveItem

				// Get unique list of modules in current parameter map
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

		menu->addChild(construct<SaveMenuItem>(&MenuItem::text, "Add module to library", &SaveMenuItem::module, module));
		menu->addChild(createMenuItem("Save rack-level mapping", "", [=]() { module->expMemSaveRackMapping(); }));

		menu->addChild(createMenuItem("Clear mappings", "", [=]() { module->clearMaps_WithLock(); }));
		menu->addChild(createMenuItem("Apply module mapping", RACK_MOD_SHIFT_NAME "+V", [=]() { enableLearn(LEARN_MODE::MEM); }));
		menu->addChild(createMenuItem("Apply rack-level mapping", "", [=]() { module->expMemApplyRackMapping(); }));

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Mapping Library"));
		menu->addChild(createBoolPtrMenuItem("Autosave changes into mapping library file", "", &module->autosaveMappingLibrary));
		menu->addChild(construct<MapMenuItem>(&MenuItem::text, "Manage library mappings", &MapMenuItem::module, module));


		menu->addChild(createSubmenuItem("Import module mappings from file", "",
			[=](Menu* menu) {
				menu->addChild(createMenuItem("Skip pre-mapped modules...", "", [=]() { loadMidiMapPreset_dialog(true); }));
				menu->addChild(createMenuItem("Overwrite pre-mapped modules...", "", [=]() { loadMidiMapPreset_dialog(false); }));
			}
		));
		menu->addChild(createSubmenuItem("Import module mappings from Factory Library", "",
			[=](Menu* menu) {
				menu->addChild(createMenuItem("Skip pre-mapped modules...", "", [=]() { importFactoryMidiMapPreset_action(true); }));
				menu->addChild(createMenuItem("Overwrite pre-mapped modules...", "", [=]() { importFactoryMidiMapPreset_action(false); }));
			}
		));

		menu->addChild(createMenuItem("Save mapping library file...", "", [=]() { expMemCreateLibrary(); }));
		menu->addChild(createMenuItem("Change mapping library file...", "", [=]() { expMemSelectLibrary(); }));

		menu->addChild(createMenuLabel(system::getFilename(module->midiMapLibraryFilename)));
		
	}
};

}  // namespace OrestesOne
}  // namespace Orestes

Model* modelOrestesOne = createModel<RSBATechModules::OrestesOne::OrestesOneModule, RSBATechModules::OrestesOne::OrestesOneWidget>("OrestesOne");
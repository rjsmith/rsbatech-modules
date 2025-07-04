#include "PyladesWidget.hpp"
#include "MapModuleBase.hpp"
#include "components/LedTextField.hpp"
#include "plugin.hpp"

namespace RSBATechModules {
namespace Pylades {

struct PyladesChoice : MapModuleChoice<MAX_CHANNELS, PyladesModule> {
	PyladesChoice() {
		textOffset = Vec(6.f, 14.7f);
		color = nvgRGB(0xf0, 0xf0, 0xf0);
	}

	std::string getSlotPrefix() override {

		if (module->nprns[id].getNprn() >= 0) {
             return string::f("FDR%03d ", module->nprns[id].getNprn());
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
			PyladesModule* module;
			int id;
			void onAction(const event::Action& e) override {
				module->clearMap(id, true);
			}
		}; // struct UnmapMidiItem

		struct NprnModeMenuItem : MenuItem {
			PyladesModule* module;
			int id;

			NprnModeMenuItem() {
				rightText = RIGHT_ARROW;
			}

			struct NprnModeItem : MenuItem {
				PyladesModule* module;
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
				reinterpret_cast<MenuItem*>(menu->children.back())->disabled = module->rackParam[id].clockMode != RackParam::CLOCKMODE::OFF;
				menu->addChild(construct<NprnModeItem>(&MenuItem::text, "Pickup (jump)", &NprnModeItem::module, module, &NprnModeItem::id, id, &NprnModeItem::nprnMode, NPRNMODE::PICKUP2));
				reinterpret_cast<MenuItem*>(menu->children.back())->disabled = module->rackParam[id].clockMode != RackParam::CLOCKMODE::OFF;
				menu->addChild(construct<NprnModeItem>(&MenuItem::text, "Toggle", &NprnModeItem::module, module, &NprnModeItem::id, id, &NprnModeItem::nprnMode, NPRNMODE::TOGGLE));
				menu->addChild(construct<NprnModeItem>(&MenuItem::text, "Toggle + Value", &NprnModeItem::module, module, &NprnModeItem::id, id, &NprnModeItem::nprnMode, NPRNMODE::TOGGLE_VALUE));
				return menu;
			}
		}; // struct NprnModeMenuItem

		if (module->nprns[id].getNprn() >= 0) {
			menu->addChild(construct<UnmapMidiItem>(&MenuItem::text, "Clear OSC assignment", &UnmapMidiItem::module, module, &UnmapMidiItem::id, id));
		}
		if (module->nprns[id].getNprn() >= 0) {
			menu->addChild(new MenuSeparator());
			menu->addChild(construct<NprnModeMenuItem>(&MenuItem::text, "Input mode for OSC", &NprnModeMenuItem::module, module, &NprnModeMenuItem::id, id));
		}

		struct PresetMenuItem : MenuItem {
			PyladesModule* module;
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
				menu->addChild(construct<PresetItem>(&MenuItem::text, "Default", &PresetItem::p, &module->rackParam[id], &PresetItem::min, 0.f, &PresetItem::max, 1.f));
				menu->addChild(construct<PresetItem>(&MenuItem::text, "Inverted", &PresetItem::p, &module->rackParam[id], &PresetItem::min, 1.f, &PresetItem::max, 0.f));
				menu->addChild(construct<PresetItem>(&MenuItem::text, "Lower 50%", &PresetItem::p, &module->rackParam[id], &PresetItem::min, 0.f, &PresetItem::max, 0.5f));
				menu->addChild(construct<PresetItem>(&MenuItem::text, "Upper 50%", &PresetItem::p, &module->rackParam[id], &PresetItem::min, 0.5f, &PresetItem::max, 1.f));
				return menu;
			}
		}; // struct PresetMenuItem

		struct LabelMenuItem : MenuItem {
			PyladesModule* module;
			int id;

			LabelMenuItem() {
				rightText = RIGHT_ARROW;
			}

			struct LabelField : ui::TextField {
				PyladesModule* module;
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
				PyladesModule* module;
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

		menu->addChild(new SlewSlider(&module->rackParam[id]));
		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Scaling"));
		std::string l = string::f("Input %s", module->nprns[id].getNprn() >= 0 ? "Value" : "");
		menu->addChild(construct<ScalingInputLabel>(&MenuLabel::text, l, &ScalingInputLabel::p, &module->rackParam[id]));
		menu->addChild(construct<ScalingOutputLabel>(&MenuLabel::text, "Parameter range", &ScalingOutputLabel::p, &module->rackParam[id]));
		menu->addChild(new MinSlider(&module->rackParam[id]));
		menu->addChild(new MaxSlider(&module->rackParam[id]));
		menu->addChild(construct<PresetMenuItem>(&MenuItem::text, "Presets", &PresetMenuItem::module, module, &PresetMenuItem::id, id));
		menu->addChild(new MenuSeparator());
		menu->addChild(construct<LabelMenuItem>(&MenuItem::text, "Custom label", &LabelMenuItem::module, module, &LabelMenuItem::id, id));

	}
};


struct OscWidget : widget::OpaqueWidget {
	PyladesModule* module;
	TheModularMind::OscelotTextField* ip;
	TheModularMind::OscelotTextField* txPort;
	TheModularMind::OscelotTextField* rxPort;
	NVGcolor color = nvgRGB(0xDA, 0xa5, 0x20);
	NVGcolor white = nvgRGB(0xfe, 0xff, 0xe0);

	void step() override {
		if (!module) return;

		ip->step();
		if (ip->isFocused)
			module->ip = ip->text;
		else
			ip->text = module->ip;

		txPort->step();
		if (txPort->isFocused)
			module->txPort = txPort->text;
		else
			txPort->text = module->txPort;

		rxPort->step();
		if (rxPort->isFocused)
			module->rxPort = rxPort->text;
		else
			rxPort->text = module->rxPort;
	}

	void setOSCPort(std::string ipT, std::string rPort, std::string tPort) {
		clearChildren();
		math::Vec pos;

		TheModularMind::OscelotTextField* ip = createWidget<TheModularMind::OscelotTextField>(pos);
		ip->box.size = mm2px(Vec(32, 5));
		ip->maxTextLength = 15;
		ip->text = ipT;
		addChild(ip);
		this->ip = ip;

		pos = ip->box.getTopRight();
		pos.x = pos.x + 1;
		TheModularMind::OscelotTextField* txPort = createWidget<TheModularMind::OscelotTextField>(pos);
		txPort->box.size = mm2px(Vec(12.5, 5));
		txPort->text = tPort;
		addChild(txPort);
		this->txPort = txPort;

		pos = txPort->box.getTopRight();
		pos.x = pos.x + 37;
		TheModularMind::OscelotTextField* rxPort = createWidget<TheModularMind::OscelotTextField>(pos);
		rxPort->box.size = mm2px(Vec(12.5, 5));
		rxPort->text = rPort;
		addChild(rxPort);
		this->rxPort = rxPort;
	}
};

struct PyladesDisplay : MapModuleDisplay<MAX_CHANNELS, PyladesModule, PyladesChoice>, OverlayMessageProvider {
	void step() override {
		if (module) {
			int mapLen = module->mapLen;
			for (int id = 0; id < MAX_CHANNELS; id++) {
				choices[id]->visible = (id < mapLen);
				separators[id]->visible = (id < mapLen);
			}
		}
		MapModuleDisplay<MAX_CHANNELS, PyladesModule, PyladesChoice>::step();
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
	PyladesModule* module;
	void step() override {
		OrestesLedDisplay::step();
		if (!module) return;
		text = string::f("%i", (int)module->midiMap.size());
	}
};

struct PyladesWidget : ThemedModuleWidget<PyladesModule>, ParamWidgetContextExtender {
	PyladesModule* module;
	PyladesDisplay* mapWidget;

	dsp::BooleanTrigger receiveTrigger;
	dsp::BooleanTrigger sendTrigger;
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

	PyladesWidget(PyladesModule* module)
		: ThemedModuleWidget<PyladesModule>(module, "Pylades") {
		setModule(module);
		this->module = module;
		box.size.x = 300;

		addChild(createWidget<OrestesBlackScrew>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<OrestesBlackScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<OrestesBlackScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<OrestesBlackScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		OscWidget* oscConfigWidget = createWidget<OscWidget>(mm2px(Vec(5.0f, 12.0f)));
		oscConfigWidget->box.size = mm2px(Vec(77, 5));
		oscConfigWidget->module = module;
		if (module) {
			oscConfigWidget->setOSCPort(module ? module->ip : NULL, module ? module->rxPort : NULL, module ? module->txPort : NULL);
		}
		addChild(oscConfigWidget);

		// Send switch
		math::Vec inpPos = mm2px(Vec(55, 14.5));
		addChild(createParamCentered<TL1105>(inpPos, module, PyladesModule::PARAM_SEND));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(inpPos, module, PyladesModule::LIGHT_SEND));

		// Receive switch
		inpPos = mm2px(Vec(80, 14.5));
		addChild(createParamCentered<TL1105>(inpPos, module, PyladesModule::PARAM_RECV));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(inpPos, module, PyladesModule::LIGHT_RECV));

		mapWidget = createWidget<PyladesDisplay>(Vec(10.0f, 75.0f));
		mapWidget->box.size = Vec(280.0f, 260.0f);
		mapWidget->setModule(module);
		addChild(mapWidget);

	

		addChild(createParamCentered<TL1105>(Vec(40.0f, 360.0f), module, PyladesModule::PARAM_PREV));
		addChild(createParamCentered<TL1105>(Vec(80.0f, 360.0f), module, PyladesModule::PARAM_NEXT));
		addChild(createLightCentered<TinyLight<WhiteLight>>(Vec(190.f, 360.f), module, PyladesModule::LIGHT_APPLY));
		addChild(createParamCentered<TL1105>(Vec(210.0f, 360.f), module, PyladesModule::PARAM_APPLY));
		
		MemDisplay* memDisplay = createWidgetCentered<MemDisplay>(Vec(260.0f, 360.f));
		memDisplay->module = module;
		addChild(memDisplay);

		if (module) {
			OverlayMessageWidget::registerProvider(mapWidget);
		}
	}

	~PyladesWidget() {
		if (learnMode != LEARN_MODE::OFF) {
			glfwSetCursor(APP->window->win, NULL);
		}

		if (module) {
			OverlayMessageWidget::unregisterProvider(mapWidget);
		}
	}


	void step() override {
		ThemedModuleWidget<PyladesModule>::step();
		if (module) {

			if (receiveTrigger.process(module->params[PyladesModule::PARAM_RECV].getValue() > 0.0f)) {
				module->receiving ^= true;
				module->receiverPower();
			}

			if (sendTrigger.process(module->params[PyladesModule::PARAM_SEND].getValue() > 0.0f)) {
				module->sending ^= true;
				module->senderPower();
			}

			// MEM
			if (module->oscProcessPrev || expMemPrevTrigger.process(module->params[PyladesModule::PARAM_PREV].getValue())) {
			    module->oscProcessPrev = false;
				expMemPrevModule();
			}
			if (module->oscProcessNext || expMemNextTrigger.process(module->params[PyladesModule::PARAM_NEXT].getValue())) {
			    module->oscProcessNext = false;
				expMemNextModule();
			}
			if (module->oscProcessSelect) {
			    module->oscProcessSelect = false;
			    expMemSelectModule(module->oscSelectedModulePos);
			    module->oscSelectedModulePos = Vec(0,0);
			}
			if (module->oscProcessApply || expMemParamTrigger.process(module->params[PyladesModule::PARAM_APPLY].getValue())) {
				module->oscProcessApply = false;
				enableLearn(LEARN_MODE::MEM);
			}
			if (module->oscProcessApplyRackMapping) {
				module->oscProcessApplyRackMapping = false;
				module->expMemApplyRackMapping();
			}
			module->lights[PyladesModule::LIGHT_APPLY].setBrightness(learnMode == LEARN_MODE::MEM);

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

	void expMemCreateNewEmptyLibrary() {

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

		std::string currentLibraryFilename = DEFAULT_LIBRARY_FILENAME;
		char* path = osdialog_file(OSDIALOG_SAVE, currentLibraryFilePath.c_str(), currentLibraryFilename.c_str(), filters);
		if (!path) {
			return;
		}
		DEFER({
			free(path);
		});

		// Update library filename
		module->midiMapLibraryFilename = path;
		module->expMemPluginDeleteAll();
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
		if (mergeMidiMapPreset_convert(moduleJ, skipPremappedModules) == 0) {
			DEBUG("No modules were imported from file");
			return;
		}

		// history::ModuleChange
		history::ModuleChange* h = new history::ModuleChange;
		h->name = "import mappings";
		h->moduleId = module->id;
		h->oldModuleJ = currentStateJ;
		h->newModuleJ = toJson();
		APP->history->push(h);

		module->expMemSaveLibrary(true);
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
	 * Merge module midiMap entries from the importedPresetJ into the current midiMap of this Pylades module.
	 * Entries from the imported presetJ will overwrite matching entries in the existing midiMap.	 * 
	 */
	int mergeMidiMapPreset_convert(json_t* importedPresetJ, bool skipPremappedModules) {
		json_t* pluginJ = json_object_get(importedPresetJ, "plugin");
        if (!pluginJ) return 0;

		std::string pluginSlug = json_string_value(pluginJ);

		// Only handle presets or midimap JSON files from RSBATechModules
		if (!(pluginSlug == module->model->plugin->slug))
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

			// Find this mapped module in the current Pylades module midiMap
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
		// DEBUG("Imported mappings for %d modules", importedModules);
		return importedModules;
	}

	void extendParamWidgetContextMenu(ParamWidget* pw, Menu* menu) override {
		if (!module) return;
		if (module->learningId >= 0) return;
		ParamQuantity* pq = pw->getParamQuantity();
		if (!pq) return;
		
		struct PyladesBeginItem : MenuLabel {
			PyladesBeginItem() {
				text = "PYLADES";
			}
		};

		struct PyladesEndItem : MenuEntry {
			PyladesEndItem() {
				box.size = Vec();
			}
		};

		struct MapMenuItem : MenuItem {
			PyladesModule* module;
			ParamQuantity* pq;
			int currentId = -1;

			MapMenuItem() {
				rightText = RIGHT_ARROW;
			}

			Menu* createChildMenu() override {
				struct MapItem : MenuItem {
					PyladesModule* module;
					int currentId;
					void onAction(const event::Action& e) override {
						module->enableLearn(currentId, true);
					}
				};

				struct MapEmptyItem : MenuItem {
					PyladesModule* module;
					ParamQuantity* pq;
					void onAction(const event::Action& e) override {
						int id = module->enableLearn(-1, true);
						if (id >= 0) module->learnParam(id, pq->module->id, pq->paramId);
					}
				};

				struct RemapItem : MenuItem {
					PyladesModule* module;
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
					menu->addChild(construct<MapEmptyItem>(&MenuItem::text, "Learn OSC", &MapEmptyItem::module, module, &MapEmptyItem::pq, pq));
				}
				else {
					menu->addChild(construct<MapItem>(&MenuItem::text, "Learn OSC", &MapItem::module, module, &MapItem::currentId, currentId));
				}

				if (module->mapLen > 0) {
					menu->addChild(new MenuSeparator);
					for (int i = 0; i < module->mapLen; i++) {
						if (module->nprns[i].getNprn() >= 0) {
							std::string text;
							if (module->nprns[i].getNprn() >= 0) {
                            	text = string::f("OSC %03d", module->nprns[i].getNprn());
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
				PyladesBeginItem* ml = dynamic_cast<PyladesBeginItem*>(*it);
				if (ml) { itCvBegin = it; continue; }
			}
			else {
				PyladesEndItem* ml = dynamic_cast<PyladesEndItem*>(*it);
				if (ml) { itCvEnd = it; break; }
			}
		}

		for (int id = 0; id < module->mapLen; id++) {
			if (module->paramHandles[id].moduleId == pq->module->id && module->paramHandles[id].paramId == pq->paramId) {
				std::string midiCatId = "";
				std::list<Widget*> w;
				w.push_back(construct<MapMenuItem>(&MenuItem::text, string::f("Re-map %s", midiCatId.c_str()), &MapMenuItem::module, module, &MapMenuItem::pq, pq, &MapMenuItem::currentId, id));
				w.push_back(new SlewSlider(&module->rackParam[id]));
				w.push_back(construct<MenuLabel>(&MenuLabel::text, "Scaling"));
				std::string l = string::f("Input %s", module->nprns[id].getNprn() >= 0 ? "OSC" : "");
				w.push_back(construct<ScalingInputLabel>(&MenuLabel::text, l, &ScalingInputLabel::p, &module->rackParam[id]));
				w.push_back(construct<ScalingOutputLabel>(&MenuLabel::text, "Parameter range", &ScalingOutputLabel::p, &module->rackParam[id]));
				w.push_back(new MinSlider(&module->rackParam[id]));
				w.push_back(new MaxSlider(&module->rackParam[id]));
				w.push_back(construct<CenterModuleItem>(&MenuItem::text, "Go to mapping module", &CenterModuleItem::mw, this));
				w.push_back(new PyladesEndItem);

				if (itCvBegin == end) {
					menu->addChild(new MenuSeparator);
					menu->addChild(construct<PyladesBeginItem>());
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

			PyladesModule* module = dynamic_cast<PyladesModule*>(this->module);
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
					PyladesModule* module = dynamic_cast<PyladesModule*>(this->module);
					disableLearn();
					module->disableLearn();
					e.consume(this);
					break;
				}
				case GLFW_KEY_SPACE: {
					if (module->learningId >= 0) {
						PyladesModule* module = dynamic_cast<PyladesModule*>(this->module);
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
		ThemedModuleWidget<PyladesModule>::onHoverKey(e);
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
		ThemedModuleWidget<PyladesModule>::appendContextMenu(menu);
		int sampleRate = int(APP->engine->getSampleRate());

		menu->addChild(new MenuSeparator());
		menu->addChild(createSubmenuItem("Preset load", "",
			[=](Menu* menu) {
				menu->addChild(createBoolPtrMenuItem("Ignore OSC devices", "", &module->oscIgnoreDevices));
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
		menu->addChild(createSubmenuItem("Re-send OSC feedback", "",
			[=](Menu* menu) {
				menu->addChild(createMenuItem("Now", "", [=]() { module->oscResendFeedback(); }));
				menu->addChild(createBoolPtrMenuItem("Periodically", "", &module->oscResendPeriodically));
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
		menu->addChild(createSubmenuItem("Automap this rack and save", "",
			[=](Menu* menu) {
				menu->addChild(createMenuItem("Skip pre-mapped modules", "", [=]() { module->autoMapAllModules(true); }));
				menu->addChild(createMenuItem("Overwrite pre-mapped modules", "", [=]() { module->autoMapAllModules(false); }));
			}
		));
		menu->addChild(createSubmenuItem("Map module (select)", "",
			[=](Menu* menu) {
				menu->addChild(createMenuItem("Automap and save", RACK_MOD_ALT_NAME "+" RACK_MOD_SHIFT_NAME "+D", [=]() { enableLearn(LEARN_MODE::BIND_AUTOMAP); }));
				menu->addChild(createMenuItem("Clear first", RACK_MOD_CTRL_NAME "+" RACK_MOD_SHIFT_NAME "+D", [=]() { enableLearn(LEARN_MODE::BIND_CLEAR); }));
				menu->addChild(createMenuItem("Keep OSC assignments", RACK_MOD_SHIFT_NAME "+D", [=]() { enableLearn(LEARN_MODE::BIND_KEEP); }));

			}
		));

		appendContextMenuMem(menu);
	}

	void appendContextMenuMem(Menu* menu) {
		PyladesModule* module = dynamic_cast<PyladesModule*>(this->module);
		assert(module);

		struct MapMenuItem : MenuItem {
			PyladesModule* module;
			MapMenuItem() {
				rightText = RIGHT_ARROW;
			}

			Menu* createChildMenu() override {
				struct MidimapModuleItem : MenuItem {
					PyladesModule* module;
					std::string pluginSlug;
					std::string moduleSlug;
					MemModule* midimapModule;
					MidimapModuleItem() {
						rightText = RIGHT_ARROW;
					}
					Menu* createChildMenu() override {
						struct DeleteItem : MenuItem {
							PyladesModule* module;
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
					PyladesModule* module;
					std::string pluginSlug;
					MidimapPluginItem() {
						rightText = RIGHT_ARROW;
					}
					// Create child menu listing all modules in this plugin
					Menu* createChildMenu() override {
						struct DeletePluginItem : MenuItem {
							PyladesModule* module;
							std::string pluginSlug;
							void onAction(const event::Action& e) override {
								module->expMemPluginDelete(pluginSlug);
								module->expMemSaveLibrary();
							}
						}; // DeletePluginItem

						struct ExportPluginItem : MenuItem {
							PyladesModule* module;
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

		struct SetPageLabelsItem : MenuItem {

			PyladesModule* module;
			SetPageLabelsItem() {
				rightText = RIGHT_ARROW;
			}

			Menu* createChildMenu() override {
				struct PageLabelMenuItem : MenuItem {
					PyladesModule* module;
					int id;

					PageLabelMenuItem() {
						rightText = RIGHT_ARROW;
					}

					struct PageLabelField : ui::TextField {
						PyladesModule* module;
						int id;
						void onSelectKey(const event::SelectKey& e) override {
							if (e.action == GLFW_PRESS && e.key == GLFW_KEY_ENTER) {
								module->pageLabels[id] = text.substr(0,20);
								ui::TextField::setText(module->pageLabels[id]);
							}

							if (!e.getTarget()) {
								ui::TextField::onSelectKey(e);
							}
						}
					};

					struct ResetItem : ui::MenuItem {
						PyladesModule* module;
						int id;
						void onAction(const event::Action& e) override {
							module->pageLabels[id] = "";
						}
					};

					Menu* createChildMenu() override {
						Menu* menu = new Menu;

						PageLabelField* labelField = new PageLabelField;
						labelField->placeholder = "Label";
						labelField->text = module->pageLabels[id];
						labelField->box.size.x = 180;
						labelField->module = module;
						labelField->id = id;
						menu->addChild(labelField);
						menu->addChild(createMenuLabel("Max 20 characters"));
						ResetItem* resetItem = new ResetItem;
						resetItem->text = "Reset";
						resetItem->module = module;
						resetItem->id = id;
						menu->addChild(resetItem);

						return menu;
					}
				}; // struct PageLabelMenuItem

				Menu* menu = new Menu;
				for (int i = 0; i < MAX_PAGES; i++) {
					std::string page = "Page ";
					page += std::to_string(i + 1);
					menu->addChild(construct<PageLabelMenuItem>(&MenuItem::text, page.c_str(), &PageLabelMenuItem::module, module, &PageLabelMenuItem::id, i));
				}
				return menu;
			}
		}; // SetPageLabelsItem
		menu->addChild(construct<SetPageLabelsItem>(&SetPageLabelsItem::text, "Set control page names", &SetPageLabelsItem::module, module));

		struct SaveMenuItem : MenuItem {
			PyladesModule* module;
			SaveMenuItem() {
				rightText = RIGHT_ARROW;
			}

			Menu* createChildMenu() override {
				struct SaveItem : MenuItem {
					PyladesModule* module;
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
					if (!m) continue;
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
		menu->addChild(new MenuSeparator());
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

		menu->addChild(createMenuItem("Save mapping library file as...", "", [=]() { expMemCreateLibrary(); }));
		menu->addChild(createMenuItem("Change mapping library file...", "", [=]() { expMemSelectLibrary(); }));
		menu->addChild(createMenuItem("Create empty mapping library file...", "", [=]() { expMemCreateNewEmptyLibrary(); }));

		menu->addChild(createMenuLabel(system::getFilename(module->midiMapLibraryFilename)));
		
	}
};

}  // namespace Pylades
}  // namespace RSBATechModules

Model* modelPylades = createModel<RSBATechModules::Pylades::PyladesModule, RSBATechModules::Pylades::PyladesWidget>("Pylades");
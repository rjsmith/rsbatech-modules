#include "plugin.hpp"

Plugin* pluginInstance;

void init(rack::Plugin* p) {
	pluginInstance = p;

	p->addModel(modelOrestesOne);
	p->addModel(modelPylades);


	pluginSettings.readFromJson();
}


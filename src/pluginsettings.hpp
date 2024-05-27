#pragma once

/*
This file was copied from https://github.com/stoermelder/vcvrack-packone
*/

struct OrestesSettings {
	int panelThemeDefault = -1;

	json_t* mbModelsJ;
	float mbV1zoom = 0.85f;
	int mbV1sort = 0;
	bool mbV1hideBrands = false;
	bool mbV1searchDescriptions = false;

	NVGcolor overlayTextColor = bndGetTheme()->menuTheme.textColor;
	int overlayHpos = 0;
	int overlayVpos = 0;
	float overlayOpacity = 1.f;
	float overlayScale = 1.f;

	void saveToJson();
	void readFromJson();
};
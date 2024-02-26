#include <iostream>
#include <fstream>

#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/cocos/cocoa/CCObject.h>
#include <Geode/ui/GeodeUI.hpp>
#include "../build/bindings/bindings/Geode/binding/GJGameLevel.hpp"

using namespace geode::prelude;
using namespace std;

AutoDeafenLevel currentlyLoadedLevel;
list<AutoDeafenLevel> loadedAutoDeafenLevels;

bool hasDeafenedThisAttempt = false;

struct AutoDeafenLevel {
	bool enabled = false;
	bool editor = false;
	int id = 0;
	float percentage = 50;
	AutoDeafenLevel(bool a, bool b, int c, float d) {
		enabled = a;
		editor = b;
		id = c;
		percentage = d;
	}
};

ghc::filesystem::path getFilePath(GJGameLevel* lvl) {

	auto path = Mod::get() -> getSaveDir();
	int id = lvl -> m_levelID.value();

	path /= (id == 0) ? ("editorlevels" + to_string(lvl -> m_M_ID)) : to_string(id);
	path /= ".autodeafen";

	return path;
}

void saveLevel(GJGameLevel* lvl, boolean enabled, boolean editor, int id, float percentage) {

	
	
	for (AutoDeafenLevel level : loadedAutoDeafenLevels) {
		if (level.id == id) {
			level.enabled = enabled;
			level.editor = editor;
			level.id = id;
			level.percentage = percentage;
			return;
		}
	}

	loadedAutoDeafenLevels.push_back(AutoDeafenLevel(enabled, editor, id, percentage));

}

void saveFile() {

	auto path = Mod::get() -> getSaveDir();
	path /= ".autodeafen";
	
	ofstream file(path);
	if (file.is_open()) {

		file.write("ad1", sizeof("ad1")); // Autodeafen file version 1
		for (AutoDeafenLevel const& a : loadedAutoDeafenLevels) {
			file.write(reinterpret_cast<const char*>(&a.enabled), sizeof(bool));
			file.write(reinterpret_cast<const char*>(&a.editor), sizeof(bool));
			file.write(reinterpret_cast<const char*>(&a.id), sizeof(int));
			file.write(reinterpret_cast<const char*>(&a.percentage), sizeof(float));
		}
		file.close();

	} else {
		log::error("AutoDeafen file failed when trying to open and save.");
	}

}


void loadFromFile(GJGameLevel* lvl) {

	auto path = getFilePath(lvl);

	if (!ghc::filesystem::exists(path)) {
		return;
	}

}



class $modify(PlayLayer) {
	
	void updateProgressbar() {

		PlayLayer::updateProgressbar();

		float percent = static_cast<float>(PlayLayer::getCurrentPercentInt());
		if (percent > currentlyLoadedLevel.percentage) {
			log::info("DEAFEN TIME!!!");
		}

	}

	void startGame() {

		PlayLayer::startGame();

		int id = m_level -> m_levelID.value();
		bool editor = (id == 0);
		if (editor) id = m_level -> m_M_ID;
		for (AutoDeafenLevel level : loadedAutoDeafenLevels)
			if (level.id == id && level.editor == editor) { currentlyLoadedLevel = level; return; }

		currentlyLoadedLevel = AutoDeafenLevel(false, editor, id, 50);

	}
	
};

class $modify(MyPauseMenuLayer, PauseLayer) {

	void customSetup() {
		PauseLayer::customSetup();
		auto winSize = CCDirector::sharedDirector() -> getWinSize();

		CCSprite* sprite = CCSprite::createWithSpriteFrameName("GJ_musicOffBtn_001.png");

		auto btn = CCMenuItemSpriteExtra::create(sprite, this, nullptr);
		auto menu = this -> getChildByID("right-button-menu");

		
	}
};

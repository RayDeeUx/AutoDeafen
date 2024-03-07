#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <vector>

#include <stdio.h>
#include <string.h>

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/LoadingLayer.hpp>
#include <Geode/cocos/cocoa/CCObject.h>
#include <Geode/binding/CCMenuItemToggler.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/binding/GJGameLevel.hpp>

#include <cocos2d.h>
#include <Geode/ui/TextInput.hpp>

// #include <geode.custom-keybinds/include/Keybinds.hpp>
// #include <../../build/geode-deps/geode.custom-keybinds/src/KeybindsLayer.hpp>

#include <custom-keybinds/Keybinds.hpp>
#include <custom-keybinds/KeybindsLayer.hpp>

using namespace geode::prelude;
using namespace std;
using namespace keybinds;

struct AutoDeafenLevel {
	bool enabled = false;
	short levelType; // 0 = Normal, 1 = Local/Editor, 2 = Daily/Weekly, 3 = gauntlet
	int id = 0;
	short percentage = 50;
	AutoDeafenLevel(bool a, short b, int c, short d) { // I am lazy lmao
		enabled = a;
		levelType = b;
		id = c;
		percentage = d;
	}
	AutoDeafenLevel() {}
};


AutoDeafenLevel currentlyLoadedLevel;
vector<AutoDeafenLevel> loadedAutoDeafenLevels;

bool hasDeafenedThisAttempt = false;
bool hasDied = false;
vector<uint32_t> deafenKeybind = {enumKeyCodes::KEY_Shift, enumKeyCodes::KEY_H};

void saveLevel(AutoDeafenLevel lvl) {

	loadedAutoDeafenLevels.push_back(lvl);

}

short getLevelType(GJGameLevel* level) {

	if (level -> m_levelType != GJLevelType::Saved) return 1;
	if (level -> m_dailyID > 0) return 2;
	if (level -> m_gauntletLevel) return 3;

	return 0;

}

void saveFile() {

	auto path = Mod::get() -> getSaveDir();
	path /= ".autodeafen";

	log::info("{}", "Saving .autodeafen file to " + path.string());

	ofstream file(path);
	if (file.is_open()) {

		file.write("ad1", sizeof("ad1")); // File Header - autodeafen file version 1
		for (AutoDeafenLevel const& a : loadedAutoDeafenLevels) {
			file.write(reinterpret_cast<const char*>(&a.enabled), sizeof(bool));
			file.write(reinterpret_cast<const char*>(&a.levelType), sizeof(short));
			file.write(reinterpret_cast<const char*>(&a.id), sizeof(int));
			file.write(reinterpret_cast<const char*>(&a.percentage), sizeof(short));
		}
		file.close();
		log::info("Successfully saved .autodeafen file.");

	} else {
		log::error("AutoDeafen file failed when trying to open and save.");
	}

}

void loadFile() {
	auto path = Mod::get() -> getSaveDir();
	path /= ".autodeafen";

	log::info("{}", "Loading .autodeafen file from " + path.string());

	ifstream file(path, std::ios::binary);
	if (file.is_open()) {

		char header[4]; // Why on earth is the length of "ad1" 4??? wtf c++
		file.read(header, sizeof("ad1"));

		if (strncmp(header, "ad1", 4) == 0) {
			log::info("Loading autodeafen file version 1.");
			while (file.good()) {
				AutoDeafenLevel level;
				file.read(reinterpret_cast<char*>(&level.enabled), sizeof(bool));
				file.read(reinterpret_cast<char*>(&level.levelType), sizeof(short));
				file.read(reinterpret_cast<char*>(&level.id), sizeof(int));
				file.read(reinterpret_cast<char*>(&level.percentage), sizeof(short));
				loadedAutoDeafenLevels.push_back(level);
				// log::debug("{} {} {} {}", level.id, level.levelType, level.enabled, level.percentage);
			}
		}

		log::info("Successfully loaded .autodeafen file.");

		file.close();

	} else {
		log::warn("AutoDeafen file failed when trying to open and load (probably just doesn't exist). Will create a new one on exit.");
	}
}

$execute {
	BindManager::get()->registerBindable({

        "deafen"_spr,
        "Discord Deafen Keybind",
        "Your discord deafen keybind.",
        { Keybind::create(KEY_H, Modifier::Shift) },
        "AutoDeafen"
    });
	log::info("Registered deafen keybind.");
}

void updateDeafenKeybind() {

	auto bindPointer = BindManager::get()->getBindable("deafen"_spr)->getDefaults()[0];
	auto bind = bindPointer.data();
	if (bind == nullptr) {
		log::error("Error occured while reloading bind (NULL)");
		return;
	}
	auto d_bind = dynamic_cast<Keybind*>(bind);

	uint32_t key = d_bind -> getKey();
	Modifier modifiers = d_bind -> getModifiers();

	bool shift = ( modifiers & Modifier::Shift   );
	bool  ctrl = ( modifiers & Modifier::Control );
	bool   alt = ( modifiers & Modifier::Alt     );

	log::info("{} {} {} {} {} {}", "Reinvoked deafen keybind with key", key, "and modifiers", shift, ctrl, alt);

	deafenKeybind.clear();
	if (shift) deafenKeybind.push_back(16);
	if  (ctrl) deafenKeybind.push_back(17);
	if   (alt) deafenKeybind.push_back(18);
	deafenKeybind.push_back(key);

	std::string str(deafenKeybind.begin(), deafenKeybind.end());
	log::info("{}{}", "Keys (update): ", str);

}

void triggerDeafenKeybind() {

	if (currentlyLoadedLevel.enabled) {
		log::info("Triggered deafen keybind.");

		std::string str(deafenKeybind.begin(), deafenKeybind.end());
		log::info("{}{}", "Keys (trigger): ", str);

		const int size = deafenKeybind.size();

		// Modifiers
		vector<uint32_t> keysPressed;

		// This is a VERY imperfect solution that presses both left and right modifier keys. I mean, hey, it works and it didn't take a lot of Windows API bs, so I ain't complaining.
		log::info("{}{}", "Size: ", size);
		for (int i = 0; i < size - 1; i++) {

			if (deafenKeybind[i] > 18 || deafenKeybind[i] < 16) {
				log::info("{}{}{}{}", "Excluded: #", i, ": ", deafenKeybind[i]);
				continue; // Modifier keys only.
			}

			// SHIFT, CTRL, ALT = 16, 17, 18 respectively
			// This works because LSHIFT, RSHIFT, LCTRL, RCTRL, LMENU, RMENU are all next to each other in the table. Pretty cool.

			const uint32_t keys[] = {
						160 + ((deafenKeybind[i] - 16) * 2),
						161 + ((deafenKeybind[i] - 16) * 2)
			};

			keybd_event(keys[0], 0, 0, 0);
			keybd_event(keys[1], 0, 0, 0);
			keysPressed.push_back(keys[0]);
			keysPressed.push_back(keys[1]);

			log::debug("{}, {}", keys[0], keys[1]);
		}

		// Discord only allows one non-modifier key, and it's always at the end! How convenient for me.
		keybd_event(deafenKeybind[size - 1], 0, 0, 0);
		keybd_event(deafenKeybind[size - 1], 0, 2, 0);

		std::string str2(keysPressed.begin(), keysPressed.end());
		log::info("{}{}", "Keys (trigger end): ", str2);
		for (uint32_t key : keysPressed)
			keybd_event(key, 0, 2, 0);

	}



}


class $modify(LoadingLayer) {
	bool init(bool p0) {
		if (!LoadingLayer::init(p0)) return false;
		loadFile();
		return true;
	}
};

class $modify(PlayerObject) {
	void playerDestroyed(bool p0) {
		if (this != nullptr) {
			auto playLayer = PlayLayer::get();
			if (playLayer != nullptr) {
				auto level = playLayer->m_level;
				if (level != nullptr) {
					
					// readability go brr
					if (	playLayer->m_player1 != nullptr &&
							this == (playLayer->m_player1) &&
							// (level->m_levelType != GJLevelType::Editor) && <- This doesn't do what I thought it did. This just disables it for local (editor/my) levels.
							!(level->isPlatformer()) &&
							!(playLayer->m_isPracticeMode)) {

						if (hasDeafenedThisAttempt && !hasDied) {
							hasDied = true;
							triggerDeafenKeybind();
						}}}}}
		PlayerObject::playerDestroyed(p0);
	}
};

class $modify(PlayLayer) {

	bool init(GJGameLevel* level, bool p1, bool p2) {

		if (!PlayLayer::init(level, p1, p2)) return false;

		updateDeafenKeybind();

		int id = m_level -> m_levelID.value();
		short levelType = getLevelType(level);
		if (levelType == 1) id = m_level -> m_M_ID;
		for (AutoDeafenLevel level : loadedAutoDeafenLevels)
			if (level.id == id && level.levelType == levelType) { currentlyLoadedLevel = level; return true; }

		currentlyLoadedLevel = AutoDeafenLevel(false, levelType, id, 50);
		hasDeafenedThisAttempt = false;

		return true;

	}

	void resetLevel() {
		PlayLayer::resetLevel();
		hasDied = false;
		hasDeafenedThisAttempt = false;
	}
	
	void updateProgressbar() {

		PlayLayer::updateProgressbar();
		if (this->m_isPracticeMode) return;

		int percent = PlayLayer::getCurrentPercentInt();
		// log::info("{}", currentlyLoadedLevel.percentage);
		// log::info("{}", percent);
		if (percent >= currentlyLoadedLevel.percentage && percent != 100 && !hasDeafenedThisAttempt) {
			hasDeafenedThisAttempt = true;
			triggerDeafenKeybind();
		}

	}

	void levelComplete() {
		PlayLayer::levelComplete();
		if (hasDeafenedThisAttempt) {
			hasDeafenedThisAttempt = false;
			triggerDeafenKeybind();
		}
	}

	void onQuit() {
		PlayLayer::onQuit();
		saveLevel(currentlyLoadedLevel);
		saveFile();
		currentlyLoadedLevel = AutoDeafenLevel();
		if (hasDeafenedThisAttempt) {
			hasDeafenedThisAttempt = false;
			triggerDeafenKeybind();
		}
	}
	
};

CCMenuItemToggler* enabledButton;
class ButtonLayer : public CCLayer {
	public:
		void toggleEnabled(CCObject* sender) {
			currentlyLoadedLevel.enabled = !currentlyLoadedLevel.enabled;
			enabledButton -> toggle(!currentlyLoadedLevel.enabled);
			// log::info("{}", currentlyLoadedLevel.enabled);
		}
		void editKeybind(CCObject*) {
			log::info("Attempting to show the enter bind layer.");
			auto bindable = BindManager::get()->getBindable("deafen"_spr);
			auto bindPointer = bindable->getDefaults()[0];
			auto bind = bindPointer.data();

			if (bindable.has_value()) {
				auto bindableValue = bindable.value();

				auto keybindsLayer = KeybindsLayer::create();
				auto bindableNode = BindableNode::create(keybindsLayer, bindableValue, 340, false);

				EnterBindLayer::create(bindableNode, bind)->show();
				log::info("Successfully shown enter bind layer.");
			} else {
				log::error("The bindable for autodeafen doesn't exist. This should never happen- if it does, please make an issue!");
			}
		}
};

TextInput* percentageInput;
class ConfigLayer : public geode::Popup<std::string const&> {
	protected:
		bool setup(std::string const& value) override {

			auto winSize = cocos2d::CCDirector::sharedDirector()->getWinSize();

			CCPoint topLeftCorner = winSize/2.f-ccp(m_size.width/2.f,-m_size.height/2.f);

			auto topLabel = CCLabelBMFont::create("AutoDeafen", "goldFont.fnt"); 
			topLabel->setAnchorPoint({0.5, 0.5});
			topLabel->setScale(1.0f);
			topLabel->setPosition(topLeftCorner + ccp(142, 5));

			auto enabledLabel = CCLabelBMFont::create("Enabled", "bigFont.fnt"); 
			enabledLabel->setAnchorPoint({0, 0.5});
			enabledLabel->setScale(0.7f);
			enabledLabel->setPosition(topLeftCorner + ccp(60, -60)); // 80 = center

			enabledButton = CCMenuItemToggler::create(
				CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png"),
				CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png"),
				this,
				menu_selector(ButtonLayer::toggleEnabled)
			);

			enabledButton -> setPosition(enabledLabel->getPosition() + ccp(140,0));
			enabledButton -> setScale(0.85f);
			enabledButton -> setClickable(true);
			enabledButton -> toggle(currentlyLoadedLevel.enabled);



			percentageInput = TextInput::create(100.f, "%");

			percentageInput -> setFilter("0123456789");
			percentageInput -> setPosition(enabledButton->getPosition() + ccp(0, -40));
			percentageInput -> setScale(0.85f);
			percentageInput -> setMaxCharCount(2);
			percentageInput -> setEnabled(true);
			percentageInput -> setString(to_string(currentlyLoadedLevel.percentage));

			auto percentageLabel = CCLabelBMFont::create("Percent", "bigFont.fnt"); 
			percentageLabel->setAnchorPoint({0, 0.5});
			percentageLabel->setScale(0.7f);
			percentageLabel->setPosition(topLeftCorner + ccp(60, -100));



			auto editKeybindButton = CCMenuItemSpriteExtra::create(
				ButtonSprite::create("Edit Keybind"),
				this, 
				menu_selector(ButtonLayer::editKeybind)
			);
			editKeybindButton->setAnchorPoint({0.5, 0.5});
			editKeybindButton->setPosition(topLeftCorner + ccp(142, -150));



			auto menu = CCMenu::create();
			menu -> setPosition( {0, 0} );

			
			menu -> addChild(enabledButton);
			menu -> addChild(percentageInput);
			menu -> addChild(editKeybindButton);

			m_mainLayer -> addChild(topLabel);
			m_mainLayer -> addChild(enabledLabel);
			m_mainLayer -> addChild(percentageLabel);

			m_mainLayer -> addChild(menu);

			return true;
		}
		void onClose(CCObject* a) override {
			Popup::onClose(a);
			currentlyLoadedLevel.percentage = stoi(percentageInput -> getString());
		}
		static ConfigLayer* create() {
			auto ret = new ConfigLayer();
			if (ret && ret->init(300, 200, "", "GJ_square02.png")) {
				ret->autorelease();
				return ret;
			}
			CC_SAFE_DELETE(ret);
			return nullptr;
		}
	public:
		void openMenu(CCObject*) {
			auto layer = create();
			layer -> show();
		}
};


class $modify(PauseLayer) {

	void customSetup() {
		PauseLayer::customSetup();
		auto winSize = CCDirector::sharedDirector() -> getWinSize();

		CCSprite* sprite = CCSprite::createWithSpriteFrameName("GJ_musicOffBtn_001.png");

		auto btn = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(ConfigLayer::openMenu) );
		auto menu = this -> getChildByID("right-button-menu");

		menu->addChild(btn);
		menu->updateLayout();

		
	}
};
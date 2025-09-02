#pragma once
#include "interpret.hpp"
#include "os.hpp"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/istreamwrapper.h"
#include <algorithm>
#include <fstream>
#include <map>
#include <string>
#include <vector>

class Input {
  public:
    struct Mouse {
        int x;
        int y;
        size_t heldFrames;
        bool isPressed;
        bool isMoving;
    };
    static Mouse mousePointer;
    static Sprite *draggingSprite;

    static std::vector<std::string> inputButtons;
    static std::map<std::string, std::string> inputControls;

    static bool isAbsolutePath(const std::string &path) {
        return path.size() > 0 && path[0] == '/';
    }

    static void applyControls(std::string controlsFilePath = "") {
        inputControls.clear();

        if (controlsFilePath != "") {
            // load controls from file
            std::ifstream file(controlsFilePath);
            if (file.is_open()) {
                Log::log("Loading controls from file: " + controlsFilePath);

                rapidjson::Document controlsJson;
                rapidjson::IStreamWrapper isw(file);
                rapidjson::ParseResult parseResult = controlsJson.ParseStream(isw);

                // Check for parse errors
                if (!parseResult) {
                    Log::logError("JSON parse error in controls file: " +
                                  std::string(rapidjson::GetParseError_En(parseResult.Code())) +
                                  " at offset " + std::to_string(parseResult.Offset()));
                    file.close();
                } else {
                    // Access the "controls" object specifically
                    if (controlsJson.HasMember("controls") && controlsJson["controls"].IsObject()) {
                        const rapidjson::Value &controls = controlsJson["controls"];

                        for (auto it = controls.MemberBegin(); it != controls.MemberEnd(); ++it) {
                            const std::string key = it->name.GetString();

                            if (it->value.IsString()) {
                                const std::string value = it->value.GetString();
                                inputControls[value] = key;
                                Log::log("Loaded control: " + key + " -> " + value);
                            }
                        }
                    }
                    file.close();
                    return;
                }
            } else {
                Log::logWarning("Failed to open controls file: " + controlsFilePath);
            }
        }

        // default controls
        inputControls["dpadUp"] = "u";
        inputControls["dpadDown"] = "h";
        inputControls["dpadLeft"] = "g";
        inputControls["dpadRight"] = "j";
        inputControls["A"] = "a";
        inputControls["B"] = "b";
        inputControls["X"] = "x";
        inputControls["Y"] = "y";
        inputControls["shoulderL"] = "l";
        inputControls["shoulderR"] = "r";
        inputControls["start"] = "1";
        inputControls["back"] = "0";
        inputControls["LeftStickRight"] = "right arrow";
        inputControls["LeftStickLeft"] = "left arrow";
        inputControls["LeftStickDown"] = "down arrow";
        inputControls["LeftStickUp"] = "up arrow";
        inputControls["LeftStickPressed"] = "c";
        inputControls["RightStickRight"] = "5";
        inputControls["RightStickLeft"] = "4";
        inputControls["RightStickDown"] = "3";
        inputControls["RightStickUp"] = "2";
        inputControls["RightStickPressed"] = "v";
        inputControls["LT"] = "z";
        inputControls["RT"] = "f";
    }

    static void buttonPress(std::string button) {
        if (inputControls.find(button) != inputControls.end()) {
            inputButtons.push_back(inputControls[button]);
        }
    }

    static bool isKeyJustPressed(std::string scratchKey) {
        return (std::find(Input::inputButtons.begin(), Input::inputButtons.end(), scratchKey) != Input::inputButtons.end()) &&
               Input::keyHeldFrames < 2;
    }

    static void doSpriteClicking() {
        if (mousePointer.isPressed) {
            mousePointer.heldFrames++;
            for (auto &sprite : sprites) {
                // click a sprite
                if (sprite->shouldDoSpriteClick) {
                    if (mousePointer.heldFrames < 2 && isColliding("mouse", sprite)) {
                        BlockExecutor::runAllBlocksByOpcode("event_whenthisspriteclicked");
                    }
                }
                // start dragging a sprite
                if (draggingSprite == nullptr && mousePointer.heldFrames < 2 && sprite->draggable && isColliding("mouse", sprite)) {
                    draggingSprite = sprite;
                }
            }
        } else {
            mousePointer.heldFrames = 0;
        }

        // move a dragging sprite
        if (draggingSprite != nullptr) {
            if (mousePointer.heldFrames == 0) {
                draggingSprite = nullptr;
                return;
            }
            draggingSprite->xPosition = mousePointer.x - (draggingSprite->spriteWidth / 2);
            draggingSprite->yPosition = mousePointer.y + (draggingSprite->spriteHeight / 2);
        }
    }

    static std::vector<int> getTouchPosition();
    static void getInput();
    static std::string getUsername();
    static int keyHeldFrames;
};

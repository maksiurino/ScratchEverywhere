#include "interpret.hpp"
#include "audio.hpp"
#include "image.hpp"
#include "input.hpp"
#include "math.hpp"
#include "miniz/miniz.h"
#include "os.hpp"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "render.hpp"
#include "sprite.hpp"
#include "unzip.hpp"
#include <cmath>
#include <cstddef>
#include <cstring>
#include <math.h>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(__WIIU__) && defined(ENABLE_CLOUDVARS)
#include <whb/sdcard.h>
#endif

#ifdef ENABLE_CLOUDVARS
#include <mist/mist.hpp>
#include <random>
#include <sstream>

const uint64_t FNV_PRIME_64 = 1099511628211ULL;
const uint64_t FNV_OFFSET_BASIS_64 = 14695981039346656037ULL;

std::string cloudUsername;

std::string projectJSON;
extern bool cloudProject;

std::unique_ptr<MistConnection> cloudConnection = nullptr;
#endif

std::vector<Sprite *> sprites;
std::vector<Sprite> spritePool;
std::vector<std::string> broadcastQueue;
std::unordered_map<std::string, Block *> blockLookup;
std::string answer;
bool toExit = false;
ProjectType projectType;

BlockExecutor executor;

int Scratch::projectWidth = 480;
int Scratch::projectHeight = 360;
int Scratch::FPS = 30;
bool Scratch::fencing = true;
bool Scratch::miscellaneousLimits = true;
bool Scratch::shouldStop = false;

#ifdef ENABLE_CLOUDVARS
bool cloudProject = false;
#endif

#ifdef ENABLE_CLOUDVARS
void initMist() {
    // Username Stuff

#ifdef __WIIU__
    std::ostringstream usernameFilenameStream;
    usernameFilenameStream << WHBGetSdCardMountPath() << "/wiiu/scratch-wiiu/cloud-username.txt";
    std::string usernameFilename = usernameFilenameStream.str();
#else
    std::string usernameFilename = "cloud-username.txt";
#endif

    std::ifstream fileStream(usernameFilename.c_str());
    if (!fileStream.good()) {
        std::random_device rd;
        std::ostringstream usernameStream;
        usernameStream << "player" << std::setw(7) << std::setfill('0') << rd() % 10000000;
        cloudUsername = usernameStream.str();
        std::ofstream usernameFile;
        usernameFile.open(usernameFilename);
        usernameFile << cloudUsername;
        usernameFile.close();
    } else {
        fileStream >> cloudUsername;
    }
    fileStream.close();

    uint64_t projectHash = FNV_OFFSET_BASIS_64;
    for (char c : projectJSON) {
        projectHash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
        projectHash *= FNV_PRIME_64;
    }

    std::ostringstream projectID;
    projectID << "Scratch-3DS/hash-" << std::hex << std::setw(16) << std::setfill('0') << projectHash;
    cloudConnection = std::make_unique<MistConnection>(projectID.str(), cloudUsername, "contact@grady.link");

    cloudConnection->onConnectionStatus([](bool connected, const std::string &message) {
        if (connected) {
            Log::log("Mist++ Connected:");
            Log::log(message);
            return;
        }
        Log::log("Mist++ Disconnected:");
        Log::log(message);
    });

    cloudConnection->onVariableUpdate(BlockExecutor::handleCloudVariableChange);

#if defined(__WIIU__) || defined(__3DS__) || defined(VITA) // These platforms require Mist++ 0.2.0 or later.
    cloudConnection->connect(false);
#else // These platforms require Mist++ 0.1.4 or later.
    cloudConnection->connect();
#endif
}
#endif

bool Scratch::startScratchProject() {
#ifdef ENABLE_CLOUDVARS
    if (cloudProject && !projectJSON.empty()) initMist();
#endif

    BlockExecutor::runAllBlocksByOpcode("event_whenflagclicked");
    BlockExecutor::timer.start();

    while (Render::appShouldRun()) {
        if (Render::checkFramerate()) {
            Input::getInput();
            BlockExecutor::runRepeatBlocks();
            BlockExecutor::runBroadcasts();
            Render::renderSprites();

            if (shouldStop) {
#ifdef __WIIU__ // wii u freezes for some reason.. TODO fix that but for now just exit app
                toExit = true;
                return false;
#endif
                if (projectType != UNEMBEDDED) {
                    toExit = true;
                    return false;
                }
                cleanupScratchProject();
                shouldStop = false;
                return true;
            }
        }
    }
    return false;
}

void Scratch::cleanupScratchProject() {
    cleanupSprites();
    Image::cleanupImages();
    SoundPlayer::cleanupAudio();
    blockLookup.clear();
    Render::visibleVariables.clear();

    // Clean up ZIP archive if it was initialized
    if (projectType != UNZIPPED) {
        mz_zip_reader_end(&Unzip::zipArchive);

        // Clear the ZIP buffer and deallocate its memory
        size_t bufferSize = Unzip::zipBuffer.size();
        Unzip::zipBuffer.clear();
        Unzip::zipBuffer.shrink_to_fit();

        // Update memory tracker for the buffer
        if (bufferSize > 0) {
            MemoryTracker::deallocate(nullptr, bufferSize);
        }

        memset(&Unzip::zipArchive, 0, sizeof(Unzip::zipArchive));
    }

#ifdef ENABLE_CLOUDVARS
    projectJSON.clear();
    projectJSON.shrink_to_fit();
#endif

    // reset default settings
    Scratch::FPS = 30;
    Scratch::projectWidth = 480;
    Scratch::projectHeight = 360;
    Scratch::fencing = true;
    Scratch::miscellaneousLimits = true;
    Render::renderMode = Render::TOP_SCREEN_ONLY;
    Unzip::filePath = "";
    Log::log("Cleaned up Scratch project.");
}

void initializeSpritePool(int poolSize) {
    for (int i = 0; i < poolSize; i++) {
        Sprite newSprite;
        newSprite.id = Math::generateRandomString(15);
        newSprite.isClone = true;
        newSprite.toDelete = true;
        newSprite.isDeleted = true;
        spritePool.push_back(newSprite);
    }
}

Sprite *getAvailableSprite() {
    for (Sprite &sprite : spritePool) {
        if (sprite.isDeleted) {
            sprite.isDeleted = false;
            sprite.toDelete = false;
            return &sprite;
        }
    }
    return nullptr;
}

void cleanupSprites() {
    for (Sprite *sprite : sprites) {
        if (sprite) {
            if (sprite->isClone) {
                sprite->isDeleted = true;
            } else {
                sprite->~Sprite();
                MemoryTracker::deallocate<Sprite>(sprite);
            }
        }
    }
    sprites.clear();
    spritePool.clear();
}

std::vector<std::pair<double, double>> getCollisionPoints(Sprite *currentSprite) {
    std::vector<std::pair<double, double>> collisionPoints;

    // Get sprite dimensions, scaled by size
    double halfWidth = (currentSprite->spriteWidth * currentSprite->size / 100.0) / 2.0;
    double halfHeight = (currentSprite->spriteHeight * currentSprite->size / 100.0) / 2.0;

    // Calculate rotation in radians
    double rotation = currentSprite->rotation;

    if (currentSprite->rotationStyle == currentSprite->NONE) rotation = 90;
    if (currentSprite->rotationStyle == currentSprite->LEFT_RIGHT) {
        if (currentSprite->rotation > 0)
            rotation = 90;
        else
            rotation = -90;
    }

    double rotationRadians = (rotation - 90) * M_PI / 180.0;
    double rotationCenterX = ((currentSprite->rotationCenterX - currentSprite->spriteWidth) * 0.75);
    double rotationCenterY = ((currentSprite->rotationCenterY - currentSprite->spriteHeight) * 0.75);

    // Define the four corners relative to the sprite's center
    std::vector<std::pair<double, double>> corners = {
        {-halfWidth - (rotationCenterX * currentSprite->size * 0.01), -halfHeight + (rotationCenterY)}, // Top-left
        {halfWidth - (rotationCenterX * currentSprite->size * 0.01), -halfHeight + (rotationCenterY)},  // Top-right
        {halfWidth - (rotationCenterX * currentSprite->size * 0.01), halfHeight + (rotationCenterY)},   // Bottom-right
        {-halfWidth - (rotationCenterX * currentSprite->size * 0.01), halfHeight + (rotationCenterY)}   // Bottom-left
    };

    // Rotate and translate each corner
    for (const auto &corner : corners) {
        double rotatedX = corner.first * cos(rotationRadians) - corner.second * sin(rotationRadians);
        double rotatedY = corner.first * sin(rotationRadians) + corner.second * cos(rotationRadians);

        collisionPoints.emplace_back(
            currentSprite->xPosition + rotatedX,
            currentSprite->yPosition + rotatedY);
    }

    return collisionPoints;
}

bool isSeparated(const std::vector<std::pair<double, double>> &poly1,
                 const std::vector<std::pair<double, double>> &poly2,
                 double axisX, double axisY) {
    double min1 = 1e9, max1 = -1e9;
    double min2 = 1e9, max2 = -1e9;

    // Project poly1 onto axis
    for (const auto &point : poly1) {
        double projection = point.first * axisX + point.second * axisY;
        min1 = std::min(min1, projection);
        max1 = std::max(max1, projection);
    }

    // Project poly2 onto axis
    for (const auto &point : poly2) {
        double projection = point.first * axisX + point.second * axisY;
        min2 = std::min(min2, projection);
        max2 = std::max(max2, projection);
    }

    return max1 < min2 || max2 < min1;
}

bool isColliding(std::string collisionType, Sprite *currentSprite, Sprite *targetSprite, std::string targetName) {
    // Get collision points of the current sprite
    std::vector<std::pair<double, double>> currentSpritePoints = getCollisionPoints(currentSprite);

    if (collisionType == "mouse") {
        // Define a small square centered on the mouse pointer
        double halfWidth = 0.5;
        double halfHeight = 0.5;

        std::vector<std::pair<double, double>> mousePoints = {
            {Input::mousePointer.x - halfWidth, Input::mousePointer.y - halfHeight}, // Top-left
            {Input::mousePointer.x + halfWidth, Input::mousePointer.y - halfHeight}, // Top-right
            {Input::mousePointer.x + halfWidth, Input::mousePointer.y + halfHeight}, // Bottom-right
            {Input::mousePointer.x - halfWidth, Input::mousePointer.y + halfHeight}  // Bottom-left
        };

        bool collision = true;

        for (int i = 0; i < 4; i++) {
            auto edge1 = std::make_pair(
                currentSpritePoints[(i + 1) % 4].first - currentSpritePoints[i].first,
                currentSpritePoints[(i + 1) % 4].second - currentSpritePoints[i].second);
            auto edge2 = std::make_pair(
                mousePoints[(i + 1) % 4].first - mousePoints[i].first,
                mousePoints[(i + 1) % 4].second - mousePoints[i].second);

            double axis1X = -edge1.second, axis1Y = edge1.first;
            double axis2X = -edge2.second, axis2Y = edge2.first;

            double len1 = sqrt(axis1X * axis1X + axis1Y * axis1Y);
            double len2 = sqrt(axis2X * axis2X + axis2Y * axis2Y);
            if (len1 > 0) {
                axis1X /= len1;
                axis1Y /= len1;
            }
            if (len2 > 0) {
                axis2X /= len2;
                axis2Y /= len2;
            }

            if (isSeparated(currentSpritePoints, mousePoints, axis1X, axis1Y) ||
                isSeparated(currentSpritePoints, mousePoints, axis2X, axis2Y)) {
                collision = false;
                break;
            }
        }

        return collision;
    } else if (collisionType == "edge") {
        double halfWidth = Scratch::projectWidth / 2.0;
        double halfHeight = Scratch::projectHeight / 2.0;

        // Check if the current sprite is touching the edge of the screen
        if (currentSprite->xPosition <= -halfWidth || currentSprite->xPosition >= halfWidth ||
            currentSprite->yPosition <= -halfHeight || currentSprite->yPosition >= halfHeight) {
            return true;
        }
        return false;
    } else if (collisionType == "sprite") {
        // Use targetSprite if provided, otherwise search by name
        if (targetSprite == nullptr && !targetName.empty()) {
            for (Sprite *sprite : sprites) {
                if (sprite->name == targetName && sprite->visible) {
                    targetSprite = sprite;
                    break;
                }
            }
        }

        if (targetSprite == nullptr || !targetSprite->visible) {
            return false;
        }

        std::vector<std::pair<double, double>> targetSpritePoints = getCollisionPoints(targetSprite);

        // Check if any point of current sprite is inside target sprite
        for (const auto &currentPoint : currentSpritePoints) {
            double x = currentPoint.first;
            double y = currentPoint.second;

            // Ray casting to check if point is inside target sprite
            int intersections = 0;
            for (int i = 0; i < 4; i++) {
                int j = (i + 1) % 4;
                double x1 = targetSpritePoints[i].first, y1 = targetSpritePoints[i].second;
                double x2 = targetSpritePoints[j].first, y2 = targetSpritePoints[j].second;

                if (((y1 > y) != (y2 > y)) &&
                    (x < (x2 - x1) * (y - y1) / (y2 - y1) + x1)) {
                    intersections++;
                }
            }

            if ((intersections % 2) == 1) {
                return true;
            }
        }

        // Check if any point of target sprite is inside current sprite
        for (const auto &targetPoint : targetSpritePoints) {
            double x = targetPoint.first;
            double y = targetPoint.second;

            // Ray casting to check if point is inside current sprite
            int intersections = 0;
            for (int i = 0; i < 4; i++) {
                int j = (i + 1) % 4;
                double x1 = currentSpritePoints[i].first, y1 = currentSpritePoints[i].second;
                double x2 = currentSpritePoints[j].first, y2 = currentSpritePoints[j].second;

                if (((y1 > y) != (y2 > y)) &&
                    (x < (x2 - x1) * (y - y1) / (y2 - y1) + x1)) {
                    intersections++;
                }
            }

            if ((intersections % 2) == 1) {
                return true;
            }
        }
    }

    return false;
}

void Scratch::fenceSpriteWithinBounds(Sprite *sprite) {
    double halfWidth = Scratch::projectWidth / 2.0;
    double halfHeight = Scratch::projectHeight / 2.0;
    double scale = sprite->size / 100.0;
    double spriteHalfWidth = (sprite->spriteWidth * scale) / 2.0;
    double spriteHalfHeight = (sprite->spriteHeight * scale) / 2.0;

    // how much of the sprite remains visible when fenced
    const double sliverSize = 5.0;

    double maxLeft = halfWidth - sliverSize;
    double minRight = -halfWidth + sliverSize;
    double maxBottom = halfHeight - sliverSize;
    double minTop = -halfHeight + sliverSize;

    if (sprite->xPosition - spriteHalfWidth > maxLeft) {
        sprite->xPosition = maxLeft + spriteHalfWidth;
    }
    if (sprite->xPosition + spriteHalfWidth < minRight) {
        sprite->xPosition = minRight - spriteHalfWidth;
    }
    if (sprite->yPosition - spriteHalfHeight > maxBottom) {
        sprite->yPosition = maxBottom + spriteHalfHeight;
    }
    if (sprite->yPosition + spriteHalfHeight < minTop) {
        sprite->yPosition = minTop - spriteHalfHeight;
    }
}

void loadSprites(const rapidjson::Document &json) {
    Log::log("beginning to load sprites...");
    sprites.reserve(400);

    const rapidjson::Value &targets = json["targets"];
    for (rapidjson::SizeType i = 0; i < targets.Size(); i++) {
        const rapidjson::Value &target = targets[i];

        Sprite *newSprite = MemoryTracker::allocate<Sprite>();
        new (newSprite) Sprite();

        if (target.HasMember("name") && target["name"].IsString()) {
            newSprite->name = target["name"].GetString();
        }
        newSprite->id = Math::generateRandomString(15);

        if (target.HasMember("isStage") && target["isStage"].IsBool()) {
            newSprite->isStage = target["isStage"].GetBool();
        }
        if (target.HasMember("draggable") && target["draggable"].IsBool()) {
            newSprite->draggable = target["draggable"].GetBool();
        }
        if (target.HasMember("visible") && target["visible"].IsBool()) {
            newSprite->visible = target["visible"].GetBool();
        } else {
            newSprite->visible = true;
        }
        if (target.HasMember("currentCostume") && target["currentCostume"].IsInt()) {
            newSprite->currentCostume = target["currentCostume"].GetInt();
        }
        if (target.HasMember("volume") && target["volume"].IsInt()) {
            newSprite->volume = target["volume"].GetInt();
        }
        if (target.HasMember("x") && target["x"].IsInt()) {
            newSprite->xPosition = target["x"].GetInt();
        }
        if (target.HasMember("y") && target["y"].IsInt()) {
            newSprite->yPosition = target["y"].GetInt();
        }
        if (target.HasMember("size") && target["size"].IsInt()) {
            newSprite->size = target["size"].GetInt();
        } else {
            newSprite->size = 100;
        }
        if (target.HasMember("direction") && target["direction"].IsInt()) {
            newSprite->rotation = target["direction"].GetInt();
        } else {
            newSprite->rotation = 90;
        }
        if (target.HasMember("layerOrder") && target["layerOrder"].IsInt()) {
            newSprite->layer = target["layerOrder"].GetInt();
        } else {
            newSprite->layer = 0;
        }
        if (target.HasMember("rotationStyle") && target["rotationStyle"].IsString()) {
            std::string rotStyle = target["rotationStyle"].GetString();
            if (rotStyle == "all around")
                newSprite->rotationStyle = newSprite->ALL_AROUND;
            else if (rotStyle == "left-right")
                newSprite->rotationStyle = newSprite->LEFT_RIGHT;
            else
                newSprite->rotationStyle = newSprite->NONE;
        }
        newSprite->toDelete = false;
        newSprite->isClone = false;

        // set variables
        if (target.HasMember("variables") && target["variables"].IsObject()) {
            const rapidjson::Value &variables = target["variables"];
            for (auto it = variables.MemberBegin(); it != variables.MemberEnd(); ++it) {
                const std::string id = it->name.GetString();
                const rapidjson::Value &data = it->value;

                Variable newVariable;
                newVariable.id = id;
                if (data.IsArray() && data.Size() >= 2) {
                    if (data[0].IsString()) {
                        newVariable.name = data[0].GetString();
                    }
                    newVariable.value = Value::fromJson(data[1]);
#ifdef ENABLE_CLOUDVARS
                    newVariable.cloud = data.Size() == 3;
                    cloudProject = cloudProject || newVariable.cloud;
#endif
                }
                newSprite->variables[newVariable.id] = newVariable;
            }
        }

        // set Blocks
        if (target.HasMember("blocks") && target["blocks"].IsObject()) {
            const rapidjson::Value &blocks = target["blocks"];
            for (auto it = blocks.MemberBegin(); it != blocks.MemberEnd(); ++it) {
                const std::string id = it->name.GetString();
                const rapidjson::Value &data = it->value;

                Block newBlock;
                newBlock.id = id;

                if (data.IsObject() && data.HasMember("opcode") && data["opcode"].IsString()) {
                    newBlock.opcode = data["opcode"].GetString();
                    if (newBlock.opcode == "event_whenthisspriteclicked") {
                        newSprite->shouldDoSpriteClick = true;
                    }
                }

                if (data.IsObject() && data.HasMember("next") && !data["next"].IsNull() && data["next"].IsString()) {
                    newBlock.next = data["next"].GetString();
                }

                if (data.IsObject() && data.HasMember("parent") && !data["parent"].IsNull() && data["parent"].IsString()) {
                    newBlock.parent = data["parent"].GetString();
                } else {
                    newBlock.parent = "null";
                }

                if (data.IsObject() && data.HasMember("fields") && data["fields"].IsObject()) {
                    const rapidjson::Value &fields = data["fields"];
                    for (auto fieldIt = fields.MemberBegin(); fieldIt != fields.MemberEnd(); ++fieldIt) {
                        const std::string fieldName = fieldIt->name.GetString();
                        const rapidjson::Value &fieldData = fieldIt->value;

                        ParsedField parsedField;

                        if (fieldData.IsArray() && fieldData.Size() > 0) {
                            if (fieldData[0].IsString()) {
                                parsedField.value = fieldData[0].GetString();
                            }

                            if (fieldData.Size() > 1 && !fieldData[1].IsNull() && fieldData[1].IsString()) {
                                parsedField.id = fieldData[1].GetString();
                            }
                        }

                        newBlock.parsedFields[fieldName] = parsedField;
                    }
                }

                if (data.IsObject() && data.HasMember("inputs") && data["inputs"].IsObject()) {
                    const rapidjson::Value &inputs = data["inputs"];
                    for (auto inputIt = inputs.MemberBegin(); inputIt != inputs.MemberEnd(); ++inputIt) {
                        const std::string inputName = inputIt->name.GetString();
                        const rapidjson::Value &inputData = inputIt->value;

                        ParsedInput parsedInput;

                        if (inputData.IsArray() && inputData.Size() >= 2) {
                            int type = inputData[0].GetInt();
                            const rapidjson::Value &inputValue = inputData[1];

                            if (type == 1) {
                                parsedInput.inputType = ParsedInput::LITERAL;
                                parsedInput.literalValue = Value::fromJson(inputValue);
                            } else if (type == 3) {
                                if (inputValue.IsArray()) {
                                    parsedInput.inputType = ParsedInput::VARIABLE;
                                    if (inputValue.Size() > 2 && inputValue[2].IsString()) {
                                        parsedInput.variableId = inputValue[2].GetString();
                                    }
                                } else {
                                    parsedInput.inputType = ParsedInput::BLOCK;
                                    if (!inputValue.IsNull() && inputValue.IsString()) {
                                        parsedInput.blockId = inputValue.GetString();
                                    }
                                }
                            } else if (type == 2) {
                                parsedInput.inputType = ParsedInput::BOOLEAN;
                                if (inputValue.IsString()) {
                                    parsedInput.blockId = inputValue.GetString();
                                }
                            }
                        }

                        newBlock.parsedInputs[inputName] = parsedInput;
                    }
                }

                if (data.IsObject() && data.HasMember("topLevel") && data["topLevel"].IsBool()) {
                    newBlock.topLevel = data["topLevel"].GetBool();
                }
                if (data.IsObject() && data.HasMember("shadow") && data["shadow"].IsBool()) {
                    newBlock.shadow = data["shadow"].GetBool();
                }

                if (data.IsObject() && data.HasMember("mutation") && data["mutation"].IsObject()) {
                    const rapidjson::Value &mutation = data["mutation"];
                    if (mutation.HasMember("proccode") && mutation["proccode"].IsString()) {
                        newBlock.customBlockId = mutation["proccode"].GetString();
                    } else {
                        newBlock.customBlockId = "";
                    }
                }

                newSprite->blocks[newBlock.id] = newBlock;

                // add custom function blocks
                if (newBlock.opcode == "procedures_prototype") {
                    if (!data.IsArray()) {
                        CustomBlock newCustomBlock;

                        if (data.IsObject() && data.HasMember("mutation") && data["mutation"].IsObject()) {
                            const rapidjson::Value &mutation = data["mutation"];

                            if (mutation.HasMember("proccode") && mutation["proccode"].IsString()) {
                                newCustomBlock.name = mutation["proccode"].GetString();
                            }
                            newCustomBlock.blockId = newBlock.id;

                            // Parse argument names
                            if (mutation.HasMember("argumentnames") && mutation["argumentnames"].IsString()) {
                                std::string rawArgumentNames = mutation["argumentnames"].GetString();
                                rapidjson::Document parsedAN;
                                if (!parsedAN.Parse(rawArgumentNames.c_str()).HasParseError() && parsedAN.IsArray()) {
                                    for (rapidjson::SizeType j = 0; j < parsedAN.Size(); j++) {
                                        if (parsedAN[j].IsString()) {
                                            newCustomBlock.argumentNames.push_back(parsedAN[j].GetString());
                                        }
                                    }
                                }
                            }

                            // Parse argument defaults
                            if (mutation.HasMember("argumentdefaults") && mutation["argumentdefaults"].IsString()) {
                                std::string rawArgumentDefaults = mutation["argumentdefaults"].GetString();
                                rapidjson::Document parsedAD;
                                if (!parsedAD.Parse(rawArgumentDefaults.c_str()).HasParseError() && parsedAD.IsArray()) {
                                    for (rapidjson::SizeType j = 0; j < parsedAD.Size(); j++) {
                                        const rapidjson::Value &item = parsedAD[j];
                                        if (item.IsString()) {
                                            newCustomBlock.argumentDefaults.push_back(item.GetString());
                                        } else if (item.IsInt()) {
                                            newCustomBlock.argumentDefaults.push_back(std::to_string(item.GetInt()));
                                        } else if (item.IsDouble()) {
                                            newCustomBlock.argumentDefaults.push_back(std::to_string(item.GetDouble()));
                                        } else {
                                            // For complex types, convert to string representation
                                            rapidjson::StringBuffer buffer;
                                            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                                            item.Accept(writer);
                                            newCustomBlock.argumentDefaults.push_back(buffer.GetString());
                                        }
                                    }
                                }
                            }

                            // Parse argument IDs
                            if (mutation.HasMember("argumentids") && mutation["argumentids"].IsString()) {
                                std::string rawArgumentIds = mutation["argumentids"].GetString();
                                rapidjson::Document parsedAID;
                                if (!parsedAID.Parse(rawArgumentIds.c_str()).HasParseError() && parsedAID.IsArray()) {
                                    for (rapidjson::SizeType j = 0; j < parsedAID.Size(); j++) {
                                        if (parsedAID[j].IsString()) {
                                            newCustomBlock.argumentIds.push_back(parsedAID[j].GetString());
                                        }
                                    }
                                }
                            }

                            if (mutation.HasMember("warp") && mutation["warp"].IsString()) {
                                newCustomBlock.runWithoutScreenRefresh = (std::string(mutation["warp"].GetString()) == "true");
                            } else {
                                newCustomBlock.runWithoutScreenRefresh = false;
                            }
                        }

                        newSprite->customBlocks[newCustomBlock.name] = newCustomBlock;
                    } else {
                        // Convert data to string for logging
                        rapidjson::StringBuffer buffer;
                        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                        data.Accept(writer);
                        Log::logError("Unknown Custom block data: " + std::string(buffer.GetString()));
                    }
                }
            }
        }

        // set Lists
        if (target.HasMember("lists") && target["lists"].IsObject()) {
            const rapidjson::Value &lists = target["lists"];
            for (auto it = lists.MemberBegin(); it != lists.MemberEnd(); ++it) {
                const std::string id = it->name.GetString();
                const rapidjson::Value &data = it->value;

                List newList;
                newList.id = id;
                if (data.IsArray() && data.Size() >= 2) {
                    if (data[0].IsString()) {
                        newList.name = data[0].GetString();
                    }
                    if (data[1].IsArray()) {
                        const rapidjson::Value &listItems = data[1];
                        for (rapidjson::SizeType j = 0; j < listItems.Size(); j++) {
                            newList.items.push_back(Value::fromJson(listItems[j]));
                        }
                    }
                }
                newSprite->lists[newList.id] = newList;
            }
        }

        // set Sounds
        if (target.HasMember("sounds") && target["sounds"].IsArray()) {
            const rapidjson::Value &sounds = target["sounds"];
            for (rapidjson::SizeType i = 0; i < sounds.Size(); i++) {
                const rapidjson::Value &data = sounds[i];

                Sound newSound;
                if (data.HasMember("assetId") && data["assetId"].IsString()) {
                    newSound.id = data["assetId"].GetString();
                }
                if (data.HasMember("name") && data["name"].IsString()) {
                    newSound.name = data["name"].GetString();
                }
                if (data.HasMember("md5ext") && data["md5ext"].IsString()) {
                    newSound.fullName = data["md5ext"].GetString();
                }
                if (data.HasMember("dataFormat") && data["dataFormat"].IsString()) {
                    newSound.dataFormat = data["dataFormat"].GetString();
                }
                if (data.HasMember("rate") && data["rate"].IsInt()) {
                    newSound.sampleRate = data["rate"].GetInt();
                }
                if (data.HasMember("sampleCount") && data["sampleCount"].IsInt()) {
                    newSound.sampleCount = data["sampleCount"].GetInt();
                }
                newSprite->sounds[newSound.name] = newSound;
            }
        }

        // set Costumes
        if (target.HasMember("costumes") && target["costumes"].IsArray()) {
            const rapidjson::Value &costumes = target["costumes"];
            for (rapidjson::SizeType i = 0; i < costumes.Size(); i++) {
                const rapidjson::Value &data = costumes[i];

                Costume newCostume;
                if (data.HasMember("assetId") && data["assetId"].IsString()) {
                    newCostume.id = data["assetId"].GetString();
                }
                if (data.HasMember("name") && data["name"].IsString()) {
                    newCostume.name = data["name"].GetString();
                }
                if (data.HasMember("bitmapResolution") && data["bitmapResolution"].IsNumber()) {
                    newCostume.bitmapResolution = data["bitmapResolution"].GetDouble();
                }
                if (data.HasMember("dataFormat") && data["dataFormat"].IsString()) {
                    newCostume.dataFormat = data["dataFormat"].GetString();
                }
                if (data.HasMember("md5ext") && data["md5ext"].IsString()) {
                    newCostume.fullName = data["md5ext"].GetString();
                }
                if (data.HasMember("rotationCenterX") && data["rotationCenterX"].IsNumber()) {
                    newCostume.rotationCenterX = data["rotationCenterX"].GetDouble();
                }
                if (data.HasMember("rotationCenterY") && data["rotationCenterY"].IsNumber()) {
                    newCostume.rotationCenterY = data["rotationCenterY"].GetDouble();
                }
                newSprite->costumes.push_back(newCostume);
            }
        }

        // set comments
        if (target.HasMember("comments") && target["comments"].IsObject()) {
            const rapidjson::Value &comments = target["comments"];
            for (auto it = comments.MemberBegin(); it != comments.MemberEnd(); ++it) {
                const std::string id = it->name.GetString();
                const rapidjson::Value &data = it->value;

                Comment newComment;
                newComment.id = id;
                if (data.HasMember("blockId") && !data["blockId"].IsNull() && data["blockId"].IsString()) {
                    newComment.blockId = data["blockId"].GetString();
                }
                if (data.HasMember("width") && data["width"].IsNumber()) {
                    newComment.width = data["width"].GetDouble();
                }
                if (data.HasMember("height") && data["height"].IsNumber()) {
                    newComment.height = data["height"].GetDouble();
                }
                if (data.HasMember("minimized") && data["minimized"].IsBool()) {
                    newComment.minimized = data["minimized"].GetBool();
                }
                if (data.HasMember("x") && data["x"].IsNumber()) {
                    newComment.x = data["x"].GetDouble();
                }
                if (data.HasMember("y") && data["y"].IsNumber()) {
                    newComment.y = data["y"].GetDouble();
                }
                if (data.HasMember("text") && data["text"].IsString()) {
                    newComment.text = data["text"].GetString();
                }
                newSprite->comments[newComment.id] = newComment;
            }
        }

        // set Broadcasts
        if (target.HasMember("broadcasts") && target["broadcasts"].IsObject()) {
            const rapidjson::Value &broadcasts = target["broadcasts"];
            for (auto it = broadcasts.MemberBegin(); it != broadcasts.MemberEnd(); ++it) {
                const std::string id = it->name.GetString();
                const rapidjson::Value &data = it->value;

                Broadcast newBroadcast;
                newBroadcast.id = id;
                if (data.IsString()) {
                    newBroadcast.name = data.GetString();
                }
                newSprite->broadcasts[newBroadcast.id] = newBroadcast;
            }
        }

        sprites.push_back(newSprite);
    }

    // Handle monitors
    if (json.HasMember("monitors") && json["monitors"].IsArray()) {
        const rapidjson::Value &monitors = json["monitors"];
        for (rapidjson::SizeType i = 0; i < monitors.Size(); i++) {
            const rapidjson::Value &monitor = monitors[i];

            Monitor newMonitor;

            if (monitor.HasMember("id") && !monitor["id"].IsNull() && monitor["id"].IsString())
                newMonitor.id = monitor["id"].GetString();

            if (monitor.HasMember("mode") && !monitor["mode"].IsNull() && monitor["mode"].IsString())
                newMonitor.mode = monitor["mode"].GetString();

            if (monitor.HasMember("opcode") && !monitor["opcode"].IsNull() && monitor["opcode"].IsString())
                newMonitor.opcode = monitor["opcode"].GetString();

            if (monitor.HasMember("params") && monitor["params"].IsObject()) {
                const rapidjson::Value &params = monitor["params"];
                for (auto it = params.MemberBegin(); it != params.MemberEnd(); ++it) {
                    std::string key = it->name.GetString();

                    // Convert value to string
                    rapidjson::StringBuffer buffer;
                    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                    it->value.Accept(writer);
                    std::string value = buffer.GetString();

                    newMonitor.parameters[key] = value;
                }
            }

            if (monitor.HasMember("spriteName") && !monitor["spriteName"].IsNull() && monitor["spriteName"].IsString())
                newMonitor.spriteName = monitor["spriteName"].GetString();
            else
                newMonitor.spriteName = "";

            if (monitor.HasMember("value") && !monitor["value"].IsNull()) {
                rapidjson::StringBuffer buffer;
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                monitor["value"].Accept(writer);
                newMonitor.value = Value(Math::removeQuotations(buffer.GetString()));
            }

            if (monitor.HasMember("x") && !monitor["x"].IsNull() && monitor["x"].IsInt())
                newMonitor.x = monitor["x"].GetInt();

            if (monitor.HasMember("y") && !monitor["y"].IsNull() && monitor["y"].IsInt())
                newMonitor.y = monitor["y"].GetInt();

            if (monitor.HasMember("visible") && !monitor["visible"].IsNull() && monitor["visible"].IsBool())
                newMonitor.visible = monitor["visible"].GetBool();

            if (monitor.HasMember("isDiscrete") && !monitor["isDiscrete"].IsNull() && monitor["isDiscrete"].IsBool())
                newMonitor.isDiscrete = monitor["isDiscrete"].GetBool();

            if (monitor.HasMember("sliderMin") && !monitor["sliderMin"].IsNull() && monitor["sliderMin"].IsNumber())
                newMonitor.sliderMin = monitor["sliderMin"].GetDouble();

            if (monitor.HasMember("sliderMax") && !monitor["sliderMax"].IsNull() && monitor["sliderMax"].IsNumber())
                newMonitor.sliderMax = monitor["sliderMax"].GetDouble();

            Render::visibleVariables.push_back(newMonitor);
        }
    }

    // load block lookup table
    blockLookup.clear();
    for (Sprite *sprite : sprites) {
        for (auto &[id, block] : sprite->blocks) {
            blockLookup[id] = &block;
        }
    }

    // setup top level blocks
    for (Sprite *currentSprite : sprites) {
        for (auto &[id, block] : currentSprite->blocks) {
            if (block.topLevel) continue;
            block.topLevelParentBlock = getBlockParent(&block)->id;
        }
    }

    // try to find the advanced project settings comment
    rapidjson::Document config;
    for (Sprite *currentSprite : sprites) {
        if (!currentSprite->isStage) continue;
        for (auto &[id, comment] : currentSprite->comments) {
            std::size_t settingsFind = comment.text.find("Configuration for https");
            if (settingsFind == std::string::npos) continue;
            std::size_t json_start = comment.text.find('{');
            if (json_start == std::string::npos) continue;

            // Use brace counting to find the true end of the JSON
            int braceCount = 0;
            std::size_t json_end = json_start;
            bool in_string = false;

            for (; json_end < comment.text.size(); ++json_end) {
                char c = comment.text[json_end];

                if (c == '"' && (json_end == 0 || comment.text[json_end - 1] != '\\')) {
                    in_string = !in_string;
                }

                if (!in_string) {
                    if (c == '{') braceCount++;
                    else if (c == '}') braceCount--;

                    if (braceCount == 0) {
                        json_end++;
                        break;
                    }
                }
            }

            if (braceCount != 0) {
                continue;
            }

            std::string json_str = comment.text.substr(json_start, json_end - json_start);

            // Replace infinity with null, since the json cant handle infinity
            std::string cleaned_json = json_str;
            std::size_t inf_pos;
            while ((inf_pos = cleaned_json.find("Infinity")) != std::string::npos) {
                cleaned_json.replace(inf_pos, 8, "1e9");
            }

            if (!config.Parse(cleaned_json.c_str()).HasParseError()) {
                break;
            }
        }
    }

    // set advanced project settings properties
    int wdth = 0;
    int hght = 0;
    int framerate = 0;
    bool fncng = true;
    bool miscLimits = true;
    bool infClones = false;

    try {
        if (config.IsObject() && config.HasMember("framerate") && config["framerate"].IsInt()) {
            framerate = config["framerate"].GetInt();
            Scratch::FPS = framerate;
            Log::log("FPS = " + std::to_string(Scratch::FPS));
        }
    } catch (...) {
        Log::logWarning("no framerate property.");
    }

    try {
        if (config.IsObject() && config.HasMember("width") && config["width"].IsInt()) {
            wdth = config["width"].GetInt();
            Scratch::projectWidth = wdth;
            Log::log("game width = " + std::to_string(Scratch::projectWidth));
        }
    } catch (...) {
        Log::logWarning("no width property.");
    }

    try {
        if (config.IsObject() && config.HasMember("height") && config["height"].IsInt()) {
            hght = config["height"].GetInt();
            Scratch::projectHeight = hght;
            Log::log("game height = " + std::to_string(Scratch::projectHeight));
        }
    } catch (...) {
        Log::logWarning("no height property.");
    }

    try {
        if (config.IsObject() && config.HasMember("runtimeOptions") && config["runtimeOptions"].IsObject()) {
            const rapidjson::Value &runtimeOptions = config["runtimeOptions"];
            if (runtimeOptions.HasMember("fencing") && runtimeOptions["fencing"].IsBool()) {
                fncng = runtimeOptions["fencing"].GetBool();
                Scratch::fencing = fncng;
                Log::log(std::string("Fencing is ") + (Scratch::fencing ? "true" : "false"));
            }
        }
    } catch (...) {
        Log::logWarning("no fencing property.");
    }

    try {
        if (config.IsObject() && config.HasMember("runtimeOptions") && config["runtimeOptions"].IsObject()) {
            const rapidjson::Value &runtimeOptions = config["runtimeOptions"];
            if (runtimeOptions.HasMember("miscLimits") && runtimeOptions["miscLimits"].IsBool()) {
                miscLimits = runtimeOptions["miscLimits"].GetBool();
                Scratch::miscellaneousLimits = miscLimits;
                Log::log(std::string("Misc limits is ") + (Scratch::miscellaneousLimits ? "true" : "false"));
            }
        }
    } catch (...) {
        Log::logWarning("no misc limits property.");
    }

    try {
        if (config.IsObject() && config.HasMember("runtimeOptions") && config["runtimeOptions"].IsObject()) {
            const rapidjson::Value &runtimeOptions = config["runtimeOptions"];
            if (runtimeOptions.HasMember("maxClones")) {
                infClones = !runtimeOptions["maxClones"].IsNull();
            }
        }
    } catch (...) {
        Log::logWarning("No Max clones property.");
    }

    if (wdth == 400 && hght == 480)
        Render::renderMode = Render::BOTH_SCREENS;
    else if (wdth == 320 && hght == 240)
        Render::renderMode = Render::BOTTOM_SCREEN_ONLY;
    else
        Render::renderMode = Render::TOP_SCREEN_ONLY;

    // load initial sprite images
    Unzip::loadingState = "Loading images";
    int sprIndex = 1;
    if (projectType == UNZIPPED) {
        for (auto &currentSprite : sprites) {
            if (!currentSprite->visible || currentSprite->ghostEffect == 100) continue;
            Unzip::loadingState = "Loading image " + std::to_string(sprIndex) + " / " + std::to_string(sprites.size());
            Image::loadImageFromFile(currentSprite->costumes[currentSprite->currentCostume].fullName);
            sprIndex++;
        }
    } else {
        for (auto &currentSprite : sprites) {
            if (!currentSprite->visible || currentSprite->ghostEffect == 100) continue;
            Unzip::loadingState = "Loading image " + std::to_string(sprIndex) + " / " + std::to_string(sprites.size());
            Image::loadImageFromSB3(&Unzip::zipArchive, currentSprite->costumes[currentSprite->currentCostume].fullName);
            sprIndex++;
        }
    }

    // if infinite clones are enabled, set a (potentially) higher max clone count
    if (!infClones) initializeSpritePool(300);
    else {
        if (OS::getPlatform() == "3DS") {
            initializeSpritePool(OS::isNew3DS() ? 450 : 300);
        } else if (OS::getPlatform() == "Wii" || OS::getPlatform() == "Vita") {
            initializeSpritePool(450);
        } else if (OS::getPlatform() == "Wii U") {
            initializeSpritePool(800);
        } else if (OS::getPlatform() == "GameCube") {
            initializeSpritePool(300);
        } else if (OS::getPlatform() == "Switch") {
            initializeSpritePool(1500);
        } else if (OS::getPlatform() == "PC") {
            initializeSpritePool(2000);
        } else {
            Log::logWarning("Unknown platform: " + OS::getPlatform() + " doing default clone limit.");
            initializeSpritePool(300);
        }
    }

    // get block chains for every block
    for (Sprite *currentSprite : sprites) {
        for (auto &[id, block] : currentSprite->blocks) {
            if (!block.topLevel) continue;
            std::string outID;
            BlockChain chain;
            chain.blockChain = getBlockChain(block.id, &outID);
            currentSprite->blockChains[outID] = chain;
            block.blockChainID = outID;

            for (auto &chainBlock : chain.blockChain) {
                if (currentSprite->blocks.find(chainBlock->id) != currentSprite->blocks.end()) {
                    currentSprite->blocks[chainBlock->id].blockChainID = outID;
                }
            }
        }
    }

    Unzip::loadingState = "Running Flag block";

    Input::applyControls(OS::getScratchFolderLocation() + Unzip::filePath + ".json");
    Log::log("Loaded " + std::to_string(sprites.size()) + " sprites.");
}

Block *findBlock(std::string blockId) {

    auto block = blockLookup.find(blockId);
    if (block != blockLookup.end()) {
        return block->second;
    }

    return nullptr;
}

std::vector<Block *> getBlockChain(std::string blockId, std::string *outID) {
    std::vector<Block *> blockChain;
    Block *currentBlock = findBlock(blockId);
    while (currentBlock != nullptr) {
        blockChain.push_back(currentBlock);
        if (outID)
            *outID += currentBlock->id;

        auto substackIt = currentBlock->parsedInputs.find("SUBSTACK");
        if (substackIt != currentBlock->parsedInputs.end() &&
            (substackIt->second.inputType == ParsedInput::BOOLEAN || substackIt->second.inputType == ParsedInput::BLOCK) &&
            !substackIt->second.blockId.empty()) {

            std::vector<Block *> subBlockChain;
            subBlockChain = getBlockChain(substackIt->second.blockId, outID);
            for (auto &block : subBlockChain) {
                blockChain.push_back(block);
                if (outID)
                    *outID += block->id;
            }
        }

        auto substack2It = currentBlock->parsedInputs.find("SUBSTACK2");
        if (substack2It != currentBlock->parsedInputs.end() &&
            (substack2It->second.inputType == ParsedInput::BOOLEAN || substack2It->second.inputType == ParsedInput::BLOCK) &&
            !substack2It->second.blockId.empty()) {

            std::vector<Block *> subBlockChain;
            subBlockChain = getBlockChain(substack2It->second.blockId, outID);
            for (auto &block : subBlockChain) {
                blockChain.push_back(block);
                if (outID)
                    *outID += block->id;
            }
        }
        currentBlock = findBlock(currentBlock->next);
    }
    return blockChain;
}

Block *getBlockParent(const Block *block) {
    Block *parentBlock;
    const Block *currentBlock = block;
    while (currentBlock->parent != "null") {
        parentBlock = findBlock(currentBlock->parent);
        if (parentBlock != nullptr) {
            currentBlock = parentBlock;
        } else {
            break;
        }
    }
    return const_cast<Block *>(currentBlock);
}

Value Scratch::getInputValue(Block &block, const std::string &inputName, Sprite *sprite) {
    auto parsedFind = block.parsedInputs.find(inputName);

    if (parsedFind == block.parsedInputs.end()) {
        return Value();
    }

    const ParsedInput &input = parsedFind->second;
    switch (input.inputType) {

    case ParsedInput::LITERAL:
        return input.literalValue;

    case ParsedInput::VARIABLE:
        return BlockExecutor::getVariableValue(input.variableId, sprite);

    case ParsedInput::BLOCK:
        return executor.getBlockValue(*findBlock(input.blockId), sprite);

    case ParsedInput::BOOLEAN:
        return executor.getBlockValue(*findBlock(input.blockId), sprite);
    }
    return Value();
}

std::string Scratch::getFieldValue(Block &block, const std::string &fieldName) {
    auto fieldFind = block.parsedFields.find(fieldName);
    if (fieldFind == block.parsedFields.end()) {
        return "";
    }
    return fieldFind->second.value;
}

std::string Scratch::getFieldId(Block &block, const std::string &fieldName) {
    auto fieldFind = block.parsedFields.find(fieldName);
    if (fieldFind == block.parsedFields.end()) {
        return "";
    }
    return fieldFind->second.id;
}
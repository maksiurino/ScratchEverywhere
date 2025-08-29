#include "pen.hpp"

#ifdef __3DS__
C2D_Image penImage;
C3D_RenderTarget *penRenderTarget;
#elif defined(SDL_BUILD)
SDL_Texture *penTexture;
#else
#error Unsupported Platform.
#endif

const unsigned int minPenSize = 1;
const unsigned int maxPenSize = 1000;

BlockResult PenBlocks::PenDown(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    sprite->penData.down = true;

    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::PenUp(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    sprite->penData.down = false;

    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::SetPenOptionTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    const std::string option = Scratch::getInputValue(block, "COLOR_PARAM", sprite).asString();

    if (option == "color") {
        sprite->penData.color.hue = Scratch::getInputValue(block, "VALUE", sprite).asInt() % 100;
        return BlockResult::CONTINUE;
    }
    if (option == "saturation") {
        sprite->penData.color.saturation = Scratch::getInputValue(block, "VALUE", sprite).asDouble();
        if (sprite->penData.color.saturation < 0) sprite->penData.color.saturation = 0;
        else if (sprite->penData.color.saturation > 100) sprite->penData.color.saturation = 100;
        return BlockResult::CONTINUE;
    }
    if (option == "brightness") {
        sprite->penData.color.brightness = Scratch::getInputValue(block, "VALUE", sprite).asDouble();
        if (sprite->penData.color.brightness < 0) sprite->penData.color.brightness = 0;
        else if (sprite->penData.color.brightness > 100) sprite->penData.color.brightness = 100;
        return BlockResult::CONTINUE;
    }
    if (option == "transparency") {
        sprite->penData.transparency = Scratch::getInputValue(block, "VALUE", sprite).asDouble();
        if (sprite->penData.transparency < 0) sprite->penData.transparency = 0;
        else if (sprite->penData.transparency > 100) sprite->penData.transparency = 100;
        return BlockResult::CONTINUE;
    }

    Log::log("Unknown pen option: " + option);

    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::ChangePenOptionBy(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    const std::string option = Scratch::getInputValue(block, "COLOR_PARAM", sprite).asString();

    if (option == "color") {
        sprite->penData.color.hue += Scratch::getInputValue(block, "VALUE", sprite).asInt() % 100;
        return BlockResult::CONTINUE;
    }
    if (option == "saturation") {
        sprite->penData.color.saturation += Scratch::getInputValue(block, "VALUE", sprite).asDouble();
        if (sprite->penData.color.saturation < 0) sprite->penData.color.saturation = 0;
        else if (sprite->penData.color.saturation > 100) sprite->penData.color.saturation = 100;
        return BlockResult::CONTINUE;
    }
    if (option == "brightness") {
        sprite->penData.color.brightness += Scratch::getInputValue(block, "VALUE", sprite).asDouble();
        if (sprite->penData.color.brightness < 0) sprite->penData.color.brightness = 0;
        else if (sprite->penData.color.brightness > 100) sprite->penData.color.brightness = 100;
        return BlockResult::CONTINUE;
    }
    if (option == "transparency") {
        sprite->penData.transparency += Scratch::getInputValue(block, "VALUE", sprite).asDouble();
        if (sprite->penData.transparency < 0) sprite->penData.transparency = 0;
        else if (sprite->penData.transparency > 100) sprite->penData.transparency = 100;
        return BlockResult::CONTINUE;
    }

    Log::log("Unknown pen option: " + option);

    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::SetPenColorTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    sprite->penData.color = Scratch::getInputValue(block, "COLOR", sprite).asColor();

    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::SetPenSizeTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    sprite->penData.size = Scratch::getInputValue(block, "SIZE", sprite).asDouble();
    if (sprite->penData.size < minPenSize) sprite->penData.size = minPenSize;
    else if (sprite->penData.size > maxPenSize) sprite->penData.size = maxPenSize;

    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::ChangePenSizeBy(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    sprite->penData.size += Scratch::getInputValue(block, "SIZE", sprite).asDouble();
    if (sprite->penData.size < minPenSize) sprite->penData.size = minPenSize;
    else if (sprite->penData.size > maxPenSize) sprite->penData.size = maxPenSize;

    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::EraseAll(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::Stamp(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    return BlockResult::CONTINUE;
}

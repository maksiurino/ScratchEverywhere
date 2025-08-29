#include "pen.hpp"

#ifdef __3DS__
C2D_Image penImage;
C3D_RenderTarget *penRenderTarget;
#elif defined(SDL_BUILD)
SDL_Texture *penTexture;
#else
#error Unsupported Platform.
#endif

BlockResult PenBlocks::PenDown(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    sprite->penData.down = true;

    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::PenUp(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    sprite->penData.down = false;

    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::SetPenOptionTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::ChangePenOptionBy(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::SetPenColorTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    sprite->penData.color = Scratch::getInputValue(block, "COLOR", sprite).asColor();
    Log::log("Set pen color to: " + Value(sprite->penData.color).asString());

    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::SetPenSizeTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    sprite->penData.size = Scratch::getInputValue(block, "SIZE", sprite).asDouble();

    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::ChangePenSizeBy(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    sprite->penData.size += Scratch::getInputValue(block, "SIZE", sprite).asDouble();

    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::EraseAll(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::Stamp(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    return BlockResult::CONTINUE;
}

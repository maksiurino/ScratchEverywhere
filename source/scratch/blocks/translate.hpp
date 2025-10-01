#pragma once
#include "sprite.hpp"
#include "value.hpp"

class TranslateBlocks {
  public:
    static Value getTranslate(Block &block, Sprite *sprite);
    static Value getViewerLanguage(Block &block, Sprite *sprite);
}

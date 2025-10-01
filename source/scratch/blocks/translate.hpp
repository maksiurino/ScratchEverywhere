#pragma once
#include "sprite.hpp"
#include "value.hpp"

class TranslateBlocks {
  public:
    static Value getTranslate(Block &block, Sprite *sprite);
    static Value menu_languages(Block &block, Sprite *sprite);
}

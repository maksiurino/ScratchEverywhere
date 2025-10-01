#pragma once
#include "sprite.hpp"
#include "value.hpp"

class TranslateBlocks {
  public:
    static Value translate(Block &block, Sprite *sprite);
    static Value language(Block &block, Sprite *sprite);
}

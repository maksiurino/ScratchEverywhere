#include "translate.hpp"
#include "interpret.hpp"
#include "sprite.hpp"
#include "value.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <math.h>

Value TranslateBlocks::getTranslate(Block &block, Sprite *sprite) {
  Value value1 = Scratch::getInputValue(block, "WORDS", sprite);
  Value value2 = Scratch::getInputValue(block, "LANGUAGE", sprite);
}

// What the port's menus (pause_menu, title_menu) share about input: stylus taps and the selected
// button's bracket bounce.
#pragma once

#include "platform/platform.h"

// A stylus tap on a menu's items: a press and a release on the same item (the 3DS reports no
// position on release, so the last touched one counts). Items are ids >= 0 found by position.
class MenuTap {
public:
    struct Frame {
        int down = -1;   // the item touched down on this frame
        int tapped = -1; // the item tapped (released on) this frame
    };

    // One frame of input; `itemAt(x, y)` (UI px, the screen's px / `scale`) -> item id, -1 = none.
    template <class ItemAt>
    Frame update(const plat::Input& in, int scale, ItemAt itemAt) {
        Frame f;
        if (in.touching) {
            x_ = in.touchX / scale;
            y_ = in.touchY / scale;
            if (!wasTouching_) f.down = pressed_ = itemAt(x_, y_);
            over_ = itemAt(x_, y_);
        } else if (wasTouching_) {
            if (pressed_ >= 0 && over_ == pressed_) f.tapped = pressed_;
            pressed_ = over_ = -1;
        }
        wasTouching_ = in.touching;
        return f;
    }
    // Ignores the touch held now (a menu opening or closing under the stylus): it's no tap.
    void release() {
        wasTouching_ = true;
        pressed_ = over_ = -1;
    }
    // Drops the item touched down on (its page changed): releasing won't tap it.
    void cancel() { pressed_ = over_ = -1; }
    // `item` is held down: touched down on and the stylus still over it (drawn pressed).
    bool held(int item) const { return wasTouching_ && pressed_ == item && over_ == item; }
    int pressed() const { return pressed_; } // touched down on (still touching), -1 = none

private:
    bool wasTouching_ = false;
    int pressed_ = -1, over_ = -1;
    int x_ = 0, y_ = 0;
};

// The selected button's brackets bounce: at rest, then 1 px out, each half a second, restarting
// whenever the selection changes. Counted in frames (the game steps a fixed 60 fps).
class BracketBounce {
public:
    static constexpr int FRAMES = 60;
    // One frame with `selection` selected (any id for what's selected: page and item).
    void update(int selection) {
        frame_ = selection == selection_ ? (frame_ + 1) % FRAMES : 0;
        selection_ = selection;
    }
    int out() const { return frame_ >= FRAMES / 2 ? 1 : 0; } // px the brackets stand out now

private:
    int selection_ = -1, frame_ = 0;
};

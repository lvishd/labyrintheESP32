/*
 * sim_display.h — écran simulé 200 x 200 sur PC (tests uniquement).
 */
#ifndef SIM_DISPLAY_H
#define SIM_DISPLAY_H

#include "display_iface.h"

namespace sim {

class SimDisplay : public maze::Display {
public:
    static constexpr int WIDTH = 200;
    static constexpr int HEIGHT = 200;

    SimDisplay();

    void clear(maze::Color c) override;
    void fillRect(int x, int y, int w, int h, maze::Color c) override;
    void flush() override;

    maze::Color pixel(int x, int y) const;
    int overflowCount() const { return overflow_; }
    int flushCount() const { return flush_; }
    void resetCounters() { overflow_ = 0; flush_ = 0; }

    int countColor(maze::Color c) const;
    bool savePng(const char *path, int scale = 4) const;

private:
    uint8_t pixels_[WIDTH * HEIGHT];
    int overflow_;
    int flush_;
};

} // namespace sim

#endif // SIM_DISPLAY_H

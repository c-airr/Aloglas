#include <doctest.h>

#include "app/controller.h"
#include "app/transport_bar.h"
#include "support/fake_host.h"
#include "support/recording_sink.h"

using namespace aloglas;
using namespace aloglas::test;

namespace {
Point center(const Widget& w) {
    return {w.rect.x + w.rect.w / 2, w.rect.y + w.rect.h / 2};
}

struct Fixture {
    Document doc;
    RecordingSink sink;
    Controller controller{doc, sink};
    LiveState live;
    TransportBar bar{controller, live};
    EventRouter router{bar};
    FakeHost host{router};

    explicit Fixture(float width = 1280) {
        controller.ensure_audio = [] { return true; };
        bar.set_host(&host);
        bar.layout({0, 0, width, 28});
    }
};
}  // namespace

TEST_CASE("transport buttons drive the controller") {
    Fixture f;
    click(f.router, center(f.bar.play()));
    REQUIRE_FALSE(f.sink.sent.empty());
    CHECK(f.sink.sent.back().type == CommandType::Play);
    f.live.playing = true;
    click(f.router, center(f.bar.play()));
    CHECK(f.sink.sent.back().type == CommandType::Stop);
    f.sink.sent.clear();
    click(f.router, center(f.bar.stop()));
    CHECK(f.sink.sent.back().type == CommandType::Stop);

    click(f.router, center(f.bar.song_mode()));
    CHECK(f.doc.play_mode == PlayMode::Song);
    click(f.router, center(f.bar.loop_mode()));
    CHECK(f.doc.play_mode == PlayMode::Loop);
    click(f.router, center(f.bar.loop_mode()));  // clicking the active mode keeps it
    CHECK(f.doc.play_mode == PlayMode::Loop);

    click(f.router, center(f.bar.metronome()));
    CHECK_FALSE(f.doc.metronome);
    click(f.router, center(f.bar.brace()));
    CHECK(f.doc.brace.enabled);
    click(f.router, center(f.bar.follow()));
    CHECK_FALSE(f.doc.follow);
}

TEST_CASE("tempo drag and signature boxes") {
    Fixture f;
    const Point t = center(f.bar.tempo());
    drag(f.router, t, {t.x, t.y - 20});
    CHECK(f.doc.tempo == doctest::Approx(150));

    const Point den = center(f.bar.sig_den());
    drag(f.router, den, {den.x, den.y - 8});
    CHECK(f.doc.signature == TimeSignature{4, 8});

    const Point num = center(f.bar.sig_num());
    drag(f.router, num, {num.x, num.y - 16});
    CHECK(f.doc.signature == TimeSignature{6, 8});
}

TEST_CASE("transport layout fits 960 px") {
    Fixture f(960);
    float meter_x = 960;
    for (const Widget* w : f.bar.children()) {
        CHECK(w->rect.x >= 0);
        CHECK(w->rect.right() <= 960);
        CHECK(w->rect.y >= 0);
        CHECK(w->rect.bottom() <= 28);
    }
    meter_x = f.bar.cpu_label().rect.x;
    CHECK(f.bar.position().rect.right() < meter_x);
}

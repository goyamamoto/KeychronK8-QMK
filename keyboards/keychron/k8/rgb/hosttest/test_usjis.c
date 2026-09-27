// Host tests for the K8 US-JIS substitution (../usjis.c), ported from the
// NuPhy Air60 V2 tests.
//
// The driver below plays the part of QMK's process_record: it calls
// usjis_process_record, applies the pass-through effect QMK would have
// (modifier keys change the real mods, plain keys are registered with any
// folded-in modifiers as weak mods), then calls usjis_post_process_record.
// Reports are recorded by the stub in stubs.c. Expected values follow the
// zmk-kb1-usjis specification (C01-C20, S01-S06, S11-S14).

#include <stdio.h>
#include <string.h>
#include "quantum.h"
#include "usjis.h"

// The keyboard side of usjis.h: the OS switch and the save-and-show hook.
static bool host_is_win         = true;
static int  mode_applied_count  = 0; // stands in for EEPROM writes
static bool mode_applied_notice = false;

bool usjis_host_is_win(void) {
    return host_is_win;
}
void usjis_mode_applied(bool enabled) {
    mode_applied_count++;
    mode_applied_notice = true;
}

static int failures = 0, checks = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        checks++;                                                                \
        if (!(cond)) {                                                           \
            failures++;                                                          \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);             \
        }                                                                        \
    } while (0)
#define SECTION(name) printf("== %s\n", name)

#define SHIFT_L MOD_BIT(KC_LSFT)
#define SHIFT_R MOD_BIT(KC_RSFT)

static void key_event(uint16_t keycode, bool pressed) {
    keyrecord_t rec = {0};
    rec.event.pressed = pressed;
    bool pass         = usjis_process_record(keycode, &rec);
    if (pass) {
        if (keycode >= KC_LCTL && keycode <= KC_RGUI) {
            if (pressed) add_mods(MOD_BIT(keycode)); else del_mods(MOD_BIT(keycode));
            send_keyboard_report();
        } else {
            uint8_t basic = QK_MODS_GET_BASIC_KEYCODE(keycode);
            uint8_t mods  = IS_QK_MODS(keycode) ? QK_MODS_GET_MODS(keycode) : 0;
            if (pressed) {
                if (mods) add_weak_mods(mods);
                register_code(basic);
            } else {
                unregister_code(basic);
                if (mods) del_weak_mods(mods);
            }
        }
    }
    usjis_post_process_record(keycode, &rec);
}
static void press(uint16_t kc) { key_event(kc, true); }
static void release(uint16_t kc) { key_event(kc, false); }

static const report_snapshot_t *last(void) { return &report_log[report_log_count - 1]; }
static int last_has_key(uint8_t kc) {
    for (int i = 0; i < 6; i++) if (last()->keys[i] == kc) return 1;
    return 0;
}
static int last_key_count(void) {
    int n = 0;
    for (int i = 0; i < 6; i++) if (last()->keys[i]) n++;
    return n;
}
// sequence of distinct report modifier values, consecutive duplicates removed
static int mods_sequence(uint8_t *out, int max) {
    int n = 0;
    for (uint32_t i = 0; i < report_log_count; i++) {
        uint8_t m = report_log[i].mods;
        if (n == 0 || out[n - 1] != m) { if (n < max) out[n++] = m; }
    }
    return n;
}

static void reset(bool enabled, bool win) {
    report_model_reset();
    usjis_clear();
    usjis_init(enabled);
    host_is_win        = win;
    mode_applied_count = 0;
}

typedef struct { uint8_t in; bool shift; uint8_t out; bool oshift; const char *id; } rule_t;
static const rule_t table[] = {
    {KC_GRV, 1, KC_EQL, 1, "C01"},  {KC_2, 1, KC_LBRC, 0, "C02"},   {KC_6, 1, KC_EQL, 0, "C03"},    {KC_7, 1, KC_6, 1, "C04"},
    {KC_8, 1, KC_QUOT, 1, "C05"},   {KC_9, 1, KC_8, 1, "C06"},      {KC_0, 1, KC_9, 1, "C07"},      {KC_MINS, 1, KC_INT1, 1, "C08"},
    {KC_EQL, 0, KC_MINS, 1, "C09"}, {KC_EQL, 1, KC_SCLN, 1, "C10"}, {KC_LBRC, 0, KC_RBRC, 0, "C11"}, {KC_LBRC, 1, KC_RBRC, 1, "C12"},
    {KC_RBRC, 0, KC_NUHS, 0, "C13"}, {KC_RBRC, 1, KC_NUHS, 1, "C14"}, {KC_BSLS, 0, KC_INT1, 0, "C15"}, {KC_BSLS, 1, KC_INT3, 1, "C16"},
    {KC_SCLN, 1, KC_QUOT, 0, "C17"}, {KC_QUOT, 0, KC_7, 1, "C18"},  {KC_QUOT, 1, KC_2, 1, "C19"},   {KC_GRV, 0, KC_LBRC, 1, "C20"},
};

static void test_table(void) {
    SECTION("C01-C20 with left and right Shift; release uses the press decision");
    for (int s = 0; s < 2; s++) {
        uint16_t shift_key = s ? KC_RSFT : KC_LSFT;
        for (unsigned i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
            const rule_t *r = &table[i];
            reset(true, true);
            if (r->shift) press(shift_key);
            press(r->in);
            if (!(last_has_key(r->out) && last_key_count() == 1 && last()->mods == (r->oshift ? SHIFT_L : 0))) {
                failures++; checks++;
                printf("  FAIL %s (%s shift): got mods %02X keys %02X\n", r->id, s ? "right" : "left", last()->mods, last()->keys[0]);
            } else {
                checks++;
            }
            release(r->in);
            CHECK(last_key_count() == 0);
            CHECK(last()->mods == (r->shift ? MOD_BIT(shift_key) : 0)); // physical Shift back
            if (r->shift) release(shift_key);
            CHECK(last()->mods == 0);
        }
    }
    // unmatched Shift condition is untouched (identity)
    reset(true, true);
    press(KC_2);
    CHECK(last_has_key(KC_2) && last()->mods == 0);
    release(KC_2);
    press(KC_LSFT);
    press(KC_A);
    CHECK(last_has_key(KC_A) && last()->mods == SHIFT_L);
    release(KC_A);
    release(KC_LSFT);
    CHECK(last_key_count() == 0 && last()->mods == 0);
    // keycode with folded-in Shift (VIA: S(KC_2)) is C02 too
    reset(true, true);
    press(LSFT(KC_2));
    CHECK(last_has_key(KC_LBRC) && last()->mods == 0);
    release(LSFT(KC_2));
    CHECK(last_key_count() == 0 && last()->mods == 0);
}

static void test_identity_when_off(void) {
    SECTION("S01: disabled, and Mac mode, are identity");
    for (int mode = 0; mode < 2; mode++) {
        reset(mode == 0 ? false : true, mode == 0 ? true : false);
        for (unsigned i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
            const rule_t *r = &table[i];
            if (r->shift) press(KC_RSFT);
            press(r->in);
            CHECK(last_has_key(r->in) && last()->mods == (r->shift ? SHIFT_R : 0));
            release(r->in);
            if (r->shift) release(KC_RSFT);
        }
        CHECK(last_key_count() == 0 && last()->mods == 0);
    }
}

static void test_s02_s03(void) {
    SECTION("S02/S03: Shift released before or after the substituted key");
    uint8_t seq[16];
    reset(true, true);
    press(KC_RSFT); press(KC_2); release(KC_2); release(KC_RSFT);
    int n = mods_sequence(seq, 16);
    CHECK(n == 4 && seq[0] == 0x20 && seq[1] == 0x00 && seq[2] == 0x20 && seq[3] == 0x00);
    CHECK(last_key_count() == 0);

    reset(true, true);
    press(KC_RSFT); press(KC_2);
    CHECK(last_has_key(KC_LBRC) && last()->mods == 0);
    release(KC_RSFT);
    CHECK(last_has_key(KC_LBRC) && last()->mods == 0); // still @ while held
    release(KC_2);
    CHECK(last_key_count() == 0 && last()->mods == 0);
}

static void test_s04_pending(void) {
    SECTION("S04: mode change while a key is held is applied after release");
    reset(true, true);
    press(KC_EQL);
    CHECK(last_has_key(KC_MINS) && last()->mods == SHIFT_L);
    usjis_toggle();
    CHECK(usjis_is_enabled());
    CHECK(mode_applied_count == 0);
    release(KC_EQL);
    CHECK(!usjis_is_enabled());
    CHECK(mode_applied_count == 1);
    CHECK(last_key_count() == 0 && last()->mods == 0);
    // two toggles cancel; no write
    press(KC_A);
    usjis_toggle(); usjis_toggle();
    release(KC_A);
    CHECK(!usjis_is_enabled() && mode_applied_count == 1);
    // toggling with nothing held applies at once, and blinks the notice
    mode_applied_notice = false;
    usjis_toggle();
    CHECK(usjis_is_enabled() && mode_applied_count == 2 && mode_applied_notice);
    // USJIS_ON / USJIS_OFF: absolute requests, no-ops when already there
    reset(false, true);
    usjis_request(true);
    CHECK(usjis_is_enabled() && mode_applied_count == 1);
    usjis_request(true);
    CHECK(usjis_is_enabled() && mode_applied_count == 1);
    usjis_request(false);
    CHECK(!usjis_is_enabled() && mode_applied_count == 2);
    usjis_toggle();
    CHECK(usjis_is_enabled() && mode_applied_count == 3);
    press(KC_A);
    usjis_request(false);   // postponed while A is held
    CHECK(usjis_is_enabled());
    release(KC_A);
    CHECK(!usjis_is_enabled() && mode_applied_count == 4);
    // the mode read at boot is not saved again
    reset(true, true);
    CHECK(usjis_is_enabled() && mode_applied_count == 0);
    // an unsubstituted key held while disabled also postpones
    reset(false, true);
    press(KC_A);
    usjis_request(true);
    CHECK(!usjis_is_enabled());
    release(KC_A);
    CHECK(usjis_is_enabled());
}

static void test_s05_two_substituted(void) {
    SECTION("S05: two substituted keys, reverse release, nothing stuck");
    uint8_t seq[16];
    reset(true, true);
    press(KC_RSFT); press(KC_2); press(KC_MINS);
    CHECK(last_has_key(KC_LBRC) && last_has_key(KC_INT1) && last()->mods == SHIFT_L);
    release(KC_MINS);
    CHECK(last_has_key(KC_LBRC) && !last_has_key(KC_INT1) && last()->mods == 0);
    release(KC_2);
    CHECK(last_key_count() == 0 && last()->mods == SHIFT_R);
    release(KC_RSFT);
    CHECK(last()->mods == 0);
    int n = mods_sequence(seq, 16);
    CHECK(n == 6 && seq[0] == 0x20 && seq[1] == 0x00 && seq[2] == 0x02 && seq[3] == 0x00 && seq[4] == 0x20 && seq[5] == 0x00);
}

static void test_conflicts(void) {
    SECTION("S11-S14: Shift follows the most recently pressed key");
    // S11: Shift+2 (@) held, then A: A gets the physical Shift, @ stays down
    reset(true, true);
    press(KC_LSFT); press(KC_2);
    press(KC_A);
    CHECK(last_has_key(KC_LBRC) && last_has_key(KC_A) && last()->mods == SHIFT_L);
    release(KC_A);
    CHECK(last_has_key(KC_LBRC) && last()->mods == 0);
    release(KC_2); release(KC_LSFT);
    CHECK(last_key_count() == 0 && last()->mods == 0);

    // S12: = (needs Shift) held, then a: Shift drops for a, returns after
    reset(true, true);
    press(KC_EQL);
    press(KC_A);
    CHECK(last_has_key(KC_MINS) && last_has_key(KC_A) && last()->mods == 0);
    release(KC_A);
    CHECK(last_has_key(KC_MINS) && last()->mods == SHIFT_L);
    release(KC_EQL);
    CHECK(last_key_count() == 0 && last()->mods == 0);

    // S13: = held, then Shift and 2 (@): the newer key wins
    reset(true, true);
    press(KC_EQL); press(KC_LSFT); press(KC_2);
    CHECK(last_has_key(KC_MINS) && last_has_key(KC_LBRC) && last()->mods == 0);
    release(KC_2);
    CHECK(last()->mods == SHIFT_L);
    release(KC_LSFT);
    CHECK(last_has_key(KC_MINS) && last()->mods == SHIFT_L);
    release(KC_EQL);
    CHECK(last_key_count() == 0 && last()->mods == 0);

    // S14: Shift+- (_) held, Shift released first: the added Shift stays
    reset(true, true);
    press(KC_RSFT); press(KC_MINS);
    CHECK(last_has_key(KC_INT1) && last()->mods == SHIFT_L);
    release(KC_RSFT);
    CHECK(last_has_key(KC_INT1) && last()->mods == SHIFT_L);
    press(KC_RSFT); // pressing Shift again re-applies the same request
    CHECK(last()->mods == SHIFT_L);
    release(KC_RSFT);
    release(KC_MINS);
    CHECK(last_key_count() == 0 && last()->mods == 0);

    // Ctrl is kept through a substitution
    reset(true, true);
    press(KC_LCTL); press(KC_LBRC);
    CHECK(last_has_key(KC_RBRC) && last()->mods == MOD_BIT(KC_LCTL));
    release(KC_LBRC); release(KC_LCTL);
    CHECK(last_key_count() == 0 && last()->mods == 0);
}

// Exact report sequences from the spec's S tests. Every report counts: the
// host decides a character from the modifiers in the report that carries the
// key down, so a wrong intermediate report types a wrong character even when
// the final state is right. Physical Shift is Right Shift (0x20) so that it
// differs from the Left Shift (0x02) the substitution adds.
typedef struct { uint8_t mods; uint8_t keys[3]; } expected_t;

static bool report_matches(const report_snapshot_t *r, const expected_t *e) {
    if (r->mods != e->mods) return false;
    int want = 0, got = 0;
    for (int i = 0; i < 3; i++) {
        if (!e->keys[i]) continue;
        want++;
        bool found = false;
        for (int j = 0; j < 6; j++) if (r->keys[j] == e->keys[i]) found = true;
        if (!found) return false;
    }
    for (int j = 0; j < 6; j++) if (r->keys[j]) got++;
    return got == want;
}

static void expect_reports(const char *id, const expected_t *exp, uint32_t n) {
    bool ok = report_log_count == n;
    for (uint32_t i = 0; ok && i < n; i++) ok = report_matches(&report_log[i], &exp[i]);
    checks++;
    if (ok) return;
    failures++;
    printf("  FAIL %s: expected %u reports, got %u:", id, (unsigned)n, (unsigned)report_log_count);
    for (uint32_t i = 0; i < report_log_count; i++) {
        printf(" {%02X,", report_log[i].mods);
        for (int j = 0; j < 6; j++) if (report_log[i].keys[j]) printf("%02X ", report_log[i].keys[j]);
        printf("}");
    }
    printf("\n");
}

static void test_exact_reports(void) {
    SECTION("S02, S03, S05, S11-S14: every report, in order");

    reset(true, true);
    press(KC_RSFT); press(KC_2); release(KC_2); release(KC_RSFT);
    expect_reports("S02", (expected_t[]){{0x20, {0}}, {0x00, {KC_LBRC}}, {0x20, {0}}, {0x00, {0}}}, 4);

    reset(true, true);
    press(KC_RSFT); press(KC_2); release(KC_RSFT); release(KC_2);
    expect_reports("S03", (expected_t[]){{0x20, {0}}, {0x00, {KC_LBRC}}, {0x00, {KC_LBRC}}, {0x00, {0}}}, 4);

    reset(true, true);
    press(KC_RSFT); press(KC_2); press(KC_MINS); release(KC_MINS); release(KC_2); release(KC_RSFT);
    expect_reports("S05", (expected_t[]){{0x20, {0}}, {0x00, {KC_LBRC}}, {0x02, {KC_LBRC, KC_INT1}}, {0x00, {KC_LBRC}}, {0x20, {0}}, {0x00, {0}}}, 6);

    // @ held, then A: A goes down with the physical Shift, so it types "A"
    reset(true, true);
    press(KC_RSFT); press(KC_2); press(KC_A); release(KC_A); release(KC_2); release(KC_RSFT);
    expect_reports("S11", (expected_t[]){{0x20, {0}}, {0x00, {KC_LBRC}}, {0x20, {KC_LBRC, KC_A}}, {0x00, {KC_LBRC}}, {0x20, {0}}, {0x00, {0}}}, 6);

    // = held, then A: A goes down without the added Shift, so it types "a"
    reset(true, true);
    press(KC_EQL); press(KC_A); release(KC_A); release(KC_EQL);
    expect_reports("S12", (expected_t[]){{0x02, {KC_MINS}}, {0x00, {KC_MINS, KC_A}}, {0x02, {KC_MINS}}, {0x00, {0}}}, 4);

    reset(true, true);
    press(KC_EQL); press(KC_RSFT); press(KC_2); release(KC_2); release(KC_RSFT); release(KC_EQL);
    expect_reports("S13", (expected_t[]){{0x02, {KC_MINS}}, {0x02, {KC_MINS}}, {0x00, {KC_MINS, KC_LBRC}}, {0x02, {KC_MINS}}, {0x02, {KC_MINS}}, {0x00, {0}}}, 6);

    reset(true, true);
    press(KC_RSFT); press(KC_MINS); release(KC_RSFT); release(KC_MINS);
    expect_reports("S14", (expected_t[]){{0x20, {0}}, {0x02, {KC_INT1}}, {0x02, {KC_INT1}}, {0x00, {0}}}, 4);
}

static void test_clear(void) {
    SECTION("clear while keys are held forgets them and applies a pending mode");
    reset(true, true);
    press(KC_RSFT); press(KC_2);
    usjis_toggle();
    usjis_clear();
    CHECK(!usjis_is_enabled());
    report_model_reset();
    release(KC_2); // unknown key now: passes through as a plain 2 release
    CHECK(last_key_count() == 0);
}

int main(void) {
    test_table();
    test_identity_when_off();
    test_s02_s03();
    test_s04_pending();
    test_s05_two_substituted();
    test_conflicts();
    test_exact_reports();
    test_clear();
    printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}

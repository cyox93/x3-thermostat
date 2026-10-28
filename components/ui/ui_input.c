#include "ui_priv.h"
#include "theme.h"

void ui_input_apply(ui_input_t in)
{
    app_result_t r;
    switch (in) {
    case UI_IN_ENC_CW:  r = app_input_enc(+1); break;
    case UI_IN_ENC_CCW: r = app_input_enc(-1); break;
    case UI_IN_SHORT:   r = app_input_short(); break;
    case UI_IN_LONG:    r = app_input_long(); break;
    case UI_IN_ROOM:    r = app_input_room(); break;
    default: return;
    }
    /* Test runs turn animations off, which also drops the flash — like the
       HTML simulator's ?test=1 — so snapshots never catch it half-faded */
    if (r.flash && ui_screens_anim()) {
        flash_show(r.flash, r.accent == APP_ACCENT_BOILER ? TH_ACCENT : TH_SETTING);
    }
    /* r.heat_changed / r.settings_commit reach sysloop and NVS in stage 3 */
}
